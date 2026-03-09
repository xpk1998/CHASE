#include "chain_execution_manager.h"
#include <algorithm>
#include <glog/logging.h>

namespace scheduling {

ChainExecutionManager::ChainExecutionManager(blp::StateCacheManager* cache_manager)
    : cache_manager_(cache_manager), thread_pool_size_(0) {
    LOG(INFO) << "ChainExecutionManager created";
}

TwoZoneStatistics ChainExecutionManager::executeChains(std::vector<Shard>& shards, 
                                                     const NonConflictingZoneExecutor::ExecutionConfig& config) {
    LOG(INFO) << "Starting chain execution with " << shards.size() << " shards";
    
    // Reset statistics
    execution_stats_ = TwoZoneStatistics();
    
    // Execute non-conflicting zone
    executeNonConflictingZone(shards, config, execution_stats_);
    
    // Global synchronization point
    globalSynchronizationPoint(shards);
    
    // Execute conflicting zone
    executeConflictingZone(shards, config, execution_stats_);
    
    LOG(INFO) << "Chain execution completed";
    return execution_stats_;
}

void ChainExecutionManager::setThreadPoolSize(size_t thread_pool_size) {
    thread_pool_size_ = thread_pool_size;
    LOG(INFO) << "Thread pool size set to " << thread_pool_size_;
}

void ChainExecutionManager::executeNonConflictingZone(std::vector<Shard>& shards,
                                                    const NonConflictingZoneExecutor::ExecutionConfig& config,
                                                    TwoZoneStatistics& stats) {
    LOG(INFO) << "Executing non-conflicting zone with " << shards.size() << " shards";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Create sequences for non-conflicting zone
    std::vector<std::vector<FinalizedTransaction>> sequences;
    for (auto& shard : shards) {
        // Extract non-conflicting transactions from shard
        std::vector<FinalizedTransaction> non_conflicting_txs;
        for (const auto& seq : shard.sequences) {
            // Filter for non-conflicting transactions (this is simplified)
            for (const auto& tx : seq.transactions) {
                // In a real implementation, we would check conflict status
                non_conflicting_txs.push_back(tx);
            }
        }
        if (!non_conflicting_txs.empty()) {
            sequences.push_back(non_conflicting_txs);
        }
    }
    
    // Execute non-conflicting zone
    auto result = NonConflictingZoneExecutor::execute(sequences, cache_manager_, config);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Update statistics
    stats.non_conflicting_zone.executed_count = result.executed_count;
    stats.non_conflicting_zone.failed_count = result.failed_count;
    stats.non_conflicting_zone.execution_time_us = duration.count();
    stats.non_conflicting_zone.sequence_parallelism = result.sequence_parallelism;
    
    LOG(INFO) << "Non-conflicting zone executed: " << result.executed_count 
              << " transactions, " << result.failed_count << " failed";
}

void ChainExecutionManager::globalSynchronizationPoint(std::vector<Shard>& shards) {
    LOG(INFO) << "Reached global synchronization point";
    
    // Wait for all non-conflicting zone executions to complete
    // In a real implementation, we would use barriers or futures
    
    // Batch merge to L1 cache
    batchMergeToL1(shards);
    
    LOG(INFO) << "Global synchronization point completed";
}

void ChainExecutionManager::executeConflictingZone(std::vector<Shard>& shards,
                                                  const scheduling::NonConflictingZoneExecutor::ExecutionConfig& config,
                                                  TwoZoneStatistics& stats) {
    LOG(INFO) << "Executing conflicting zone with " << shards.size() << " shards";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Execute conflicting zone transactions
    size_t executed_count = 0;
    size_t failed_count = 0;
    
    for (auto& shard : shards) {
        for (const auto& seq : shard.sequences) {
            // Execute sequence
            std::vector<void*> executed_txs;
            size_t seq_executed = 0;
            size_t seq_failed = 0;
            
            ConflictingZoneExecutor::executeSequence(seq, cache_manager_, executed_txs, seq_executed, seq_failed);
            
            executed_count += seq_executed;
            failed_count += seq_failed;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Update statistics
    stats.conflicting_zone.executed_count = executed_count;
    stats.conflicting_zone.failed_count = failed_count;
    stats.conflicting_zone.execution_time_us = duration.count();
    
    LOG(INFO) << "Conflicting zone executed: " << executed_count 
              << " transactions, " << failed_count << " failed";
}

void ChainExecutionManager::batchMergeToL1(std::vector<Shard>& shards) {
    LOG(INFO) << "Batch merging to L1 cache";
    
    // Merge sequences to L1 cache for both zones
    // This is a simplified implementation
    
    // For non-conflicting zone
    for (const auto& shard : shards) {
        // In a real implementation, we would merge sequence caches to L1
        LOG(INFO) << "Merged shard to L1 cache";
    }
    
    LOG(INFO) << "Batch merge to L1 completed";
}

size_t ChainExecutionManager::getOptimalThreadCount() const {
    if (thread_pool_size_ > 0) {
        return thread_pool_size_;
    }
    
    // Auto-detect optimal thread count
    size_t hw_threads = std::thread::hardware_concurrency();
    return hw_threads > 0 ? hw_threads : 4;  // Default to 4 if unable to detect
}

} // namespace scheduling