//
// Enhanced Dependency Chain Extractor with Three-Stage Pipeline
// Implements the "Sorting → Conflict Resolution → Chain Aggregation" pipeline
// as described in the BLP detailed design document
//

#ifndef NEUBLOCKCHAIN_PIPELINE_DEPENDENCY_EXTRACTOR_H
#define NEUBLOCKCHAIN_PIPELINE_DEPENDENCY_EXTRACTOR_H

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include "kdg_node.h"
#include "dependency_chain.h"
// #include "executor/kdg/union_find_chain_manager.h"  // File not found

namespace scheduling {

// Forward declarations
class DependencyDetector;
class ChainLoadCalculator;

// Enhanced statistics for pipeline dependency extraction
struct PipelineChainStatistics {
    // Timing metrics for each stage
    uint64_t sorting_time_us = 0;              // Stage 1: Transaction sorting time
    uint64_t conflict_resolution_time_us = 0;  // Stage 2: Conflict resolution time
    uint64_t aggregation_time_us = 0;          // Stage 3: Chain aggregation time
    uint64_t total_extraction_time_us = 0;     // Total extraction time
    
    // Stage-specific counters
    size_t sorted_transactions = 0;            // Transactions successfully sorted
    size_t resolved_conflicts = 0;             // Conflicts resolved in stage 2
    size_t aborted_transactions = 0;           // Transactions marked as aborted
    
    // Chain counts after aggregation
    size_t non_conflicting_chains = 0;
    size_t conflicting_chains = 0;
    size_t total_chains = 0;
    
    // Chain characteristics
    size_t max_chain_length = 0;
    size_t min_chain_length = UINT64_MAX;
    double avg_chain_length = 0.0;
    
    // Load distribution
    uint64_t total_gas = 0;
    uint64_t max_chain_load = 0;
    uint64_t min_chain_load = UINT64_MAX;
    double avg_chain_load = 0.0;
    
    // Parallelism metrics
    size_t num_threads_used = 0;
    double speedup = 0.0;
    double parallel_efficiency = 0.0;
    bool parallel_enabled = false;
    
    // Reset all statistics
    void reset() {
        sorting_time_us = 0;
        conflict_resolution_time_us = 0;
        aggregation_time_us = 0;
        total_extraction_time_us = 0;
        
        sorted_transactions = 0;
        resolved_conflicts = 0;
        aborted_transactions = 0;
        
        non_conflicting_chains = 0;
        conflicting_chains = 0;
        total_chains = 0;
        
        max_chain_length = 0;
        min_chain_length = UINT64_MAX;
        avg_chain_length = 0.0;
        
        total_gas = 0;
        max_chain_load = 0;
        min_chain_load = UINT64_MAX;
        avg_chain_load = 0.0;
        
        num_threads_used = 0;
        speedup = 0.0;
        parallel_efficiency = 0.0;
        parallel_enabled = false;
    }
};

// Result of pipeline dependency chain extraction
struct PipelineChainResult {
    std::vector<DependencyChain> non_conflicting_chains;
    std::vector<DependencyChain> conflicting_chains;
    PipelineChainStatistics statistics;
    
    // Get all chains combined
    std::vector<DependencyChain> getAllChains() const {
        std::vector<DependencyChain> all_chains;
        all_chains.reserve(non_conflicting_chains.size() + conflicting_chains.size());
        all_chains.insert(all_chains.end(), 
                         non_conflicting_chains.begin(), 
                         non_conflicting_chains.end());
        all_chains.insert(all_chains.end(), 
                         conflicting_chains.begin(), 
                         conflicting_chains.end());
        return all_chains;
    }
    
