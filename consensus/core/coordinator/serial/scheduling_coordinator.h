#ifndef NEUBLOCKCHAIN_SCHEDULING_COORDINATOR_H
#define NEUBLOCKCHAIN_SCHEDULING_COORDINATOR_H

#include "coordinator.h"
#include "utilities/types/aria_types.h"
#include "../../../../executor/address_based_conflict_graph.h"
#include "../../../../executor/kdg/two_zone_types.h"
#include "../../../../executor/impl/simulation_executor.h"
#include "../../../../executor/kdg/shard_manager.h"
#include "../../../../executor/kdg/shard.h"
#include "../../../../executor/kdg/dependency_chain.h"
#include "../../../../executor/kdg/kdg_node.h"
#include "../../../storage/core/cache/state_cache_manager.h"
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdint>

class Transaction;

namespace scheduling {

// Statistics reporting
struct SchedulingStats {
    // Transaction counts
    uint64_t total_simulated;
    uint64_t total_scheduled;
    uint64_t total_aborted;
    uint64_t total_reordered;
    
    // Zone statistics
    uint64_t non_conflicting_zone_txs;  // Transactions in non-conflicting zone
    uint64_t conflicting_zone_txs;      // Transactions in conflicting zone
    
    // Timing breakdown (microseconds)
    uint64_t simulation_time_us;
    uint64_t kdg_build_time_us;
    uint64_t scheduling_time_us;
    uint64_t execution_time_us;
    uint64_t commit_time_us;
    
    // Throughput metrics
    uint64_t total_batches;             // Total batches processed (replaces total_epochs)
    uint64_t total_execution_time_us;
    double avg_throughput_tps;          // transactions per second
    
    // Rates
    double abort_rate;
    double reorder_rate;
    
    // Sharding statistics (Requirement 12.3)
    bool sharding_enabled;
    uint64_t total_sharding_operations;
    uint64_t sharding_time_us;
    uint64_t optimization_time_us;
    uint32_t avg_num_shards;
    double avg_load_balance_improvement;
    uint32_t avg_optimization_iterations;
    
    // Default constructor
    SchedulingStats() 
        : total_simulated(0), total_scheduled(0), total_aborted(0), total_reordered(0),
          non_conflicting_zone_txs(0), conflicting_zone_txs(0),
          simulation_time_us(0), kdg_build_time_us(0), scheduling_time_us(0),
          execution_time_us(0), commit_time_us(0),
          total_batches(0), total_execution_time_us(0), avg_throughput_tps(0.0),
          abort_rate(0.0), reorder_rate(0.0),
          sharding_enabled(false), total_sharding_operations(0),
          sharding_time_us(0), optimization_time_us(0),
          avg_num_shards(0), avg_load_balance_improvement(0.0),
          avg_optimization_iterations(0) {}
};

class SchedulingCoordinator : public consensus::Coordinator {
public:
    explicit SchedulingCoordinator(epoch_size_t startupEpoch);
    virtual ~SchedulingCoordinator();
    
    // Override main execution loop
    void run();
    
    // Add transactions (same interface as ARIA)
    size_t addTransaction(std::unique_ptr<std::vector<::Transaction*>> trWrapper);
    
    // Stop the coordinator
    void stop();
    
protected:
    // Execution pipeline
    void executeSchedulingPipeline();
    
    // Phase 1: Simulation and scheduling
    void phaseOneExecution(std::vector<::Transaction*>& transactions,
                          blp::StateCacheManager* cache_manager = nullptr);
    
    // Phase 2: Re-execute aborted transactions in conflicting zone
    void phaseTwoExecution(const std::vector<AbortedTransaction>& aborted_txs);
    
    // Phase 2 with dependency chain information (Task 10.4)
    // Requirement 5.3, 5.4: Use chain information for sequence assignment
    void phaseTwoExecutionWithChains(
        const std::vector<AbortedTransaction>& aborted_txs,
        const std::vector<DependencyChain>& conflicting_chains);
    
    // Simulate transactions to extract RW sets
    std::vector<SimulatedTransaction> simulateTransactions(
        const std::vector<::Transaction*>& transactions);
    
    // Build KDG and schedule
    ScheduledInfo buildKdgAndSchedule(
        const std::vector<SimulatedTransaction>& sim_txs);
    
    // Perform sharding (Requirement 12.1, 12.3, 14.4)
    bool performSharding(ScheduledInfo& schedule_info);
    
    // Convert shards to scheduled info (Requirement 12.1)
    void convertShardsToScheduledInfo(
        const std::vector<Shard>& shards,
        ScheduledInfo& schedule_info);
    
    // Execute scheduled transactions
    void executeScheduled(
        const std::vector<std::vector<FinalizedTransaction>>& scheduled_txs,
        blp::StateCacheManager* cache_manager = nullptr);
    
    // Execute transactions using shard-based parallel execution
    // Requirement 12.1: Execute shards in parallel
    void executeWithSharding(
        const std::vector<Shard>& shards,
        blp::StateCacheManager* cache_manager = nullptr);
    
