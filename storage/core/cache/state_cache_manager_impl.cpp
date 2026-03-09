#include "state_cache_manager.h"
#include "uncommitted_state_cache.h"
#include "../state/key_page_aggregator.h"
#include "../../../utilities/types/aria_types.h"
#include "glog/logging.h"
#include <chrono>
#include <thread>
#include <future>

namespace blp {

StateCacheManager::StateCacheManager(VersionedDB* committed_db) 
    : committed_db_(committed_db),
      nc_zone_cache_(nullptr),
      c_zone_cache_(nullptr),
      nc_zone_aggregator_(std::make_unique<KeyPageAggregator>()),
      c_zone_aggregator_(std::make_unique<KeyPageAggregator>()) {
    LOG(INFO) << "[CACHE] StateCacheManager initialized";
}

StateCacheManager::~StateCacheManager() {
    LOG(INFO) << "[CACHE] StateCacheManager destroyed";
}

void StateCacheManager::createZoneCache(ZoneType zone_type) {
    LOG(INFO) << "[CACHE] Creating zone cache for " 
              << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    if (zone_type == ZoneType::NC_ZONE) {
        nc_zone_cache_ = std::make_unique<UncommittedStateCache>(0); // epoch 0 for now
    } else {
        c_zone_cache_ = std::make_unique<UncommittedStateCache>(0); // epoch 0 for now
    }
}

bool StateCacheManager::getState(ZoneType zone_type,
                                 const std::string& table,
                                 const std::string& key,
                                 std::string& value) {
    // First, try to get from L1 cache
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (zone_cache) {
        std::shared_lock<std::shared_mutex> lock(zone_cache->getMutex());
        if (zone_cache->get(0, table, key, value)) {
            cache_hits_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    
    // Second, try to get from read cache
    if (getFromReadCache(table, key, value)) {
        cache_hits_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    
    // Finally, get from committed state (persistent storage)
    if (committed_db_ && committed_db_->get(table, key, value)) {
        committed_state_reads_.fetch_add(1, std::memory_order_relaxed);
        
        // Put into read cache for future queries
        putIntoReadCache(table, key, value);
        return true;
    }
    
    cache_misses_.fetch_add(1, std::memory_order_relaxed);
    return false;
}

void StateCacheManager::putState(ZoneType zone_type,
                                 const std::string& table,
                                 const std::string& key,
                                 const std::string& value) {
    LOG(INFO) << "[CACHE] putState called for zone " << static_cast<int>(zone_type) 
              << ", table: " << table << ", key: " << key;
    
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (!zone_cache) {
        LOG(WARNING) << "[CACHE] Zone cache not created for zone type " << static_cast<int>(zone_type);
        return;
    }
    
    LOG(INFO) << "[CACHE] Calling zone_cache->put directly (no mutex needed here)";
    try {
        // Let UncommittedStateCache handle its own locking
        zone_cache->put(0, table, key, value);
        LOG(INFO) << "[CACHE] putState completed";
    } catch (const std::system_error& e) {
        LOG(ERROR) << "[CACHE] Exception in putState: " << e.what();
        throw;
    }
}

void StateCacheManager::commitState(ZoneType zone_type) {
    LOG(INFO) << "[CACHE] Committing state for " 
              << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (!zone_cache) {
        LOG(WARNING) << "[CACHE] Zone cache not created for zone type " << static_cast<int>(zone_type);
        return;
    }
    
    zone_cache->commit(committed_db_);
    
    // Clear the cache after successful commit
    // Note: commit() already acquires the lock and clears the cache internally
    // So we don't need to acquire the lock again
}

void StateCacheManager::rollbackState(ZoneType zone_type) {
    LOG(INFO) << "[CACHE] Rolling back state for " 
              << (zone_type == ZoneType::NC_ZONE ? "NC" : "C") << " zone";
    
    UncommittedStateCache* zone_cache = getZoneCache(zone_type);
    if (!zone_cache) {
        LOG(WARNING) << "[CACHE] Zone cache not created for zone type " << static_cast<int>(zone_type);
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(zone_cache->getMutex());
    zone_cache->clear();
}

void StateCacheManager::cleanupCache() {
    LOG(INFO) << "[CACHE] Cleaning up all caches";
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    if (nc_zone_cache_) {
        // Let UncommittedStateCache handle its own locking
        nc_zone_cache_->clear();
        nc_zone_cache_.reset();
    }
    
    if (c_zone_cache_) {
        // Let UncommittedStateCache handle its own locking
        c_zone_cache_->clear();
        c_zone_cache_.reset();
    }
    
    // Clear aggregators
    if (nc_zone_aggregator_) {
        nc_zone_aggregator_->clear();
    }
    
    if (c_zone_aggregator_) {
        c_zone_aggregator_->clear();
    }
    
    // Clear read cache
    {
        std::unique_lock<std::shared_mutex> read_lock(read_cache_mutex_);
        read_cache_.clear();
        read_cache_lru_.clear();
    }
}

void StateCacheManager::asyncCleanupCache() {
    LOG(INFO) << "[CACHE] Starting asynchronous cache cleanup";
    
    // Create a detached thread to perform cleanup
    std::thread cleanup_thread([this]() {
        try {
            cleanupCache();
            LOG(INFO) << "[CACHE] Asynchronous cache cleanup completed";
        } catch (const std::exception& e) {
            LOG(ERROR) << "[CACHE] Error during asynchronous cache cleanup: " << e.what();
        }
    });
    
    cleanup_thread.detach();
}

UncommittedStateCache* StateCacheManager::getZoneCache(ZoneType zone_type) {
    // 不再持有锁，直接返回缓存指针
    // 调用者需要确保在使用返回的指针时正确处理并发访问
    
    if (zone_type == ZoneType::NC_ZONE) {
        return nc_zone_cache_.get();
    } else {
        return c_zone_cache_.get();
    }
}

bool StateCacheManager::getFromReadCache(const std::string& table, 
                                         const std::string& key,
                                         std::string& value) {
    std::string cache_key = table + ":" + key;
    
    std::shared_lock<std::shared_mutex> lock(read_cache_mutex_);
    auto it = read_cache_.find(cache_key);
    if (it != read_cache_.end()) {
        value = it->second.value;
        
        // Update LRU position
        read_cache_lru_.erase(it->second.lru_iter);
        read_cache_lru_.push_front(cache_key);
        read_cache_[cache_key].lru_iter = read_cache_lru_.begin();
        
        return true;
    }
    
    return false;
}

void StateCacheManager::putIntoReadCache(const std::string& table,
                                         const std::string& key,
                                         const std::string& value) {
    std::string cache_key = table + ":" + key;
    
    std::unique_lock<std::shared_mutex> lock(read_cache_mutex_);
    
    // Check if key already exists
    auto it = read_cache_.find(cache_key);
    if (it != read_cache_.end()) {
        // Update existing entry
        it->second.value = value;
        
        // Update LRU position
        read_cache_lru_.erase(it->second.lru_iter);
        read_cache_lru_.push_front(cache_key);
        it->second.lru_iter = read_cache_lru_.begin();
    } else {
        // Add new entry
        read_cache_lru_.push_front(cache_key);
        read_cache_[cache_key] = {value, read_cache_lru_.begin()};
        
        // Evict oldest entry if cache is full
        if (read_cache_.size() > READ_CACHE_MAX_SIZE) {
            std::string oldest_key = read_cache_lru_.back();
            read_cache_.erase(oldest_key);
            read_cache_lru_.pop_back();
        }
    }
}

void StateCacheManager::invalidateReadCache(const std::string& table, const std::string& key) {
    std::string cache_key = table + ":" + key;
    
    std::unique_lock<std::shared_mutex> lock(read_cache_mutex_);
    auto it = read_cache_.find(cache_key);
    if (it != read_cache_.end()) {
        read_cache_lru_.erase(it->second.lru_iter);
        read_cache_.erase(it);
    }
}

CacheStats StateCacheManager::getStats() const {
    CacheStats stats;
    stats.cache_hits = cache_hits_.load(std::memory_order_relaxed);
    stats.cache_misses = cache_misses_.load(std::memory_order_relaxed);
    stats.committed_state_reads = committed_state_reads_.load(std::memory_order_relaxed);
    stats.lock_contentions = lock_contentions_.load(std::memory_order_relaxed);
    stats.lock_wait_time_us = lock_wait_time_us_.load(std::memory_order_relaxed);
    stats.degradation_events = degradation_events_.load(std::memory_order_relaxed);
    stats.is_degraded = is_degraded_.load(std::memory_order_relaxed);
    
    stats.calculate();
    return stats;
}

void StateCacheManager::printStats() const {
    CacheStats stats = getStats();
    
    LOG(INFO) << "[CACHE STATS] Hits: " << stats.cache_hits 
              << ", Misses: " << stats.cache_misses
              << ", Hit Rate: " << stats.cache_hit_rate
              << ", Committed Reads: " << stats.committed_state_reads
              << ", Contention Rate: " << stats.contention_rate
              << ", Is Degraded: " << (stats.is_degraded ? "Yes" : "No");
}

std::string StateCacheManager::exportStats() const {
    CacheStats stats = getStats();
    
    std::ostringstream oss;
    oss << "cache_hits=" << stats.cache_hits << ","
        << "cache_misses=" << stats.cache_misses << ","
        << "hit_rate=" << stats.cache_hit_rate << ","
        << "committed_reads=" << stats.committed_state_reads << ","
        << "contention_rate=" << stats.contention_rate << ","
        << "is_degraded=" << (stats.is_degraded ? "1" : "0");
    
    return oss.str();
}

} // namespace blp