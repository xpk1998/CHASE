//
// Created for New Scheduling Scheme - Dependency Chain Sharding
// Shard: Represents a shard containing dependency chains and independent transactions
// Requirements: 1.1, 1.2, 1.3, 2.2, 2.3, 2.4, 6.2, 6.3, 9.4, 9.5, 10.5, 10.6
//

#ifndef NEUBLOCKCHAIN_SHARD_H
#define NEUBLOCKCHAIN_SHARD_H

#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <cstdint>
#include <string>
#include "dependency_chain.h"
#include "../transaction/transaction.h"

namespace scheduling {

// Shard: Represents a shard with dual-zone structure
// Each shard contains:
// - Non-conflicting zone: for non-conflicting dependency chains
// - Conflicting zone: for conflicting dependency chains
// - Independent transactions: transactions not in any chain
class Shard {
public:
    // Default constructor
    Shard() : shard_id_(0), total_load_(0), sequences_scheduled_(false), 
              concurrency_width_(0), execution_depth_(0) {}
    
    // Constructor
    // Requirement 1.1: Create shard with unique ID
    explicit Shard(uint32_t shard_id);
    
    // Add dependency chain to appropriate zone
    // Requirement 2.2, 2.3: Add to non-conflicting or conflicting zone based on chain type
    void addChain(const DependencyChain& chain);
    
    // Add independent transaction
    // Requirement 6.2: Add independent transaction to shard
    void addIndependentTransaction(std::shared_ptr<TransactionNode> tx);
    
    // Remove dependency chain
    // Requirement 9.4, 10.5: Support chain removal for migration/swap
    bool removeChain(uint64_t chain_id);
    
    // Find chain by ID
    // Returns pointer to chain if found, nullptr otherwise
    DependencyChain* findChain(uint64_t chain_id);
    const DependencyChain* findChain(uint64_t chain_id) const;
    
    // Get shard ID
    // Requirement 1.2: Return unique shard identifier
    uint32_t getShardId() const { return shard_id_; }
    
    // Get shard total load
    // Requirement 1.3, 7.1: Return total load (Gas consumption)
    uint64_t getTotalLoad() const { return total_load_; }
    
    // Update load
    // Requirement 9.5, 10.6: Update load after migration/swap
    void updateLoad(int64_t delta);
    
    // Get non-conflicting zone chains
    // Requirement 2.2: Return non-conflicting chains
    const std::vector<DependencyChain>& getNonConflictingChains() const {
        return non_conflicting_chains_;
    }
    
    // Get conflicting zone chains
    // Requirement 2.3: Return conflicting chains
    const std::vector<DependencyChain>& getConflictingChains() const {
        return conflicting_chains_;
    }
    
    // Get independent transactions
    // Requirement 6.3: Return independent transactions
    const std::vector<std::shared_ptr<TransactionNode>>& getIndependentTransactions() const {
        return independent_txs_;
    }
    
    // Get total chain count
    size_t getChainCount() const {
        return non_conflicting_chains_.size() + conflicting_chains_.size();
    }
    
    // Get all chains (both non-conflicting and conflicting)
    std::vector<DependencyChain> getChains() const {
        std::vector<DependencyChain> all_chains;
        all_chains.reserve(non_conflicting_chains_.size() + conflicting_chains_.size());
        
        // Add non-conflicting chains
        all_chains.insert(all_chains.end(), non_conflicting_chains_.begin(), non_conflicting_chains_.end());
        
        // Add conflicting chains
        all_chains.insert(all_chains.end(), conflicting_chains_.begin(), conflicting_chains_.end());
        
        return all_chains;
    }
    
    // Get total transaction count (including independent txs)
    size_t getTransactionCount() const;
    
    // Check if shard is empty
    bool empty() const {
        return non_conflicting_chains_.empty() && 
               conflicting_chains_.empty() && 
               independent_txs_.empty();
    }
    
    // Get shard statistics
    struct ShardStats {
        uint32_t shard_id;
        uint64_t total_load;
        size_t num_non_conflicting_chains;
        size_t num_conflicting_chains;
        size_t num_independent_txs;
        size_t total_transactions;
    };
    
    ShardStats getStats() const;
    
    // Debug: Convert to string representation
    std::string toString() const;
    
    // ========================================================================
    // SEQUENCE SCHEDULING SUPPORT
    // ========================================================================
    
    // Sequence: transactions to execute in parallel
    using Sequence = std::vector<std::shared_ptr<TransactionNode>>;
    
    // SequenceMap: map from sequence number to transactions
    using SequenceMap = std::map<uint32_t, Sequence>;
    
    // Get scheduled sequences for non-conflicting zone
    // Returns layered sequences organized by dependency depth
    const SequenceMap& getNonConflictingSequences() const {
        return nc_sequences_;
    }
    
    // Get scheduled sequences for conflicting zone
    // Returns serialized sequences for conflicting chains
    const SequenceMap& getConflictingSequences() const {
        return c_sequences_;
    }
    
    // Schedule sequences for this shard
    // Organizes transactions into sequences for execution
    // Returns true if scheduling succeeds
    bool scheduleSequences();
    
    // Check if sequences have been scheduled
    bool isScheduled() const { return sequences_scheduled_; }
    
    // Get concurrency metrics
    uint32_t getConcurrencyWidth() const { return concurrency_width_; }
    uint32_t getExecutionDepth() const { return execution_depth_; }

private:
    // Recalculate total load from all chains and transactions
    void recalculateLoad();
    
    uint32_t shard_id_;                                      // Unique shard identifier
    uint64_t total_load_;                                    // Total Gas consumption (load)
    
    // Dual-zone structure
    std::vector<DependencyChain> non_conflicting_chains_;    // Non-conflicting zone
    std::vector<DependencyChain> conflicting_chains_;        // Conflicting zone
    std::vector<std::shared_ptr<TransactionNode>> independent_txs_; // Independent transactions
    
    // Chain ID to zone mapping for fast lookup
    // true = non-conflicting zone, false = conflicting zone
    std::unordered_map<uint64_t, bool> chain_id_to_zone_;
    
    // Sequence scheduling state
    bool sequences_scheduled_;                               // Whether sequences have been scheduled
    SequenceMap nc_sequences_;                               // NC zone sequences (layered)
    SequenceMap c_sequences_;                                // C zone sequences (serialized)
    uint32_t concurrency_width_;                             // NC zone concurrency width
    uint32_t execution_depth_;                               // NC zone execution depth
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_SHARD_H
