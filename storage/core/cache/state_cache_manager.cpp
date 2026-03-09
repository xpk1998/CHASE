#include "state_cache_manager.h"
#include "uncommitted_state_cache.h"
#include "sequence_cache.h"
#include "../state/key_page_aggregator.h"
#include "../../../utilities/types/aria_types.h"
#include "glog/logging.h"
#include <chrono>
#include <fstream>

namespace blp {

void StateCacheManager::batchCommit(epoch_size_t epoch) {
    LOG(INFO) << "[CACHE] Starting batch commit for epoch " << epoch;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 分别提交NC_ZONE和C_ZONE的缓存
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        
        // 提交NC_ZONE缓存
        if (nc_zone_cache_) {
            LOG(INFO) << "[CACHE] Committing NC zone cache for epoch " << epoch;
            nc_zone_cache_->commit(committed_db_);
            
            // 清理NC_ZONE缓存
            {
                std::unique_lock<std::shared_mutex> zone_lock(nc_zone_cache_->getMutex());
                nc_zone_cache_->clear();
            }
        }
        
        // 提交C_ZONE缓存
        if (c_zone_cache_) {
            LOG(INFO) << "[CACHE] Committing C zone cache for epoch " << epoch;
            c_zone_cache_->commit(committed_db_);
            
            // 清理C_ZONE缓存
            {
                std::unique_lock<std::shared_mutex> zone_lock(c_zone_cache_->getMutex());
                c_zone_cache_->clear();
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[CACHE] Batch commit completed in " << duration << " μs for epoch " << epoch;
}

void StateCacheManager::evictOldEpochs(epoch_size_t current_epoch) {
    LOG(INFO) << "[CACHE] Evicting old epochs, current epoch: " << current_epoch;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 在这个实现中，我们假设缓存是按epoch组织的
    // 但由于当前实现没有使用epoch作为键，我们只清理过期的缓存项
    // 这里我们可以添加更复杂的epoch管理逻辑
    
    // 清理读缓存中可能过期的条目
    {
        std::unique_lock<std::shared_mutex> lock(read_cache_mutex_);
        // 在实际实现中，如果读缓存支持epoch标记，我们可以清理过期的条目
        // 但由于当前实现不支持，我们暂时保持不变
        LOG(INFO) << "[CACHE] Read cache size before eviction: " << read_cache_.size();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[CACHE] Epoch eviction completed in " << duration << " μs";
}

void StateCacheManager::mergeSequenceToL1(ZoneType zone_type,
                                          SequenceCache& sequence_cache) {
    LOG(INFO) << "[CACHE] Merging sequence " << sequence_cache.sequenceId() 
              << " to L1 for " << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 获取对应区域的缓存
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (!zone_cache) {
        LOG(WARNING) << "[CACHE] Zone cache not created for zone type " << static_cast<int>(zone_type);
        return;
    }
    
    // 获取序列的所有变更
    const auto& all_changes = sequence_cache.getAllChanges();
    
    // 将非链级变更合并到L1缓存
    // Let UncommittedStateCache handle its own locking
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
    LOG(INFO) << "[CACHE] Sequence merge completed in " << duration << " μs";
}

void StateCacheManager::syncNCZoneToL2() {
    LOG(INFO) << "[CACHE] Syncing NC zone L1 to L2 (Global Sync Point 1)";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!nc_zone_aggregator_) {
        LOG(WARNING) << "[CACHE] NC zone aggregator not initialized";
        return;
    }
    
    // 获取所有聚合的KeyPage
    std::vector<KeyPage> pages = nc_zone_aggregator_->getKeyPages();
    
    if (pages.empty()) {
        LOG(INFO) << "[CACHE] No updates to sync for NC zone";
        return;
    }
    
    LOG(INFO) << "[CACHE] Syncing " << pages.size() << " KeyPages for NC zone";
    
    // 批量写入L2
    batchWriteKeyPagesToL2(pages);
    
    // 清理聚合器
    nc_zone_aggregator_->clear();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[CACHE] NC zone sync completed in " << duration << " μs";
}

void StateCacheManager::syncCZoneToL2() {
    LOG(INFO) << "[CACHE] Syncing C zone L1 to L2 (Global Sync Point 2)";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!c_zone_aggregator_) {
        LOG(WARNING) << "[CACHE] C zone aggregator not initialized";
        return;
    }
    
    // 获取所有聚合的KeyPage
    std::vector<KeyPage> pages = c_zone_aggregator_->getKeyPages();
    
    if (pages.empty()) {
        LOG(INFO) << "[CACHE] No updates to sync for C zone";
        return;
    }
    
    LOG(INFO) << "[CACHE] Syncing " << pages.size() << " KeyPages for C zone";
    
    // 批量写入L2
    batchWriteKeyPagesToL2(pages);
    
    // 清理聚合器
    c_zone_aggregator_->clear();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[CACHE] C zone sync completed in " << duration << " μs";
}

void StateCacheManager::batchWriteKeyPagesToL2(const std::vector<KeyPage>& pages) {
    if (pages.empty() || !committed_db_) {
        return;
    }
    
    LOG(INFO) << "[CACHE] Batch writing " << pages.size() << " KeyPages to L2";
    
    // Cast committed_db_ to Database* to get storage
    Database* db = dynamic_cast<Database*>(committed_db_);
    if (!db) {
        LOG(ERROR) << "[CACHE] Failed to cast committed_db_ to Database*";
        return;
    }
    
    DBStorage* storage = db->getStorage();
    if (!storage) {
        LOG(ERROR) << "[CACHE] Failed to get storage from committed database";
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process each KeyPage
    for (const auto& page : pages) {
        // Write all entries in the page
        for (const auto& entry : page.entries) {
            const std::string& key = entry.first;
            const std::string& value = entry.second;
            
            // Update L2 storage
            storage->updateWriteSet(key, value, page.table);
            
            // Update L2 read cache
            putIntoReadCache(page.table, key, value);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[CACHE] Batch write completed in " << duration << " μs";
}

void StateCacheManager::batchMergeChainsAtPartitionSyncPoint(ZoneType zone_type,
                                                          const std::vector<uint64_t>& chain_order) {
    LOG(INFO) << "[CACHE] Batch merging chains at partition sync point for " 
              << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 获取对应区域的缓存
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (!zone_cache) {
        LOG(WARNING) << "[CACHE] Zone cache not created for zone type " << static_cast<int>(zone_type);
        return;
    }
    
    // 批量合并链级差异
    zone_cache->batchMergeChainDiffs(chain_order);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    LOG(INFO) << "[CACHE] Batch chain merge completed in " << duration << " μs";
}

} // namespace blp