    // Shard execution result structure (Task 1.2)
    struct ShardExecutionResult {
        uint32_t shard_id;
        size_t executed_count;
        size_t failed_count;
        uint64_t execution_time_us;
        
        // Zone-specific results
        size_t nc_zone_executed;
        size_t c_zone_executed;
    };
    
    // Execute single shard helper method (Task 1.2)
    ShardExecutionResult executeSingleShard(
        const Shard& shard,
        blp::StateCacheManager* cache_manager);
    
    // Validate optimistic assumptions
    bool validateOptimisticAssumption(
        const std::vector<::Transaction*>& re_executed_txs,
        std::vector<::Transaction*>& valid_txs,
        std::vector<::Transaction*>& invalid_txs);
    
    // Commit transactions
    void commitTransactions(const std::vector<::Transaction*>& transactions);
    
    // Get transactions from buffer
    std::vector<::Transaction*> getTransactionsFromBuffer();
    
    // Execute transaction (real execution, not simulation)
    bool executeTransactionReal(::Transaction* tx, class VersionedDB* db);
    
    // Execute transaction with cache support
    bool executeTransactionWithCache(::Transaction* tx,
                                     blp::StateCacheManager* cache_manager);
    
    SchedulingStats getStats() const;
    void printStats() const;
    
    // Sharding configuration methods (Requirement 15.1, 15.2, 15.3, 15.4)
    void setShardingEnabled(bool enabled) { enable_sharding_ = enabled; }
    bool isShardingEnabled() const { return enable_sharding_; }
    
    void setNumShards(uint32_t num_shards) { num_shards_ = num_shards; }
    uint32_t getNumShards() const { return num_shards_; }
    
    void setOptimizationThreshold(double threshold) { optimization_threshold_ = threshold; }
    double getOptimizationThreshold() const { return optimization_threshold_; }
    
    void setMaxOptimizationIterations(uint32_t max_iterations) { 
        max_optimization_iterations_ = max_iterations; 
    }
    uint32_t getMaxOptimizationIterations() const { return max_optimization_iterations_; }
    
private:
    // Simulation executor
    std::unique_ptr<SimulationExecutor> simulation_executor_;
    
    // Statistics
    std::atomic<uint64_t> total_simulated_{0};
    std::atomic<uint64_t> total_scheduled_{0};
    std::atomic<uint64_t> total_aborted_{0};
    std::atomic<uint64_t> total_reordered_{0};
    
    // Configuration
    bool enable_parallel_simulation_;
    bool enable_parallel_kdg_;
    bool enable_early_abort_;
    bool enable_reordering_;
    bool enable_dependency_chains_;  // Enable dependency chain extraction
    int simulation_threads_;
    int kdg_threads_;
    
    // Sharding configuration (Requirement 15.1, 15.2, 15.3, 15.4)
    bool enable_sharding_;                  // Enable/disable sharding
    uint32_t num_shards_;                   // Number of shards (k)
    double optimization_threshold_;         // Optimization threshold (ε)
    uint32_t max_optimization_iterations_;  // Maximum optimization iterations
    
    // Two-zone execution configuration (Requirement 9.1, 9.2, 9.3, 9.4)
    bool enable_two_zone_optimization_;     // Enable/disable two-zone execution
    int nc_zone_max_parallelism_;           // Non-conflicting zone max parallelism
    bool c_zone_enable_binary_search_;      // Enable binary search for conflicting zone
    bool enable_strict_validation_;         // Enable strict optimistic validation
    
    // Performance tracking
    std::atomic<uint64_t> simulation_time_us_{0};
    std::atomic<uint64_t> kdg_build_time_us_{0};
    std::atomic<uint64_t> scheduling_time_us_{0};
    std::atomic<uint64_t> execution_time_us_{0};
    std::atomic<uint64_t> commit_time_us_{0};
    
    // Throughput metrics
    std::atomic<uint64_t> total_batches_{0};  // Replaces total_epochs_
    std::atomic<uint64_t> total_execution_time_us_{0};
    
    // Zone statistics tracking
    std::atomic<uint64_t> non_conflicting_zone_txs_{0};
    std::atomic<uint64_t> conflicting_zone_txs_{0};
    
    // Sharding statistics tracking (Requirement 12.3)
    std::atomic<uint64_t> total_sharding_operations_{0};
    std::atomic<uint64_t> sharding_time_us_{0};
    std::atomic<uint64_t> optimization_time_us_{0};
    std::atomic<uint64_t> total_shards_used_{0};
    double total_load_balance_improvement_{0.0};
    std::atomic<uint64_t> total_optimization_iterations_{0};
    
    // Two-zone execution statistics (Requirement 8.1, 8.2, 8.3, 8.6)
    TwoZoneStatistics two_zone_statistics_;
    
    // Inherited from Coordinator
    epoch_size_t currentEpoch;
    volatile bool finishSignal;
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_SCHEDULING_COORDINATOR_H