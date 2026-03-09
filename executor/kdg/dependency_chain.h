#ifndef NEUBLOCKCHAIN_DEPENDENCY_CHAIN_H
#define NEUBLOCKCHAIN_DEPENDENCY_CHAIN_H

#include <vector>
#include <memory>
#include <cstdint>
#include <set>
#include <mutex>
#include "kdg_node.h"

namespace scheduling {

// Forward declaration
class DependencyChain;

// Chain statistics structure
struct ChainStatistics {
    // Timing metrics
    uint64_t extraction_time_us = 0;         // Total extraction time
    uint64_t non_conflicting_time_us = 0;    // Non-conflicting zone extraction time
    uint64_t conflicting_time_us = 0;        // Conflicting zone extraction time
    uint64_t load_calculation_time_us = 0;   // Load calculation time
    uint64_t validation_time_us = 0;         // Validation time
    
    // Chain counts
    size_t total_chains = 0;                 // Total chains
    size_t num_non_conflicting_chains = 0;   // Non-conflicting chains
    size_t num_conflicting_chains = 0;       // Conflicting chains
    
    // Chain characteristics
    size_t max_chain_length = 0;
    size_t min_chain_length = UINT64_MAX;
    double avg_chain_length = 0.0;
    double chain_length_std_dev = 0.0;
    
    // Load distribution
    uint64_t total_gas = 0;
    uint64_t max_chain_load = 0;
    uint64_t min_chain_load = UINT64_MAX;
    double avg_chain_load = 0.0;
    double chain_load_std_dev = 0.0;
    double load_balance_factor = 0.0;
    
    // Transaction distribution
    size_t num_transactions = 0;             // Total transactions
    size_t num_aborted_transactions = 0;     // Aborted transactions
    size_t num_independent_transactions = 0; // Independent transactions (chains of length 1)
    double avg_tx_per_chain = 0.0;
    
    // Parallelism metrics
    bool parallel_enabled = false;
    size_t num_threads_used = 0;
    double speedup = 0.0;
    double parallel_efficiency = 0.0;
    
    // Performance indicators
    double throughput_tx_per_sec = 0.0;
    
    // Reset all statistics
    void reset() {
        extraction_time_us = 0;
        non_conflicting_time_us = 0;
        conflicting_time_us = 0;
        load_calculation_time_us = 0;
        validation_time_us = 0;
        
        total_chains = 0;
        num_non_conflicting_chains = 0;
        num_conflicting_chains = 0;
        
        max_chain_length = 0;
        min_chain_length = UINT64_MAX;
        avg_chain_length = 0.0;
        chain_length_std_dev = 0.0;
        
        total_gas = 0;
        max_chain_load = 0;
        min_chain_load = UINT64_MAX;
        avg_chain_load = 0.0;
        chain_load_std_dev = 0.0;
        load_balance_factor = 0.0;
        
        num_transactions = 0;
        num_aborted_transactions = 0;
        num_independent_transactions = 0;
        avg_tx_per_chain = 0.0;
        
        parallel_enabled = false;
        num_threads_used = 0;
        speedup = 0.0;
        parallel_efficiency = 0.0;
        
        throughput_tx_per_sec = 0.0;
    }
    
    // Convert to string representation
    std::string toString() const;
    
    // Convert to JSON representation
    std::string toJSON() const;
    
    // Convert to summary string
    std::string toSummary() const;
};

// DependencyChainResult: Result of dependency chain extraction
struct DependencyChainResult {
    std::vector<DependencyChain> non_conflicting_chains;
    std::vector<DependencyChain> conflicting_chains;
    ChainStatistics statistics;
    
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
    
    // Get total chain count
    size_t totalChainCount() const {
        return non_conflicting_chains.size() + conflicting_chains.size();
    }
};

// DependencyChain: Represents a chain of dependent transactions
// Transactions in a chain must be executed sequentially in order
// Different chains can be executed in parallel
class DependencyChain {
public:
    // Constructor with chain ID
    explicit DependencyChain(uint64_t chain_id);
    
