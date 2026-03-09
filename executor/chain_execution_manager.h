//
// ChainExecutionManager: 链级异步执行管理器
// 实现全局同步点和分区隔离执行机制
//

#ifndef NEUBLOCKCHAIN_CHAIN_EXECUTION_MANAGER_H
#define NEUBLOCKCHAIN_CHAIN_EXECUTION_MANAGER_H

#include <vector>
#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include "dependency_chain.h"
#include "shard.h"
#include "two_zone_types.h"
#include "non_conflicting_zone_executor.h"
#include "conflicting_zone_executor.h"
#include "../../../utilities/cyclic_barrier.h"
#include "../../storage/core/cache/state_cache_manager.h"

namespace scheduling {

class ChainExecutionManager {
public:
    // 构造函数
    explicit ChainExecutionManager(blp::StateCacheManager* cache_manager);
    
    // 析构函数
    ~ChainExecutionManager() = default;
    
    // 删除拷贝构造和赋值操作
    ChainExecutionManager(const ChainExecutionManager&) = delete;
    ChainExecutionManager& operator=(const ChainExecutionManager&) = delete;
    ChainExecutionManager(ChainExecutionManager&&) = delete;
    ChainExecutionManager& operator=(ChainExecutionManager&&) = delete;
    
    /**
     * 链级异步执行入口点
     * @param shards 分片列表
     * @param config 执行配置
     * @return 执行统计信息
     */
    TwoZoneStatistics executeChains(std::vector<Shard>& shards, 
                                   const NonConflictingZoneExecutor::ExecutionConfig& config);
    
    /**
     * 设置线程池大小
     * @param thread_pool_size 线程池大小
     */
    void setThreadPoolSize(size_t thread_pool_size);

private:
    /**
     * 执行无冲突区域
     * @param shards 分片列表
     * @param config 执行配置
     * @param stats 执行统计信息
     */
    void executeNonConflictingZone(std::vector<Shard>& shards,
                                  const NonConflictingZoneExecutor::ExecutionConfig& config,
                                  TwoZoneStatistics& stats);
    
    /**
     * 全局同步点
     * 等待所有无冲突区域执行完成，然后执行批量合并
     * @param shards 分片列表
     */
    void globalSynchronizationPoint(std::vector<Shard>& shards);
    
    /**
     * 执行冲突区域
     * @param shards 分片列表
     * @param config 执行配置
     * @param stats 执行统计信息
     */
    void executeConflictingZone(std::vector<Shard>& shards,
                              const scheduling::NonConflictingZoneExecutor::ExecutionConfig& config,
                              TwoZoneStatistics& stats);
    
    /**
     * 批量合并到L1缓存
     * @param shards 分片列表
     */
    void batchMergeToL1(std::vector<Shard>& shards);
    
    /**
     * 获取最优线程数
     * @return 最优线程数
     */
    size_t getOptimalThreadCount() const;
    
    // 成员变量
    blp::StateCacheManager* cache_manager_;
    size_t thread_pool_size_;
    
    // 全局屏障，用于同步无冲突区域和冲突区域的执行
    std::unique_ptr<cbar::CyclicBarrier> global_barrier_;
    
    // 执行统计信息
    TwoZoneStatistics execution_stats_;
};

} // namespace scheduling

#endif // NEUBLOCKCHAIN_CHAIN_EXECUTION_MANAGER_H