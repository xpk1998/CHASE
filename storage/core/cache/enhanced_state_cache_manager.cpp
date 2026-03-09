//
// Enhanced State Cache Manager Implementation
// Improved version of StateCacheManager with better synchronization mechanisms
//

#include "enhanced_state_cache_manager.h"
#include "../state/key_page_aggregator.h"
#include "glog/logging.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <optional>

namespace blp {

EnhancedStateCacheManager::EnhancedStateCacheManager(std::shared_ptr<VersionedDB> committed_db,
                                                   size_t read_cache_capacity,
                                                   size_t l1_cache_capacity)
    : committed_db_(committed_db),
      read_cache_capacity_(read_cache_capacity),
      l1_read_count_(0),
      l2_read_count_(0),
      l1_hit_count_(0),
      l2_hit_count_(0),
      write_count_(0),
      cache_eviction_count_(0),
      background_flush_enabled_(false),
      stop_background_thread_(false) {
    
    LOG(INFO) << "[ENHANCED_CACHE] Initializing EnhancedStateCacheManager with "
              << "read_cache_capacity=" << read_cache_capacity_
              << ", l1_cache_capacity=" << l1_cache_capacity;
    
    // 初始化KeyPage聚合器
    nc_zone_aggregator_ = std::make_unique<KeyPageAggregator>();
    c_zone_aggregator_ = std::make_unique<KeyPageAggregator>();
    
    // 启动后台刷新线程
    background_flush_enabled_ = true;
    background_thread_ = std::thread(&EnhancedStateCacheManager::backgroundFlushThread, this);
}

EnhancedStateCacheManager::~EnhancedStateCacheManager() {
    // 停止后台线程
    stop_background_thread_ = true;
    flush_cv_.notify_all();
    
    if (background_thread_.joinable()) {
        background_thread_.join();
    }
}

void EnhancedStateCacheManager::initializeZoneCaches(ZoneType zone_type) {
    LOG(INFO) << "[ENHANCED_CACHE] Initializing zone cache for " 
              << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    if (zone_type == ZoneType::NC_ZONE && !nc_zone_cache_) {
        nc_zone_cache_ = std::make_unique<UncommittedStateCache>(0);
        LOG(INFO) << "[ENHANCED_CACHE] NC zone cache initialized";
    } else if (zone_type == ZoneType::C_ZONE && !c_zone_cache_) {
        c_zone_cache_ = std::make_unique<UncommittedStateCache>(0);
        LOG(INFO) << "[ENHANCED_CACHE] C zone cache initialized";
    }
}

UncommittedStateCache* EnhancedStateCacheManager::getZoneCache(ZoneType zone_type) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    if (zone_type == ZoneType::NC_ZONE) {
        return nc_zone_cache_.get();
    } else if (zone_type == ZoneType::C_ZONE) {
        return c_zone_cache_.get();
    }
    
    return nullptr;
}

std::string EnhancedStateCacheManager::get(ZoneType zone_type, uint64_t chain_id,
                                         const std::string& table, const std::string& key) {
    l1_read_count_++;
    
    // 首先尝试从读缓存获取
    auto cached_value = getFromReadCache(table, key);
    if (cached_value.has_value()) {
        l1_hit_count_++;
        return cached_value.value();
    }
    
    l2_read_count_++;
    
    // 从L1缓存获取
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (zone_cache) {
        std::string value;
        zone_cache->get(chain_id, table, key, value);
        if (!value.empty()) {
            l2_hit_count_++;
            // 更新读缓存
            putIntoReadCache(table, key, value);
            return value;
        }
    }
    
    // 从L2缓存（已提交数据库）获取
    if (committed_db_) {
        std::string value;
        committed_db_->get(table, key, value);
        if (!value.empty()) {
            // 更新读缓存
            putIntoReadCache(table, key, value);
            return value;
        }
    }
    
    // 未找到数据
    return "";
}