    // Add a transaction to the end of the chain
    // Requirement 7.2: Update chain tail and add to transaction list
    void appendTransaction(std::shared_ptr<TransactionNode> tx);
    
    // Merge another chain into this one
    // Requirement 7.5: Append all transactions from other chain
    void merge(DependencyChain&& other);
    
    // Get chain ID
    // Requirement 7.1: Return unique chain identifier
    uint64_t getChainId() const { return chain_id_; }
    
    // Get chain head (first transaction)
    // Requirement 7.1: Return the first transaction in the chain
    std::shared_ptr<TransactionNode> getHead() const;
    
    // Get chain tail (last transaction)
    // Requirement 7.1: Return the last transaction in the chain
    std::shared_ptr<TransactionNode> getTail() const;
    
    // Get all transactions in the chain
    // Requirement 7.1: Return complete transaction list
    const std::vector<std::shared_ptr<TransactionNode>>& getTransactions() const {
        return transactions_;
    }
    
    // Get chain length (number of transactions)
    // Requirement 7.3: Return transaction count
    size_t getTransactionCount() const { return transactions_.size(); }
    size_t length() const { return transactions_.size(); }
    
    // Check if chain is empty
    bool empty() const { return transactions_.empty(); }
    
    // Get chain load (total Gas consumption)
    // Requirement 7.4: Return cached Gas total
    uint64_t getTotalGas() const { return total_gas_; }
    uint64_t getLoad() const { return total_gas_; }
    
    // Set chain load
    // Requirement 7.4: Update Gas total
    void setLoad(uint64_t load) { total_gas_ = load; }
    
    // Get sequence number range [min, max]
    // Returns the minimum and maximum sequence numbers in the chain
    std::pair<uint32_t, uint32_t> getSequenceRange() const;
    
    // Validate chain correctness
    // Requirement 9.1, 9.2: Check sequence monotonicity and dependencies
    bool validate() const;
    
    // Validate with detailed error reporting
    struct ValidationResult {
        bool valid;
        std::string error_message;
        uint64_t error_tx_id;  // Transaction ID where error occurred
        
        ValidationResult() : valid(true), error_tx_id(0) {}
        ValidationResult(bool v, const std::string& msg, uint64_t tx_id = 0)
            : valid(v), error_message(msg), error_tx_id(tx_id) {}
    };
    
    ValidationResult validateDetailed() const;
    
    // Get chain statistics
    struct ChainStats {
        uint64_t chain_id;
        size_t length;
        uint64_t total_gas;
        uint32_t min_sequence;
        uint32_t max_sequence;
        uint64_t head_tx_id;
        uint64_t tail_tx_id;
    };
    
    ChainStats getStats() const;
    
    // Debug: Convert to string representation
    std::string toString() const;
    
    // ========================================================================
    // Sharding Support (Added for Dependency Chain Sharding Feature)
    // Requirements: 2.1, 4.3, 5.3
    // ========================================================================
    
    // Get/Set conflicting attribute
    // Requirement 2.1: Mark chain as conflicting or non-conflicting
    bool isConflicting() const { return is_conflicting_; }
    void setConflicting(bool conflicting) { is_conflicting_ = conflicting; }
    
    // Get/Set assigned shard ID
    // Requirement 4.3, 5.3: Track which shard the chain is assigned to
    uint32_t getAssignedShardId() const { return assigned_shard_id_; }
    void setAssignedShardId(uint32_t shard_id) { assigned_shard_id_ = shard_id; }

private:
    // Check if sequence numbers are monotonically increasing
    // Requirement 9.1: Validate sequence monotonicity
    bool checkSequenceMonotonicity() const;
    
    // Check if adjacent transactions have dependencies
    // Requirement 9.2: Validate dependency relationships
    bool checkDependencyRelationships() const;
    
    uint64_t chain_id_;                                      // Unique chain identifier
    std::vector<std::shared_ptr<TransactionNode>> transactions_; // Ordered list of transactions
    uint64_t total_gas_;                                     // Total Gas consumption (load)
    
