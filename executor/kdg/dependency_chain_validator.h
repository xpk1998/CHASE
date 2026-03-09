//
// Created for New Scheduling Scheme Dependency Chain Extraction
// DependencyChainValidator: Validates correctness of dependency chains
// Requirements: 9.1, 9.2, 9.3, 9.4, 9.5
//

#ifndef NEUBLOCKCHAIN_DEPENDENCY_CHAIN_VALIDATOR_H
#define NEUBLOCKCHAIN_DEPENDENCY_CHAIN_VALIDATOR_H

#include <vector>
#include <memory>
#include <string>
#include <set>
#include "dependency_chain.h"
#include "dependency_detector.h"

namespace scheduling {

// DependencyChainValidator: Validates the correctness of dependency chains
// Ensures chains satisfy causality constraints and dependency relationships
class DependencyChainValidator {
public:
    // Validation error information
    struct ValidationError {
        enum class ErrorType {
            SEQUENCE_NOT_MONOTONIC,      // Sequence numbers not monotonically increasing
            NO_DEPENDENCY_BETWEEN_ADJACENT, // Adjacent transactions lack dependency
            CROSS_CHAIN_CONFLICT,        // Conflict exists between different chains
            EMPTY_CHAIN,                 // Chain is empty (warning, not error)
            INVALID_TRANSACTION          // Transaction pointer is null
        };
        
        ErrorType type;
        std::string message;
        uint64_t chain_id;
        uint64_t tx_id;          // Transaction ID where error occurred
        size_t position;         // Position in chain where error occurred
        
        ValidationError(ErrorType t, const std::string& msg, 
                       uint64_t cid, uint64_t tid = 0, size_t pos = 0)
            : type(t), message(msg), chain_id(cid), tx_id(tid), position(pos) {}
        
        std::string toString() const;
    };
    
    // Validation result for a single chain
    struct ChainValidationResult {
        bool valid;
        uint64_t chain_id;
        std::vector<ValidationError> errors;
        
        ChainValidationResult() : valid(true), chain_id(0) {}
        ChainValidationResult(uint64_t cid) : valid(true), chain_id(cid) {}
        
        void addError(const ValidationError& error) {
            errors.push_back(error);
            valid = false;
        }
        
        std::string toString() const;
    };
    
    // Validation result for multiple chains
    struct ValidationResult {
        bool all_valid;
        size_t num_chains;
        size_t num_valid_chains;
        size_t num_invalid_chains;
        std::vector<ChainValidationResult> chain_results;
        std::vector<ValidationError> cross_chain_errors;
        
        ValidationResult() 
            : all_valid(true), num_chains(0), 
              num_valid_chains(0), num_invalid_chains(0) {}
        
        void addChainResult(const ChainValidationResult& result) {
            chain_results.push_back(result);
            num_chains++;
            if (result.valid) {
                num_valid_chains++;
            } else {
                num_invalid_chains++;
                all_valid = false;
            }
        }
        
        void addCrossChainError(const ValidationError& error) {
            cross_chain_errors.push_back(error);
            all_valid = false;
        }
        
        std::string toString() const;
    };
    
    // Validate a single dependency chain
    // Requirement 9.1, 9.2: Check sequence monotonicity and dependencies
    static ChainValidationResult validateChain(const DependencyChain& chain);
    
    // Validate multiple dependency chains
    // Requirement 9.3: Check for cross-chain conflicts
    static ValidationResult validateChains(const std::vector<DependencyChain>& chains);
    
    // Validate sequence number monotonicity within a chain
    // Requirement 9.1: Sequence numbers must be monotonically increasing
    static bool validateSequenceMonotonicity(
        const DependencyChain& chain,
        std::vector<ValidationError>& errors);
    
    // Validate dependency relationships between adjacent transactions
    // Requirement 9.2: Adjacent transactions must have dependencies
    static bool validateDependencyRelationships(
        const DependencyChain& chain,
        std::vector<ValidationError>& errors);
    
    // Validate that different chains don't have conflicting dependencies
    // Requirement 9.3: Check for cross-chain conflicts
    static bool validateCrossChainConflicts(
        const std::vector<DependencyChain>& chains,
        std::vector<ValidationError>& errors);
    
    // Helper: Check if two chains have conflicting transactions
    static bool haveCrossChainConflict(
        const DependencyChain& chain1,
        const DependencyChain& chain2,
        ValidationError& error);

private:
    // Helper: Convert error type to string
    static std::string errorTypeToString(ValidationError::ErrorType type);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_DEPENDENCY_CHAIN_VALIDATOR_H
