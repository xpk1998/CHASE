//
// AddressBasedConflictGraph - KDG-based conflict detection for BLP
// Implements address-level conflict detection using Kahn's algorithm
//

#ifndef NEUBLOCKCHAIN_ADDRESS_BASED_CONFLICT_GRAPH_H
#define NEUBLOCKCHAIN_ADDRESS_BASED_CONFLICT_GRAPH_H

#include <vector>
#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include "kdg_node.h"
#include "../../utilities/types/aria_types.h"
#include "two_zone_types.h"
#include "shard.h"
#include "dependency_chain.h"
#include "dependency_chain_extractor.h"
#include "../core/simulated_transaction.h"

namespace scheduling {

// Forward declaration
struct ScheduledInfo;

// Address-Based Conflict Graph (KDG)
// This is the core data structure for dependency chain sharding scheduling
class AddressBasedConflictGraph {
public:
    AddressBasedConflictGraph() = default;
    
    // Construct KDG from simulated transactions
    static AddressBasedConflictGraph construct(
        const std::vector<SimulatedTransaction>& simulation_result);
    
    // Parallel construction (divide-and-conquer)
    static AddressBasedConflictGraph parConstruct(
        const std::vector<SimulatedTransaction>& simulation_result,
        int num_threads = 0);  // 0 = auto-detect
    
    // Hierarchical sort algorithm
    AddressBasedConflictGraph& hierarchicalSort();
    
    // Reorder aborted transactions (Epoch-level reordering)
    AddressBasedConflictGraph& reorder();
    
    // Extract dependency chains (after hierarchical sort)
    // Requirements: 8.1, 8.2, 8.3, 8.4
    // This method extracts dependency chains from both non-conflicting and conflicting zones
    // Returns: DependencyChainResult containing chains and statistics
    DependencyChainResult extractDependencyChains();
    
    // Extract schedule result
    ScheduledInfo extractSchedule();
    
    // Extract schedule with dependency chains
    // This combines extractDependencyChains() and extractSchedule()
    // Requirement 8.4: Include dependency chain information in ScheduledInfo
    ScheduledInfo extractScheduleWithDependencyChains();
    
    // Get statistics
    size_t transactionCount() const { return tx_list_.size(); }
    size_t abortedCount() const { return aborted_txs_.size(); }
    size_t addressCount() const { return addresses_.size(); }
    
    // O(1) conflict detection methods
    bool hasConflictWithRead(uint64_t tx_id, const std::string& key) const;
    bool hasConflictWithWrite(uint64_t tx_id, const std::string& key) const;
    
    // Get all dependencies for a transaction in O(1)
    const std::unordered_set<uint64_t>& getDependencies(uint64_t tx_id) const;
    
    // Get all transactions that depend on this transaction in O(1)
    const std::unordered_set<uint64_t>& getReverseDependencies(uint64_t tx_id) const;
    
    // Check if transaction has any conflicts in O(1) average case
    bool hasConflicts(uint64_t tx_id) const;
    
    // Performance statistics
    struct ConstructionStats {
        uint64_t construction_time_us = 0;
        uint64_t sorting_time_us = 0;
        uint64_t reordering_time_us = 0;
        size_t num_transactions = 0;
        size_t num_addresses = 0;
        size_t num_aborted = 0;
        size_t num_reordered = 0;
        
        // Sequence number distribution
        uint32_t max_sequence = 0;
        double avg_sequence = 0.0;
        
        void reset() {
            construction_time_us = 0;
            sorting_time_us = 0;
            reordering_time_us = 0;
            num_transactions = 0;
            num_addresses = 0;
            num_aborted = 0;
            num_reordered = 0;
            max_sequence = 0;
            avg_sequence = 0.0;
        }
    };
    
    const ConstructionStats& getStats() const { return stats_; }
    
private:
    // Internal construction helper
    static AddressBasedConflictGraph constructInternal(
        const std::vector<SimulatedTransaction>& txs);
    
    // Address ranking algorithm
    std::vector<std::string> addressRank() const;
    
    // Add units to address
    void addUnitsToAddress(std::vector<std::shared_ptr<Unit>> units);
    
    // Convert read/write set to units
    static std::vector<std::shared_ptr<Unit>> convertToUnits(
        std::shared_ptr<TransactionNode> tx,
        UnitType unit_type,
        const std::map<std::string, std::map<std::string, std::string>>& rw_map,
        const std::map<std::string, std::map<std::string, std::string>>* read_set = nullptr);
    
    // Set WR dependencies between read and write units
    static void setWRDependencies(
        std::vector<std::shared_ptr<Unit>>& read_units,
        std::vector<std::shared_ptr<Unit>>& write_units);
    
    // Check if updater already exists (early abort detection)
    bool checkUpdaterAlreadyExists(
        const std::vector<std::shared_ptr<Unit>>& write_units);
    
    // Extract aborted transactions
    std::vector<std::shared_ptr<TransactionNode>> extractAbortedTxs();
    
    // Merge another graph into this one (for parallel construction)
    void merge(AddressBasedConflictGraph&& other);
    
private:
    // Address map: address_key -> Address
    std::unordered_map<std::string, Address> addresses_;
    
    // Optimized conflict detection: key -> set of transaction IDs that write to this key
    // This allows O(1) conflict detection during transaction processing
    std::unordered_map<std::string, std::unordered_set<uint64_t>> write_key_to_txs_;
    