    // Sharding fields (Added for Dependency Chain Sharding Feature)
    bool is_conflicting_;                                    // Whether chain is conflicting (Requirement 2.1)
    uint32_t assigned_shard_id_;                             // Assigned shard ID (Requirement 4.3, 5.3)
};

/**
 * UnionFindChainManager: Union-Find (Disjoint Set Union) for Chain Membership
 * 
 * Manages dependency chain membership using the Union-Find data structure.
 * Provides near-constant O(α(n)) amortized time complexity for operations.
 * 
 * Purpose:
 * - Track which chain each transaction belongs to
 * - Efficiently merge transactions into same chain when dependency detected
 * - Support parallel chain extension with thread-safe operations
 * 
 * Chain Merging Cases (Requirement 3.2, 3.3, 3.4):
 * 
 * Case 1: Both T_i and T_j unassigned to any chain
 *   → Create new chain, add both transactions
 *   → parent_[i] = parent_[j] = new_chain_id
 * 
 * Case 2: Only T_i assigned to chain C_a, T_j unassigned
 *   → Merge T_j into existing chain C_a
 *   → parent_[j] = find(i)
 * 
 * Case 3: Only T_j assigned to chain C_b, T_i unassigned
 *   → Merge T_i into existing chain C_b
 *   → parent_[i] = find(j)
 * 
 * Case 4: Both assigned to different chains C_a and C_b
 *   → Merge chains C_a and C_b into single chain
 *   → All transactions in both chains now in same chain
 *   → Uses union by rank for balanced tree
 * 
 * All dependent transactions recursively merged into same chain.
 * 
 * Optimizations:
 * - Path Compression (Requirement 3.5):
 *   During find(), flatten tree by making nodes point directly to root
 *   Reduces future find() operations to near O(1)
 * 
 * - Union by Rank:
 *   When merging, attach smaller tree under root of larger tree
 *   Keeps tree height logarithmic, ensures efficiency
 * 
 * Thread Safety (Requirement 3.4):
 * - Mutex protects all operations
 * - Supports concurrent chain extension from multiple threads
 * - Safe for parallel dependency chain extraction
 * 
 * Performance:
 * - Find: O(α(n)) amortized - nearly constant time
 * - Unite: O(α(n)) amortized
 * - Space: O(n) for n transactions
 * - Where α(n) is inverse Ackermann function (grows extremely slowly)
 */
class UnionFindChainManager {
public:
    // Constructor: Initialize union-find structure for given number of transactions
    // Requirement 3.1: Create independent set for each transaction
    explicit UnionFindChainManager(size_t num_transactions);
    
    // Find the chain ID that a transaction belongs to (with path compression)
    // Requirement 3.5: Use path compression to optimize find efficiency
    // Returns: The root chain ID
    uint64_t find(uint64_t tx_id);
    
    // Unite two transactions into the same chain (with union by rank)
    // Requirement 3.2, 3.3, 3.4: Handle four cases of chain merging
    // Returns: The chain ID after merging
    uint64_t unite(uint64_t tx_id1, uint64_t tx_id2);
    
    // Check if two transactions are in the same chain
    bool inSameChain(uint64_t tx_id1, uint64_t tx_id2);
    
    // Get all unique chain IDs
    std::set<uint64_t> getAllChainIds() const;
    
    // Get all transaction IDs belonging to a specific chain
    std::vector<uint64_t> getChainMembers(uint64_t chain_id) const;
    
    // Get the total number of independent chains
    size_t getChainCount() const;
    
    // Reset the union-find structure
    void reset();
    
    // Get statistics
    struct UFStats {
        size_t num_transactions;
        size_t num_chains;
        size_t max_chain_size;
        double avg_chain_size;
    };
    
    UFStats getStats() const;

private:
    // Find with path compression (internal implementation)
    uint64_t findInternal(uint64_t tx_id);
    
    // Union-find parent array: parent_[i] = parent of transaction i
    std::vector<uint64_t> parent_;
    
    // Rank array for union by rank optimization
    std::vector<uint32_t> rank_;
    
    // Mutex for thread-safe operations
    // Requirement 3.4: Support concurrent access
    mutable std::mutex mutex_;
    
    // Number of transactions
    size_t num_transactions_;
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_DEPENDENCY_CHAIN_H