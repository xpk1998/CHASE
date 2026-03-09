#ifndef NEUBLOCKCHAIN_NON_CONFLICTING_ZONE_EXECUTOR_H
#define NEUBLOCKCHAIN_NON_CONFLICTING_ZONE_EXECUTOR_H

#include "two_zone_types.h"
#include "address_based_conflict_graph.h"
#include "../../storage/core/cache/state_cache_manager.h"
#include <vector>
#include <future>
#include <thread>
#include <chrono>
#include <glog/logging.h>

// 添加HashSet的定义
template<typename T>
using HashSet = std::unordered_set<T>;

namespace scheduling {

// Forward declaration
class TransactionNode;

// ============================================================================
// NonConflictingZoneExecutor: Executes non-conflicting zone transactions in parallel
// 
// EXECUTION STRATEGY:
// - Transactions execute in PARALLEL by sequence
// - NO VALIDATION required (based on valid snapshot under no-conflict assumption)
// - Direct commitment after execution
// 
// Process Flow:
// 1. Group transactions into sequences (by dependency depth)
// 2. Execute all transactions within each sequence in PARALLEL
// 3. Execute all sequences in PARALLEL (full parallelism)
// 4. Directly commit results (no validation needed)
// 
// Correctness Guarantee:
// - Zone is constructed under no-conflict assumption (valid snapshot)
// - All dependencies resolved through sequence ordering
// - Write sets are guaranteed to be disjoint within each sequence
// 
// Requirements:
// - Requirement 2.2: Execute all sequences in parallel using thread pool
// - Requirement 2.3: Within each sequence, execute all transactions in parallel
// - Requirement 2.4: Record parallelism metrics
// - Requirement 2.5: Handle execution errors gracefully
// - Requirement 10.4: Integrate with StateCacheManager
// ============================================================================

class NonConflictingZoneExecutor {
public:
    // Execution configuration
    struct ExecutionConfig {
        size_t max_parallelism;  // Maximum parallel threads (0 = auto)
        bool enable_sharding;     // Enable shard-based execution
        size_t num_shards;        // Number of shards (if enabled)
        
        ExecutionConfig() 
            : max_parallelism(0),  // 0 = auto (use all available cores)
              enable_sharding(false),
              num_shards(4) {}
    };
    
    // Execution result
    struct ExecutionResult {
        size_t executed_count;
        size_t failed_count;
        uint64_t execution_time_us;
        std::vector<double> sequence_parallelism;  // Parallelism per sequence
        
        // Sharding statistics (Task 11.3, Requirement 7.5)
        size_t shard_count;                        // Number of shards used
        std::vector<double> shard_parallelism;     // Parallelism per shard
        double avg_shard_parallelism;              // Average shard-level parallelism
        
        ExecutionResult() 
            : executed_count(0), failed_count(0), execution_time_us(0),
              shard_count(0), avg_shard_parallelism(0.0) {}
    };
    
    // Execute non-conflicting zone with full parallelism
    // Requirement 2.2: Execute all sequences in parallel using thread pool
    // Requirement 2.3: Within each sequence, execute all transactions in parallel
    // Requirement 2.4: Collect execution results and statistics
    // Requirement 2.5: Handle execution errors gracefully
    // Requirement 10.4: Execute transactions using cache-aware execution
    static ExecutionResult execute(
        const std::vector<std::vector<FinalizedTransaction>>& sequences,
        blp::StateCacheManager* cache_manager,
        const ExecutionConfig& config);
    
private:
    // Shard structure for grouping transactions
    // Requirement 7.1: Apply sharding within non-conflicting zone sequences
    // Requirement 7.2: Ensure same shard transactions have disjoint write sets
    struct Shard {
        std::vector<FinalizedTransaction> transactions;
        HashSet<std::string> write_set;  // Union of all write sets in shard
        
        Shard() = default;
        
        // Check if transaction can be added to this shard (no write conflicts)
        bool canAddTransaction(const FinalizedTransaction& tx) const {
            for (const auto& key : tx.write_keys) {
                if (write_set.count(key) > 0) {
                    return false;
                }
            }
            return true;
        }
        
        // Add transaction to shard
        void addTransaction(const FinalizedTransaction& tx) {
            transactions.push_back(tx);
            for (const auto& key : tx.write_keys) {
                write_set.insert(key);
            }
        }
    };
    
    // Execute one sequence in parallel
    // Requirement 2.2: Launch async tasks for each transaction in sequence
    // Requirement 2.3: Use std::async or thread pool for parallel execution
    // Requirement 2.3: Wait for all transactions to complete
    // Requirement 2.3: Collect committed transactions
    static void executeSequenceParallel(
        const std::vector<FinalizedTransaction>& sequence,
        blp::StateCacheManager* cache_manager,
        size_t& executed_count,
        size_t& failed_count,
        double& parallelism);
    
    // Execute one sequence with sharding support
    // Requirement 7.1: Check if sharding is enabled in configuration
    // Requirement 7.3: Execute shards in parallel
    static void executeSequenceWithSharding(
        const std::vector<FinalizedTransaction>& sequence,
        blp::StateCacheManager* cache_manager,
        size_t num_shards,
        size_t& executed_count,
        size_t& failed_count,
        double& parallelism,
        std::vector<double>& shard_parallelism);
    
    // Create shards from sequence transactions
    // Requirement 7.1: Apply sharding within non-conflicting zone sequences
    // Requirement 7.2: Ensure same shard transactions have disjoint write sets
    static std::vector<Shard> createShards(
        const std::vector<FinalizedTransaction>& sequence,
        size_t num_shards);
    
    // Execute a single shard in parallel
    // Requirement 7.3: Execute shards in parallel
    static void executeShardParallel(
        const Shard& shard,
        blp::StateCacheManager* cache_manager,
        size_t& executed_count,
        size_t& failed_count,
        double& parallelism);
    
    // Execute a single transaction
    // Requirement 10.4: Execute transactions using cache-aware execution
    // Requirement 10.4: Read from StateCacheManager (UncommittedCache + CommittedState)
    // Requirement 10.4: Write to SequenceCache
    static bool executeSingleTransaction(
        const FinalizedTransaction& tx,
        blp::SequenceCache* sequence_cache,
        blp::StateCacheManager* cache_manager);
    
    // Calculate parallelism metrics
    // Requirement 2.4: Record average parallelism (concurrent transactions)
    // Requirement 2.4: Record maximum parallelism
    // Requirement 2.4: Record per-sequence parallelism
    static double calculateParallelism(
        size_t tx_count,
        uint64_t execution_time_us);
    
    // Get optimal thread count
    static size_t getOptimalThreadCount(size_t max_parallelism);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_NON_CONFLICTING_ZONE_EXECUTOR_H