    // Optimized conflict detection: key -> set of transaction IDs that read from this key
    // This allows O(1) conflict detection during transaction processing
    std::unordered_map<std::string, std::unordered_set<uint64_t>> read_key_to_txs_;
    
    // Transaction dependencies for fast lookup
    // tx_id -> set of transaction IDs this transaction depends on
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> tx_dependencies_;
    
    // Reverse transaction dependencies for fast lookup
    // tx_id -> set of transaction IDs that depend on this transaction
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> tx_reverse_dependencies_;
    
    // Transaction list: tx_id -> Transaction
    std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>> tx_list_;
    
    // Aborted transactions (for reordering)
    std::vector<std::shared_ptr<TransactionNode>> aborted_txs_;
    
    // Performance statistics
    mutable ConstructionStats stats_;
};

// Scheduled transaction (with sequence number)
struct FinalizedTransaction {
    uint64_t tx_id;
    uint32_t sequence;
    void* raw_transaction;
    std::set<std::string> read_keys;   // Read set for conflict detection
    std::set<std::string> write_keys;  // Write set for conflict detection
    
    FinalizedTransaction(uint64_t id, uint32_t seq, void* raw_tx)
        : tx_id(id), sequence(seq), raw_transaction(raw_tx) {}
    
    FinalizedTransaction(uint64_t id, uint32_t seq, void* raw_tx,
                        const std::set<std::string>& r_keys,
                        const std::set<std::string>& w_keys)
        : tx_id(id), sequence(seq), raw_transaction(raw_tx),
          read_keys(r_keys), write_keys(w_keys) {}
};

// Aborted transaction (for reordering)
struct AbortedTransaction {
    uint64_t tx_id;
    std::set<std::string> read_keys;
    std::set<std::string> write_keys;
    void* raw_transaction;
    
    AbortedTransaction(std::shared_ptr<TransactionNode> tx)
        : tx_id(tx->id()),
          read_keys(tx->readKeys()),
          write_keys(tx->writeKeys()),
          raw_transaction(tx->rawTransaction()) {}
};

// Schedule result
struct ScheduledInfo {
    // Dual-zone structure: Non-conflicting zone and Conflicting zone
    // Non-conflicting zone: transactions without conflicts
    std::vector<std::vector<FinalizedTransaction>> non_conflicting_zone_txs;
    
    // Conflicting zone: transactions with conflicts
    // Organized by sequence number for sequential reordering within the zone
    std::vector<std::vector<FinalizedTransaction>> conflicting_zone_txs;
    
    // Aborted transactions for conflicting zone sequential reordering
    // These will be scheduled to subsequent sequences in the conflicting zone
    std::vector<AbortedTransaction> aborted_txs;
    
    // Dependency chain information (optional, populated if extractDependencyChains was called)
    // Requirement 8.4: Include dependency chain information in ScheduledInfo
    bool has_dependency_chains = false;
    std::shared_ptr<DependencyChainResult> dependency_chains;
    
    // Sharding information (optional, populated if sharding was performed)
    bool has_sharding = false;
    std::vector<Shard> shards;
    
    // Statistics
    size_t scheduledTxsCount() const {
        size_t count = 0;
        for (const auto& group : non_conflicting_zone_txs) {
            count += group.size();
        }
        for (const auto& group : conflicting_zone_txs) {
            count += group.size();
        }
        return count;
    }
    
    size_t nonConflictingZoneTxsCount() const {
        size_t count = 0;
        for (const auto& group : non_conflicting_zone_txs) {
            count += group.size();
        }
        return count;
    }
    
    size_t conflictingZoneTxsCount() const {
        size_t count = 0;
        for (const auto& group : conflicting_zone_txs) {
            count += group.size();
        }
        return count;
    }
    
    size_t abortedTxsCount() const {
        return aborted_txs.size();
    }
    
    // Get dependency chain count
    size_t dependencyChainCount() const {
        if (!has_dependency_chains || !dependency_chains) {
            return 0;
        }
        return dependency_chains->totalChainCount();
    }
    
    // Get parallelism metrics
    struct ParallelismMetrics {
        size_t total_tx;
        double average_width;
        double std_width;
        size_t max_width;
        size_t depth;
    };
    
    ParallelismMetrics getParallelismMetrics() const;
    
    // Create from transaction lists
    static ScheduledInfo create(
        std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list,
        std::vector<std::shared_ptr<TransactionNode>>& aborted_txs);
    
    // Create from transaction lists with dependency chains
    // Requirement 8.4: Include dependency chain information in ScheduledInfo
    static ScheduledInfo createWithDependencyChains(
        std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list,
        std::vector<std::shared_ptr<TransactionNode>>& aborted_txs,
        DependencyChainResult&& chain_result);
    
    // Find minimum sequence in conflicting zone with no conflicts
    // If transaction conflicts with sequence i, place it in sequence i+1
    static size_t findMinimumSequenceWithNoConflicts(
        const std::set<std::string>& read_keys,
        const std::set<std::string>& write_keys,
        const std::vector<std::set<std::string>>& sequence_map);
    
private:
    // Schedule sorted transactions
    static std::vector<std::vector<FinalizedTransaction>> scheduleSortedTxs(
        std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list);
    
    // Schedule aborted transactions for conflicting zone sequential reordering
    // Aborted transactions are placed in subsequent sequences within the conflicting zone
    static std::vector<AbortedTransaction> scheduleAbortedTxs(
        std::vector<std::shared_ptr<TransactionNode>>& aborted_txs);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_ADDRESS_BASED_CONFLICT_GRAPH_H