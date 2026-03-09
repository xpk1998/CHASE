//
// Created for Two-Zone Execution
// ConflictingZoneScheduler: Assigns aborted transactions to conflicting zone sequences
// Requirements: 3.1, 3.2, 3.3, 6.2
//

#ifndef NEUBLOCKCHAIN_CONFLICTING_ZONE_SCHEDULER_H
#define NEUBLOCKCHAIN_CONFLICTING_ZONE_SCHEDULER_H

#include "two_zone_types.h"
#include "shard.h"
#include "address_based_conflict_graph.h"
#include <glog/logging.h>

namespace scheduling {

// Forward declarations
class DependencyChain;

// ============================================================================
// ConflictingZoneScheduler: Assigns aborted transactions to sequences
// Requirement 3.1: Use binary search algorithm to find minimum conflict-free sequence
// Requirement 3.2: Maintain sequence_map data structure
// Requirement 3.3: Build SequenceMap with sequence write sets
// ============================================================================

class ConflictingZoneScheduler {
public:
    // Assign aborted transactions to conflicting zone sequences
    // Requirement 3.1: Sort aborted transactions by ID for determinism
    // Requirement 3.1: Iterate through transactions and assign to sequences
    // Requirement 3.1: Use binary search or linear search based on configuration
    // Requirement 3.2: Build SequenceMap with sequence write sets
    static Shard::SequenceMap assignToSequences(
        const std::vector<AbortedTransaction>& aborted_txs,
        bool enable_binary_search = true);
    
    // Assign with dependency chain information
    // Requirement 5.3: Group transactions by dependency chain
    // Requirement 5.4: Assign conflicting chains to different sequences
    // Requirement 5.4: Maintain chain-level statistics
    static Shard::SequenceMap assignToSequencesWithChains(
        const std::vector<AbortedTransaction>& aborted_txs,
        const std::vector<DependencyChain>& conflicting_chains,
        bool enable_binary_search = true);
    
    // Chain-level statistics
    struct ChainSequenceStats {
        size_t num_chains;
        size_t num_sequences;
        double avg_chains_per_sequence;
        size_t max_chains_per_sequence;
        std::vector<size_t> chains_per_sequence;  // Number of chains in each sequence
        
        ChainSequenceStats() 
            : num_chains(0), num_sequences(0),
              avg_chains_per_sequence(0.0), max_chains_per_sequence(0) {}
        
        void calculate() {
            if (num_sequences > 0) {
                avg_chains_per_sequence = static_cast<double>(num_chains) / num_sequences;
            }
            
            max_chains_per_sequence = 0;
            for (size_t count : chains_per_sequence) {
                if (count > max_chains_per_sequence) {
                    max_chains_per_sequence = count;
                }
            }
        }
    };
    
private:
    // Find minimum sequence where transaction has no conflicts
    // Requirement 3.1: Find minimum sequence with no conflicts
    // Requirement 3.3: Use binary search to find minimum sequence where transaction has no conflicts
    // Requirement 6.2: Check if read_keys ∪ write_keys is disjoint with sequence write sets
    static size_t findMinimumConflictFreeSequence(
        const std::set<std::string>& read_keys,
        const std::set<std::string>& write_keys,
        const Shard::SequenceMap& sequence_map,
        bool enable_binary_search);
    
    // Linear search for minimum conflict-free sequence
    static size_t linearSearchConflictFreeSequence(
        const std::set<std::string>& read_keys,
        const std::set<std::string>& write_keys,
        const Shard::SequenceMap& sequence_map);
    
    // Binary search for minimum conflict-free sequence
    static size_t binarySearchConflictFreeSequence(
        const std::set<std::string>& read_keys,
        const std::set<std::string>& write_keys,
        const Shard::SequenceMap& sequence_map);
    
    // Check if transaction conflicts with a sequence
    static bool conflictsWithSequence(
        const std::set<std::string>& read_keys,
        const std::set<std::string>& write_keys,
        const std::set<std::string>& sequence_write_keys);
    
    // Extract chain write set
    // Requirement 5.3: Build write set for a dependency chain
    static HashSet<std::string> extractChainWriteSet(const DependencyChain& chain);
    
    // Find minimum conflict-free sequence for a chain
    // Requirement 5.4: Assign conflicting chains to different sequences
    static size_t findMinimumConflictFreeSequenceForChain(
        const DependencyChain& chain,
        const Shard::SequenceMap& sequence_map,
        bool enable_binary_search);
    
    // Check if chain conflicts with a sequence
    static bool chainConflictsWithSequence(
        const HashSet<std::string>& chain_write_set,
        const std::set<std::string>& sequence_write_keys);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_CONFLICTING_ZONE_SCHEDULER_H