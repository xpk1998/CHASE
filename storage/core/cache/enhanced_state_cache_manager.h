//
// Enhanced State Cache Manager
// Improved version of StateCacheManager with better synchronization mechanisms
//

#ifndef NEUBLOCKCHAIN_ENHANCED_STATE_CACHE_MANAGER_H
#define NEUBLOCKCHAIN_ENHANCED_STATE_CACHE_MANAGER_H

#include "uncommitted_state_cache.h"
#include "sequence_cache.h"
#include "../state/key_page_aggregator.h"
//#include "database/versioned_db.h"
#include "../../../utilities/types/aria_types.h"
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <optional>

namespace blp {

// 区域类型枚举
enum class ZoneType {
    NC_ZONE,  // 非冲突区域
    C_ZONE    // 冲突区域
};

// 缓存统计信息
struct CacheStatistics {
    uint64_t l1_read_count = 0;
    uint64_t l2_read_count = 0;
    uint64_t l1_hit_count = 0;
    uint64_t l2_hit_count = 0;
    uint64_t write_count = 0;
    uint64_t cache_eviction_count = 0;
    double l1_hit_rate = 0.0;
    double l2_hit_rate = 0.0;
    size_t l1_cache_size = 0;
    size_t l2_cache_size = 0;
    size_t read_cache_size = 0;
};

// 增强的状态缓存管理器类
class EnhancedStateCacheManager {
public:
    // 构造函数
    EnhancedStateCacheManager(std::shared_ptr<VersionedDB> committed_db,
                            size_t read_cache_capacity = 10000,
                            size_t l1_cache_capacity = 50000);
    
    // 析构函数
    ~EnhancedStateCacheManager();
    
    // 初始化缓存区域
    void initializeZoneCaches(ZoneType zone_type);
    
    // 获取指定区域的缓存
    UncommittedStateCache* getZoneCache(ZoneType zone_type);
    
    // 从缓存读取数据（L1 + L2 + CommittedDB）
    std::string get(ZoneType zone_type, uint64_t chain_id, 
                   const std::string& table, const std::string& key);
    
    // 写入缓存数据
    void put(ZoneType zone_type, uint64_t chain_id,
             const std::string& table, const std::string& key, const std::string& value);
    
    // 批量提交缓存数据到L2
    void batchCommit(epoch_size_t epoch);
    
    // 清理旧的epoch数据
    void evictOldEpochs(epoch_size_t current_epoch);
    
    // 将序列缓存合并到L1
    void mergeSequenceToL1(ZoneType zone_type, SequenceCache& sequence_cache);
    
    // 同步NC区域到L2（全局同步点1）
    void syncNCZoneToL2();
    
    // 同步C区域到L2（全局同步点2）
    void syncCZoneToL2();
    
    // 在分区同步点批量合并链级差异
    void batchMergeChainsAtPartitionSyncPoint(ZoneType zone_type,
                                             const std::vector<uint64_t>& chain_order);
    
    // 获取缓存统计信息
    CacheStatistics getStatistics() const;
    
    // 重置统计信息
    void resetStatistics();
    
    // 启用/禁用后台刷新线程
    void enableBackgroundFlush(bool enable);
    
private:
    // 批量写入KeyPage到L2
    void batchWriteKeyPagesToL2(const std::vector<KeyPage>& pages);
    
    // 写入读缓存
    void putIntoReadCache(const std::string& table, const std::string& key, const std::string& value);
    
    // 从读缓存读取
    std::optional<std::string> getFromReadCache(const std::string& table, const std::string& key) const;
    
    // 使读缓存失效
    void invalidateReadCache(const std::string& table, const std::string& key);
    
    // 后台刷新线程函数
    void backgroundFlushThread();
    
    // 计算缓存命中率
    void calculateHitRates(CacheStatistics& stats) const;
    
    // L1缓存（未提交状态缓存）
    std::unique_ptr<UncommittedStateCache> nc_zone_cache_;
    std::unique_ptr<UncommittedStateCache> c_zone_cache_;
    
    // L2缓存（已提交数据库）
    std::shared_ptr<VersionedDB> committed_db_;
    
    // 读缓存（L1 + L2的组合视图）
    mutable std::shared_mutex read_cache_mutex_;
    std::unordered_map<std::string, std::string> read_cache_; // table|key -> value
    size_t read_cache_capacity_;
    
    // KeyPage聚合器
    std::unique_ptr<KeyPageAggregator> nc_zone_aggregator_;
    std::unique_ptr<KeyPageAggregator> c_zone_aggregator_;
    
    // 缓存互斥锁
    mutable std::shared_mutex cache_mutex_;
    
    // 统计信息
    mutable std::atomic<uint64_t> l1_read_count_;
    mutable std::atomic<uint64_t> l2_read_count_;
    mutable std::atomic<uint64_t> l1_hit_count_;
    mutable std::atomic<uint64_t> l2_hit_count_;
    mutable std::atomic<uint64_t> write_count_;
    mutable std::atomic<uint64_t> cache_eviction_count_;
    
    // 后台刷新线程相关
    std::atomic<bool> background_flush_enabled_;
    std::atomic<bool> stop_background_thread_;
    std::thread background_thread_;
    std::mutex flush_mutex_;
    std::condition_variable flush_cv_;
};

} // namespace blp

#endif // NEUBLOCKCHAIN_ENHANCED_STATE_CACHE_MANAGER_H