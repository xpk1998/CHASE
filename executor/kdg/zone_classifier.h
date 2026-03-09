//
// Created for Two-Zone Execution
// ZoneClassifier: Classifies transactions into non-conflicting and conflicting zones
// Requirements: 1.3, 2.1, 6.2
//

#ifndef NEUBLOCKCHAIN_ZONE_CLASSIFIER_H
#define NEUBLOCKCHAIN_ZONE_CLASSIFIER_H

#include "two_zone_types.h"
#include "address_based_conflict_graph.h"
#include <glog/logging.h>

namespace scheduling {

// Forward declarations
class DependencyChain;
struct DependencyChainResult;

// Zone information structure
struct ZoneInfo {
    // Non-conflicting zone information
    struct NonConflictingZoneInfo {
        // Sequences of transactions that can be executed in parallel
        std::vector<std::vector<FinalizedTransaction>> sequences;
        
        // Total number of sequences
        size_t sequence_count;
        
        // Total number of transactions in non-conflicting zone
        size_t total_tx_count;
        
        // Union of all write keys in non-conflicting zone (for verification)
        HashSet<std::string> all_write_keys;
        
        // Constructor
        NonConflictingZoneInfo() 
            : sequence_count(0), total_tx_count(0) {}
    };
    
    NonConflictingZoneInfo non_conflicting_zone;
    
    // Aborted transactions that will be handled in conflicting zone
    std::vector<AbortedTransaction> aborted_transactions;
    
    // Statistics
    double non_conflicting_ratio;  // Ratio of non-conflicting transactions
    double abort_rate;             // Ratio of aborted transactions
    
    // Constructor
    ZoneInfo() 
        : non_conflicting_ratio(0.0), abort_rate(0.0) {}
    
    // Calculate statistics based on total transaction count
    void calculateStats(size_t total_tx_count) {
        if (total_tx_count > 0) {
            non_conflicting_ratio = static_cast<double>(non_conflicting_zone.total_tx_count) / total_tx_count;
            abort_rate = static_cast<double>(aborted_transactions.size()) / total_tx_count;
        } else {
            non_conflicting_ratio = 0.0;
            abort_rate = 0.0;
        }
    }
};

// ============================================================================
// ZoneClassifier: Classifies scheduled transactions into zones
// Requirement 1.3: Classify transactions into non-conflicting and conflicting zones
// Requirement 2.1: Verify write set disjoint property
// ============================================================================

class ZoneClassifier {
public:
    // Classify scheduled transactions into zones
    // Requirement 1.3: Extract non-conflicting zone sequences from ScheduledInfo
    // Requirement 1.3: Extract aborted transactions for conflicting zone
    // Requirement 1.3: Compute zone statistics (transaction counts, ratios)
    static ZoneInfo classifyIntoZones(const ScheduledInfo& schedule_info);
    
    // Classify with dependency chain information
    // Requirement 5.1: Use dependency chain information for zone classification
    // Requirement 5.2: Prioritize assigning chains with disjoint write sets to non-conflicting zone
    static ZoneInfo classifyIntoZonesWithChains(const ScheduledInfo& schedule_info);
    
    // Verify non-conflicting zone invariant: all write sets are disjoint
    // Requirement 2.1: Verify all write sets in non-conflicting zone are disjoint
    // Requirement 6.2: Use hashbrown::HashSet for efficient disjoint checking
    static bool verifyNonConflictingZone(
        const std::vector<std::vector<FinalizedTransaction>>& sequences);
    
private:
    // Extract write keys from a transaction
    static std::set<std::string> extractWriteKeys(const FinalizedTransaction& tx);
    
    // Check if two write sets are disjoint
    static bool areWriteSetsDisjoint(
        const HashSet<std::string>& set1,
        const HashSet<std::string>& set2);
    
    // Check if dependency chains are available in ScheduledInfo
    // Requirement 5.1: Check if dependency chains are available
    static bool hasDependencyChains(const ScheduledInfo& schedule_info);
    
    // Extract chain write sets for conflict detection
    // Requirement 5.2: Build write sets for each chain
    static HashSet<std::string> extractChainWriteSet(const DependencyChain& chain);
    
    // Classify chains into zones based on write set conflicts
    // Requirement 5.2: Prioritize assigning chains with disjoint write sets to non-conflicting zone
    static void classifyChainsByWriteSets(
        const std::vector<DependencyChain>& chains,
        std::vector<DependencyChain>& non_conflicting_chains,
        std::vector<DependencyChain>& conflicting_chains);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_ZONE_CLASSIFIER_H