    size_t totalChainCount() const {
        return non_conflicting_chains.size() + conflicting_chains.size();
    }
};

/**
 * PipelineDependencyExtractor: Enhanced dependency chain extractor
 * Implements the three-stage pipeline as described in BLP design document:
 * 
 * Stage 1: Transaction Sorting
 * ===========================
 * Assigns logical sequence indices to transactions and their internal access operations
 * based on the KDG's topological structure to characterize partial order constraints.
 * 
 * Two layers:
 * 1. Address-level sorting: Topological sort of key/address nodes in KDG for global processing order
 * 2. Transaction internal sorting: Assign local indices to read/write units within addresses
 *    - Read operations get lower indices for visibility
 *    - Write operations depending on reads get indices >= max(read_indices) for causality
 * 
 * Stage 2: Conflict Resolution
 * ===========================
 * Resolves WAW dependencies when multiple transactions compete for the same key's write permission
 * using a deterministic conflict resolution mechanism based on static timestamps:
 * 
 * - Consensus-layer transaction IDs as monotonically increasing logical timestamps
 * - For multiple writes to the same key, only the one with minimum timestamp is valid
 * - Others are marked as "abort" and reorganized in Stage 3
 * 
 * Stage 3: Chain Aggregation
 * =========================
 * Constructs structured linear scheduling units from KDG's topological relationships:
 * 
 * Non-conflicting Chains:
 * - For transactions with execution rights after conflict resolution
 * - Based on sequence indices from sorting stage
 * - Same sequence index transactions are logically parallel → independent chain heads
 * - Dependent transactions linked to chain ends to extend chains
 * 
 * Conflicting Chains:
 * - For "abort"-marked transactions using lightweight incremental dependency reorganization
 * - Algorithm steps:
 *   1) Process isolated transactions in original consensus order (TX ID ascending)
 *   2) Maintain conflict chain set L_conflict, each chain tracks only tail transaction's RW set
 *   3) For each aborted transaction T_j, compare RW set with each chain tail T_tail:
 *      RW(T_j) ∩ RW(T_tail) ≠ ∅
 *   4) If intersection exists, append T_j to chain tail and update chain tail's RW set
 *      Otherwise create new conflict chain
 * 
 * Benefits:
 * - Deterministic, low complexity, parallel-safe
 * - Each transaction uniquely belongs to one dependency chain
 * - Final structured dependency chain collection: L = {L_1, L_2, ..., L_l}
 */
class PipelineDependencyExtractor {
public:
    // Constructor
    explicit PipelineDependencyExtractor(bool enable_parallel = true, 
                                      int num_threads = 0);
    
    // Extract dependency chains using the three-stage pipeline
    PipelineChainResult extractChains(
        const std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list,
        const std::vector<std::shared_ptr<TransactionNode>>& aborted_txs);
    
    // Get statistics
    const PipelineChainStatistics& getStatistics() const { return statistics_; }
    
    // Reset statistics
    void resetStatistics() { statistics_.reset(); }
    
    // Enable/disable parallel processing
    void setParallelEnabled(bool enabled) { enable_parallel_ = enabled; }
    bool isParallelEnabled() const { return enable_parallel_; }
    
    // Set number of threads
    void setNumThreads(int num_threads) { num_threads_ = num_threads; }
    int getNumThreads() const { return num_threads_; }

private:
    // Stage 1: Transaction Sorting
    // Group transactions by sequence number after hierarchical sorting
    std::map<uint32_t, std::vector<std::shared_ptr<TransactionNode>>> 
        groupBySequence(const std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list);
    
    // Validate sequence numbers
    bool validateSequenceNumbers(
        const std::map<uint32_t, std::vector<std::shared_ptr<TransactionNode>>>& grouped_txs);
    
    // Stage 2: Conflict Resolution
    // Resolve write-write conflicts using First-Committer-Wins strategy
    std::vector<std::shared_ptr<TransactionNode>> resolveConflicts(
        const std::vector<std::shared_ptr<TransactionNode>>& aborted_txs);
    
    // Stage 3: Chain Aggregation
    // Extract dependency chains from non-conflicting zone
    std::vector<DependencyChain> aggregateNonConflictingChains(
        const std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list);
    
    // Extract dependency chains from conflicting zone
    std::vector<DependencyChain> aggregateConflictingChains(
        const std::vector<std::shared_ptr<TransactionNode>>& aborted_txs);
    
    // Serial chain extension for non-conflicting zone
    void serialExtendChains(
        const std::vector<std::shared_ptr<TransactionNode>>& current_seq_txs,
        const std::vector<std::shared_ptr<TransactionNode>>& next_seq_txs,
        UnionFindChainManager& chain_manager);
    
    // Parallel chain extension for non-conflicting zone
    void parallelExtendChains(
        const std::vector<std::shared_ptr<TransactionNode>>& current_seq_txs,
        const std::vector<std::shared_ptr<TransactionNode>>& next_seq_txs,
        UnionFindChainManager& chain_manager);
    
    // Build dependency chains from union-find structure
    std::vector<DependencyChain> buildChainsFromUnionFind(
        const std::map<uint32_t, std::vector<std::shared_ptr<TransactionNode>>>& grouped_txs,
        UnionFindChainManager& chain_manager);
    
    // Calculate statistics for extracted chains
    void calculateStatistics(
        const std::vector<DependencyChain>& chains);
    
    bool enable_parallel_;
    int num_threads_;
    PipelineChainStatistics statistics_;
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_PIPELINE_DEPENDENCY_EXTRACTOR_H