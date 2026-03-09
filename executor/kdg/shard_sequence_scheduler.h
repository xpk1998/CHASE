//
// Created for Shard Sequence Scheduling
// ShardSequenceScheduler: Schedules transactions within a shard into sequences
// Implements precise ordering and layering to ensure serializability
//

#ifndef NEUBLOCKCHAIN_SHARD_SEQUENCE_SCHEDULER_H
#define NEUBLOCKCHAIN_SHARD_SEQUENCE_SCHEDULER_H

#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include "shard.h"
//#include "dependency_chain.h"
#include "../transaction/transaction.h"

namespace scheduling {

/**
 * ShardSequenceScheduler: Schedules transactions within a shard into sequences
 * 
 * ========================================================================
 * SCHEDULING OBJECTIVE
 * ========================================================================
 * 
 * Goal: Assign transactions to sequences within each shard to:
 * 1. Ensure serializability constraints are satisfied
 * 2. Maximize parallel execution efficiency
 * 3. Maintain execution correctness
 * 
 * ========================================================================
 * NON-CONFLICTING ZONE TRANSACTION ORDERING
 * ========================================================================
 * 
 * Strategy: Layer transactions by dependency depth
 * 
 * Sequence 1 (Initial Layer):
 *   - All first transactions from each dependency chain (chain heads)
 *   - All independent transactions (non-chain transactions)
 *   - These transactions have no dependencies, can execute in parallel
 * 
 * Sequence 2 (Second Layer):
 *   - All second transactions from each dependency chain
 *   - Each depends on its chain's first transaction
 *   - Can execute in parallel across different chains
 * 
 * Sequence 3 (Third Layer):
 *   - All third transactions from each dependency chain
 *   - Pattern continues...
 * 
 * General Rule:
 *   Sequence N contains all N-th transactions from dependency chains
 * 
 * Concurrency Metrics:
 *   - Concurrency Width = Number of independent chains in shard
 *     (number of transactions that can execute in parallel per sequence)
 *   - Execution Depth = Length of longest dependency chain
 *     (number of sequential sequences needed)
 * 
 * Objective:
 *   Under ideal network and compute conditions, achieve maximum
 *   parallel execution efficiency
 * 
 * ========================================================================
 * CONFLICTING ZONE TRANSACTION ORDERING
 * ========================================================================
 * 
 * Trigger Timing:
 *   - Starts AFTER global synchronization point completes
 *   - Processes aborted transactions reconstructed into conflicting chains
 * 
 * Processing Objects:
 *   - Conflicting dependency chains (from inter-epoch reordering)
 * 
 * Ordering Strategy:
 *   - Each conflicting chain MUST be serialized into consecutive sequences
 *   - Chain 1: sequences [1, 2, ..., len(chain_1)]
 *   - Chain 2: sequences [len(chain_1)+1, ..., len(chain_1)+len(chain_2)]
 *   - Maintains original execution order within each chain
 * 
 * Parallel Optimization:
 *   - Different conflicting chains can execute in parallel
 *   - But ALL conflicting zone transactions execute AFTER
 *     ALL non-conflicting zone transactions complete
 * 
 * Correctness Guarantee:
 *   - Strict execution timing control ensures global serializability
 * 
 * ========================================================================
 * DATA STRUCTURES
 * ========================================================================
 * 
 * Sequence: Vector of transactions to execute in parallel
 * SequenceMap: Map from sequence number to transactions
 * 
 * Non-Conflicting Zone:
 *   sequence_num → [tx_1_at_depth_N, tx_2_at_depth_N, ..., independent_txs]
 * 
 * Conflicting Zone:
 *   sequence_num → [tx_from_chain_i]
 *   (each sequence contains transactions from same or different chains)
 * 
 * ========================================================================
 */
class ShardSequenceScheduler {
public:
    // Sequence: transactions to execute in parallel
    using Sequence = std::vector<std::shared_ptr<TransactionNode>>;
    
    // SequenceMap: map from sequence number to transactions
    using SequenceMap = std::map<uint32_t, Sequence>;
    
    // Scheduling result
    struct SchedulingResult {
        SequenceMap non_conflicting_sequences;  // NC zone sequences
        SequenceMap conflicting_sequences;      // C zone sequences
        
        // Concurrency metrics (NC zone)
        uint32_t concurrency_width;   // Number of independent chains
        uint32_t execution_depth;     // Longest chain length
        
        // Sequence counts
        uint32_t num_nc_sequences;    // Total NC sequences
        uint32_t num_c_sequences;     // Total C sequences
        
        // Transaction counts
        uint32_t num_nc_transactions; // Total NC transactions
        uint32_t num_c_transactions;  // Total C transactions
        
        bool success;
        std::string error_message;
        
        SchedulingResult() 
            : concurrency_width(0), execution_depth(0),
              num_nc_sequences(0), num_c_sequences(0),
              num_nc_transactions(0), num_c_transactions(0),
              success(false) {}
    };
    
    // Constructor
    explicit ShardSequenceScheduler(const Shard& shard);
    
    // Perform sequence scheduling for the entire shard
    // Returns scheduling result with both NC and C zone sequences
    SchedulingResult scheduleSequences();
    
    // Schedule non-conflicting zone transactions
    // Input: NC chains and independent transactions
    // Output: Layered sequences (by dependency depth)
    SequenceMap scheduleNonConflictingZone(
        const std::vector<DependencyChain>& nc_chains,
        const std::vector<std::shared_ptr<TransactionNode>>& independent_txs);
    
    // Schedule conflicting zone transactions
    // Input: Conflicting chains
    // Output: Serialized sequences (consecutive per chain)
    SequenceMap scheduleConflictingZone(
        const std::vector<DependencyChain>& c_chains);

private:
    const Shard& shard_;  // Reference to the shard being scheduled
    
    // Calculate concurrency metrics
    void calculateConcurrencyMetrics(
        const std::vector<DependencyChain>& nc_chains,
        uint32_t& width, uint32_t& depth);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_SHARD_SEQUENCE_SCHEDULER_H
