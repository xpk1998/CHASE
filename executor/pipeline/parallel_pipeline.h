//
// Created for Block Level Pipelining with Parallel Execution
// Parallel Pipeline Implementation based on Intel TBB
//

#ifndef PARACHAIN_PARALLEL_PIPELINE_H
#define PARACHAIN_PARALLEL_PIPELINE_H

#include <vector>
#include <memory>
#include <functional>
// 使用标准线程库替代TBB
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <queue>
#include "../../utilities/types/aria_types.h"
#include "../../storage/core/cache/enhanced_state_cache_manager.h"
#include "../kdg/address_based_conflict_graph.h"  // 添加KDG支持

namespace pipeline {

// Forward declarations
class ExecutionContext;

// Pipeline stage types
enum class StageType {
    SIMULATION,      // Transaction simulation to extract RW sets
    SCHEDULING,      // Build KDG and schedule transactions (集成KDG技术)
    EXECUTION,       // Unified execution stage (取代原来的EXECUTION_NC和EXECUTION_C)
    COMMIT           // Commit transactions
};

// Transaction wrapper for pipeline processing
struct PipelineTransaction {
    void* raw_transaction;  // Raw transaction pointer
    std::string rw_set;     // Read-write set extracted during simulation
    uint32_t sequence_id;   // Sequence ID for scheduling
    bool is_conflicting;    // Whether transaction is in conflicting zone
    bool is_aborted;        // Whether transaction is aborted
    
    // KDG相关字段
    std::set<std::string> read_keys;   // 读取的键集合
    std::set<std::string> write_keys;  // 写入的键集合
    std::set<uint32_t> dependencies;   // 依赖的交易ID集合
};

// Pipeline filter types
using InputFilter = std::function<std::vector<PipelineTransaction>(epoch_size_t)>;
using ProcessingFilter = std::function<void(std::vector<PipelineTransaction>&, blp::StateCacheManager*)>;
using OutputFilter = std::function<void(const std::vector<PipelineTransaction>&)>;

/**
 * ParallelPipeline: Multi-stage parallel pipeline executor
 * 
 * Implements a 5-stage pipeline based on FISCO-BCOS design:
 * 1. Transaction input and preprocessing
 * 2. Simulation stage (extract RW sets)
 * 3. Scheduling stage (build KDG and schedule, 集成KDG技术)
 * 4. Execution stage (execute transactions in parallel)
 * 5. Commit stage (commit results)
 * 
 * Uses Intel TBB parallel_pipeline for efficient parallel execution
 */
class ParallelPipeline {
public:
    /**
     * Constructor
     * @param num_threads Number of threads to use (0 = auto)
     */
    explicit ParallelPipeline(size_t num_threads = 0);
    
    ~ParallelPipeline() = default;
    
    // Delete copy and move constructors
    ParallelPipeline(const ParallelPipeline&) = delete;
    ParallelPipeline& operator=(const ParallelPipeline&) = delete;
    ParallelPipeline(ParallelPipeline&&) = delete;
    ParallelPipeline& operator=(ParallelPipeline&&) = delete;
    
    /**
     * Set input filter for pipeline
     * @param filter Function to fetch transactions for an epoch
     */
    void setInputFilter(const InputFilter& filter);
    
    /**
     * Set processing filters for pipeline stages
     * @param stage Stage type
     * @param filter Processing filter function
     */
    void setProcessingFilter(StageType stage, const ProcessingFilter& filter);
    
    /**
     * Set output filter for pipeline
     * @param filter Function to handle completed transactions
     */
    void setOutputFilter(const OutputFilter& filter);
    
    /**
     * Execute pipeline for a specific epoch
     * @param epoch Epoch number to process
     * @param cache_manager State cache manager for BLP
     */
    void execute(epoch_size_t epoch, blp::StateCacheManager* cache_manager = nullptr);
    
    /**
     * Get pipeline statistics
     */
    struct Stats {
        size_t total_transactions;
        size_t processed_transactions;
        uint64_t total_time_us;
        double throughput;  // transactions per second
        
        Stats() : total_transactions(0), processed_transactions(0), 
                  total_time_us(0), throughput(0.0) {}
    };
    
    Stats getStats() const;
    
    /**
     * Reset pipeline statistics
     */
    void resetStats();

private:
    // Pipeline configuration
    size_t num_threads_;
    size_t thread_count_;
    
    // Pipeline filters
    InputFilter input_filter_;
    std::map<StageType, ProcessingFilter> processing_filters_;
    OutputFilter output_filter_;
    
    // Statistics
    mutable Stats stats_;
    
    // KDG支持
    std::unique_ptr<scheduling::AddressBasedConflictGraph> conflict_graph_;
    
    // Helper methods
    void executePipeline(epoch_size_t epoch, blp::StateCacheManager* cache_manager);
    
    // KDG相关方法
    void buildConflictGraph(const std::vector<PipelineTransaction>& transactions);
    void detectDependencies(std::vector<PipelineTransaction>& transactions);
};

/**
 * ExecutionContext: Context for transaction execution within pipeline
 * 
 * Holds execution state and results for a batch of transactions
 */
class ExecutionContext {
public:
    explicit ExecutionContext(epoch_size_t epoch);
    ~ExecutionContext() = default;
    
    // Delete copy and move constructors
    ExecutionContext(const ExecutionContext&) = delete;
    ExecutionContext& operator=(const ExecutionContext&) = delete;
    ExecutionContext(ExecutionContext&&) = delete;
    ExecutionContext& operator=(ExecutionContext&&) = delete;
    
    // Getters and setters
    epoch_size_t getEpoch() const { return epoch_; }
    
    const std::vector<PipelineTransaction>& getTransactions() const { return transactions_; }
    void setTransactions(std::vector<PipelineTransaction>&& transactions) { 
        transactions_ = std::move(transactions); 
    }
    
    blp::StateCacheManager* getCacheManager() const { return cache_manager_; }
    void setCacheManager(blp::StateCacheManager* cache_manager) { cache_manager_ = cache_manager; }
    
    size_t getProcessedCount() const { return processed_count_; }
    void incrementProcessedCount(size_t count = 1) { processed_count_ += count; }
    
private:
    epoch_size_t epoch_;
    std::vector<PipelineTransaction> transactions_;
    blp::StateCacheManager* cache_manager_;
    size_t processed_count_;
};

} // namespace pipeline

#endif // PARACHAIN_PARALLEL_PIPELINE_H