void EnhancedStateCacheManager::put(ZoneType zone_type, uint64_t chain_id,
                                  const std::string& table, const std::string& key, 
                                  const std::string& value) {
    write_count_++;
    
    // 写入L1缓存
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (zone_cache) {
        zone_cache->put(chain_id, table, key, value);
    }
    
    // 更新读缓存
    putIntoReadCache(table, key, value);
    
    // 使相关的KeyPage聚合器失效
    if (zone_type == ZoneType::NC_ZONE && nc_zone_aggregator_) {
        nc_zone_aggregator_->addUpdate(table, key, value);
    } else if (zone_type == ZoneType::C_ZONE && c_zone_aggregator_) {
        c_zone_aggregator_->addUpdate(table, key, value);
    }
}

void EnhancedStateCacheManager::batchCommit(epoch_size_t epoch) {
    LOG(INFO) << "[ENHANCED_CACHE] Starting batch commit for epoch " << epoch;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 分别提交NC_ZONE和C_ZONE的缓存
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        
        // 提交NC_ZONE缓存
        if (nc_zone_cache_) {
            LOG(INFO) << "[ENHANCED_CACHE] Committing NC zone cache for epoch " << epoch;
            nc_zone_cache_->commit(committed_db_.get());
            
            // 清理NC_ZONE缓存
            {
                std::unique_lock<std::shared_mutex> zone_lock(nc_zone_cache_->getMutex());
                nc_zone_cache_->clear();
            }
        }
        
        // 提交C_ZONE缓存
        if (c_zone_cache_) {
            LOG(INFO) << "[ENHANCED_CACHE] Committing C zone cache for epoch " << epoch;
            c_zone_cache_->commit(committed_db_.get());
            
            // 清理C_ZONE缓存
            {
                std::unique_lock<std::shared_mutex> zone_lock(c_zone_cache_->getMutex());
                c_zone_cache_->clear();
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[ENHANCED_CACHE] Batch commit completed in " << duration << " μs for epoch " << epoch;
}

void EnhancedStateCacheManager::evictOldEpochs(epoch_size_t current_epoch) {
    LOG(INFO) << "[ENHANCED_CACHE] Evicting old epochs, current epoch: " << current_epoch;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 清理读缓存中可能过期的条目
    {
        std::unique_lock<std::shared_mutex> lock(read_cache_mutex_);
        // 在增强版本中，我们可以实现更智能的缓存清理策略
        // 例如LRU或基于访问频率的清理
        LOG(INFO) << "[ENHANCED_CACHE] Read cache size before eviction: " << read_cache_.size();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[ENHANCED_CACHE] Epoch eviction completed in " << duration << " μs";
}

void EnhancedStateCacheManager::mergeSequenceToL1(ZoneType zone_type,
                                                SequenceCache& sequence_cache) {
    LOG(INFO) << "[ENHANCED_CACHE] Merging sequence " << sequence_cache.sequenceId() 
              << " to L1 for " << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 获取对应区域的缓存
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (!zone_cache) {
        LOG(WARNING) << "[ENHANCED_CACHE] Zone cache not created for zone type " << static_cast<int>(zone_type);
        return;
    }
    
    // 获取序列的所有变更
    const auto& all_changes = sequence_cache.getAllChanges();
    
    // 将非链级变更合并到L1缓存
    for (const auto& table_entry : all_changes) {
        const std::string& table = table_entry.first;
        for (const auto& key_entry : table_entry.second) {
            const std::string& key = key_entry.first;
            const std::string& value = key_entry.second;
            
            // 写入L1缓存（chain_id为0表示非链级变更）
            zone_cache->put(0, table, key, value);
            
            // 同时添加到对应的KeyPage聚合器中
            if (zone_type == ZoneType::NC_ZONE && nc_zone_aggregator_) {
                nc_zone_aggregator_->addUpdate(table, key, value);
            } else if (zone_type == ZoneType::C_ZONE && c_zone_aggregator_) {
                c_zone_aggregator_->addUpdate(table, key, value);
            }
        }
    }
    
    // 获取序列的所有链级变更
    const auto& all_chain_changes = sequence_cache.getAllChainChanges();
    
    // 将链级变更合并到L1缓存
    for (const auto& chain_entry : all_chain_changes) {
        uint64_t chain_id = chain_entry.first;
        const auto& chain_changes = chain_entry.second;
        
        // 为每个链创建一个ChainDiff对象
        auto chain_diff = std::make_unique<ChainDiff>(chain_id);
        
        for (const auto& table_entry : chain_changes) {
            const std::string& table = table_entry.first;
            for (const auto& key_entry : table_entry.second) {
                const std::string& key = key_entry.first;
                const std::string& value = key_entry.second;
                
                // 写入链级差异
                chain_diff->write(table, key, value);
                
                // 同时添加到对应的KeyPage聚合器中
                if (zone_type == ZoneType::NC_ZONE && nc_zone_aggregator_) {
                    nc_zone_aggregator_->addUpdate(table, key, value);
                } else if (zone_type == ZoneType::C_ZONE && c_zone_aggregator_) {
                    c_zone_aggregator_->addUpdate(table, key, value);
                }
            }
        }
        
        // 将链级差异添加到L1缓存
        zone_cache->addChainDiff(std::move(chain_diff));
    }
    
    // 清理序列缓存
    sequence_cache.clear();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[ENHANCED_CACHE] Sequence merge completed in " << duration << " μs";
}

void EnhancedStateCacheManager::syncNCZoneToL2() {
    LOG(INFO) << "[ENHANCED_CACHE] Syncing NC zone L1 to L2 (Global Sync Point 1)";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!nc_zone_aggregator_) {
        LOG(WARNING) << "[ENHANCED_CACHE] NC zone aggregator not initialized";
        return;
    }
    
    // 获取所有聚合的KeyPage
    std::vector<KeyPage> pages = nc_zone_aggregator_->getKeyPages();
    
    if (pages.empty()) {
        LOG(INFO) << "[ENHANCED_CACHE] No updates to sync for NC zone";
        return;
    }
    
    LOG(INFO) << "[ENHANCED_CACHE] Syncing " << pages.size() << " KeyPages for NC zone";
    
    // 批量写入L2
    batchWriteKeyPagesToL2(pages);
    
    // 清理聚合器
    nc_zone_aggregator_->clear();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[ENHANCED_CACHE] NC zone sync completed in " << duration << " μs";
}

void EnhancedStateCacheManager::syncCZoneToL2() {
    LOG(INFO) << "[ENHANCED_CACHE] Syncing C zone L1 to L2 (Global Sync Point 2)";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!c_zone_aggregator_) {
        LOG(WARNING) << "[ENHANCED_CACHE] C zone aggregator not initialized";
        return;
    }
    
    // 获取所有聚合的KeyPage
    std::vector<KeyPage> pages = c_zone_aggregator_->getKeyPages();
    
    if (pages.empty()) {
        LOG(INFO) << "[ENHANCED_CACHE] No updates to sync for C zone";
        return;
    }
    
    LOG(INFO) << "[ENHANCED_CACHE] Syncing " << pages.size() << " KeyPages for C zone";
    
    // 批量写入L2
    batchWriteKeyPagesToL2(pages);
    
    // 清理聚合器
    c_zone_aggregator_->clear();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[ENHANCED_CACHE] C zone sync completed in " << duration << " μs";
}

void EnhancedStateCacheManager::batchWriteKeyPagesToL2(const std::vector<KeyPage>& pages) {
    if (pages.empty() || !committed_db_) {
        return;
    }
    
    LOG(INFO) << "[ENHANCED_CACHE] Batch writing " << pages.size() << " KeyPages to L2";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process each KeyPage
    for (const auto& page : pages) {
        // Write all entries in the page
        for (const auto& entry : page.entries) {
            const std::string& key = entry.first;
            const std::string& value = entry.second;
            
            // Update L2 storage
            committed_db_->put(page.table, key, value);
            
            // Update L2 read cache
            putIntoReadCache(page.table, key, value);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[ENHANCED_CACHE] Batch write completed in " << duration << " μs";
}

void EnhancedStateCacheManager::batchMergeChainsAtPartitionSyncPoint(ZoneType zone_type,
                                                                   const std::vector<uint64_t>& chain_order) {
    LOG(INFO) << "[ENHANCED_CACHE] Batch merging chains at partition sync point for " 
              << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 获取对应区域的缓存
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (!zone_cache) {
        LOG(WARNING) << "[ENHANCED_CACHE] Zone cache not created for zone type " << static_cast<int>(zone_type);
        return;
    }
    
    // 批量合并链级差异
    zone_cache->batchMergeChainDiffs(chain_order);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[ENHANCED_CACHE] Batch chain merge completed in " << duration << " μs";
}

void EnhancedStateCacheManager::putIntoReadCache(const std::string& table, 
                                                const std::string& key, 
                                                const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(read_cache_mutex_);
    
    // 检查缓存大小，如果超过容量则进行清理
    if (read_cache_.size() >= read_cache_capacity_) {
        // 简单的LRU实现：删除一部分旧条目
        size_t entries_to_remove = read_cache_capacity_ / 10; // 删除10%的条目
        auto it = read_cache_.begin();
        for (size_t i = 0; i < entries_to_remove && it != read_cache_.end(); ++i) {
            it = read_cache_.erase(it);
        }
        cache_eviction_count_ += entries_to_remove;
    }
    
    // 插入新条目
    std::string cache_key = table + "|" + key;
    read_cache_[cache_key] = value;
}

std::optional<std::string> EnhancedStateCacheManager::getFromReadCache(const std::string& table, 
                                                                      const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(read_cache_mutex_);
    
    std::string cache_key = table + "|" + key;
    auto it = read_cache_.find(cache_key);
    if (it != read_cache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void EnhancedStateCacheManager::invalidateReadCache(const std::string& table, const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(read_cache_mutex_);
    
    std::string cache_key = table + "|" + key;
    auto it = read_cache_.find(cache_key);
    if (it != read_cache_.end()) {
        read_cache_.erase(it);
        cache_eviction_count_++;
    }
}

CacheStatistics EnhancedStateCacheManager::getStatistics() const {
    CacheStatistics stats;
    
    stats.l1_read_count = l1_read_count_.load();
    stats.l2_read_count = l2_read_count_.load();
    stats.l1_hit_count = l1_hit_count_.load();
    stats.l2_hit_count = l2_hit_count_.load();
    stats.write_count = write_count_.load();
    stats.cache_eviction_count = cache_eviction_count_.load();
    
    // 计算缓存大小
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        if (nc_zone_cache_) {
            stats.l1_cache_size += nc_zone_cache_->size();
        }
        if (c_zone_cache_) {
            stats.l1_cache_size += c_zone_cache_->size();
        }
    }
    
    {
        std::shared_lock<std::shared_mutex> lock(read_cache_mutex_);
        stats.read_cache_size = read_cache_.size();
    }
    
    // 计算命中率
    calculateHitRates(stats);
    
    return stats;
}

void EnhancedStateCacheManager::calculateHitRates(CacheStatistics& stats) const {
    if (stats.l1_read_count > 0) {
        stats.l1_hit_rate = static_cast<double>(stats.l1_hit_count) / stats.l1_read_count;
    }
    
    if (stats.l2_read_count > 0) {
        stats.l2_hit_rate = static_cast<double>(stats.l2_hit_count) / stats.l2_read_count;
    }
}

void EnhancedStateCacheManager::resetStatistics() {
    l1_read_count_.store(0);
    l2_read_count_.store(0);
    l1_hit_count_.store(0);
    l2_hit_count_.store(0);
    write_count_.store(0);
    cache_eviction_count_.store(0);
}

void EnhancedStateCacheManager::enableBackgroundFlush(bool enable) {
    background_flush_enabled_ = enable;
    if (enable) {
        flush_cv_.notify_all();
    }
}

void EnhancedStateCacheManager::backgroundFlushThread() {
    LOG(INFO) << "[ENHANCED_CACHE] Background flush thread started";
    
    while (!stop_background_thread_) {
        // 等待刷新信号或超时
        std::unique_lock<std::mutex> lock(flush_mutex_);
        flush_cv_.wait_for(lock, std::chrono::seconds(5), [this] { 
            return stop_background_thread_ || background_flush_enabled_; 
        });
        
        if (stop_background_thread_) {
            break;
        }
        
        if (background_flush_enabled_) {
            // 执行后台刷新操作
            // 这里可以实现定期的缓存刷新、统计信息收集等操作
            auto stats = getStatistics();
            LOG(INFO) << "[ENHANCED_CACHE] Background stats - L1 hit rate: " << stats.l1_hit_rate
                      << ", L2 hit rate: " << stats.l2_hit_rate
                      << ", Read cache size: " << stats.read_cache_size;
        }
    }
    
    LOG(INFO) << "[ENHANCED_CACHE] Background flush thread stopped";
}

} // namespace blp