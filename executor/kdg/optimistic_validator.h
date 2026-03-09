//
// Created for Two-Zone Execution
// OptimisticValidator: Validates optimistic assumption for re-executed transactions
// Requirements: 4.1, 4.3, 4.4, 4.5, 6.2, 8.3
//

#ifndef NEUBLOCKCHAIN_OPTIMISTIC_VALIDATOR_H
#define NEUBLOCKCHAIN_OPTIMISTIC_VALIDATOR_H

#include "two_zone_types.h"
#include "address_based_conflict_graph.h"
#include <vector>
#include <glog/logging.h>

namespace scheduling {

// Forward declaration
class TransactionNode;

// ============================================================================
// OptimisticValidator: Validates optimistic assumption for re-executed transactions
// 
// VALIDATION STRATEGY:
// - Check if transaction's read keys have been modified by committed writes
// - Classify transactions as VALID or INVALID based on key changes
// - Enable further processing based on validation results
// 
// Validation Process:
// 1. Maintain committed_write_set: tracks all keys written by committed txs
// 2. For each re-executed transaction:
//    a) Extract its write keys
//    b) Check if write set is disjoint with committed_write_set
//    c) If DISJOINT:
//       - No conflicts detected
//       - Transaction is VALID
//       - Can commit in parallel
//    d) If INTERSECTS:
//       - Conflicts detected (keys have changed)
//       - Transaction is INVALID
//       - Requires further handling:
//         · Check if write set disjoint with current sequence
//         · If yes: can still commit in parallel (reclassified as non-conflicting)
//         · If no: must re-execute serially
// 
// 3. Separate into valid_txs and invalid_txs lists
// 4. Return validation statistics
// 
// Key Insight:
// Even if optimistic assumption fails (INVALID), transaction may still
// be committable in parallel if its write set doesn't conflict with
// current sequence's write operations.
// 
// Requirements:
// - Requirement 4.1: Implement optimistic assumption validation algorithm
// - Requirement 4.3: Maintain committed_write_set to track committed writes
// - Requirement 4.4: Check if each transaction's write set is disjoint with committed_write_set
// - Requirement 4.5: Separate transactions into valid and invalid lists
// ============================================================================

class OptimisticValidator {
public:
    // Validation result
    struct ValidationResult {
        std::vector<void*> valid_txs;
        std::vector<void*> invalid_txs;
        size_t total_validated;
        size_t conflicts_detected;
        
        // Requirement 8.3: Validation statistics
        size_t valid_count;
        size_t invalid_count;
        double validation_success_rate;
        
        ValidationResult() 
            : total_validated(0), conflicts_detected(0),
              valid_count(0), invalid_count(0), validation_success_rate(0.0) {}
        
        // Calculate validation success rate
        // Requirement 8.3: Calculate validation success rate
        void calculateSuccessRate() {
            if (total_validated > 0) {
                validation_success_rate = static_cast<double>(valid_count) / total_validated;
            } else {
                validation_success_rate = 0.0;
            }
        }
    };
    
    // Validate optimistic assumption for re-executed transactions
    // Requirement 4.1: Implement optimistic assumption validation algorithm
    // Requirement 4.3: Maintain committed_write_set to track committed writes
    // Requirement 4.4: Check if each transaction's write set is disjoint with committed_write_set
    // Requirement 4.5: Separate transactions into valid and invalid lists
    static ValidationResult validate(
        const std::vector<void*>& re_executed_txs,
        const std::vector<AbortedTransaction>& tx_info);
    
private:
    // Check if transaction's write set is disjoint with committed write set
    // Requirement 4.4: Extract write keys from transaction
    // Requirement 6.2: Use hashbrown::HashSet::is_disjoint() for efficient checking
    // Returns true if disjoint (no conflict), false if conflict
    static bool isWriteSetDisjoint(
        const std::set<std::string>& write_keys,
        const HashSet<std::string>& committed_write_set);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_OPTIMISTIC_VALIDATOR_H
