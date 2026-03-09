//
// Created for New Scheduling Scheme - Dependency Chain Sharding
// ShardManager: Main controller for the sharding process
// Requirements: All requirements (orchestrates the entire sharding pipeline)
//

#ifndef NEUBLOCKCHAIN_SHARD_MANAGER_H
#define NEUBLOCKCHAIN_SHARD_MANAGER_H

#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include "shard.h"
#include "dependency_chain.h"
#include "kdg_node.h"

namespace scheduling {

// Sharding statistics
struct ShardingStatistics {
    // Configuration parameters
    uint32_t num_shards = 0;
    double optimization_threshold = 0.0;
    uint32_t max_iterations = 0;
    
    // Chain counts
    uint32_t num_chains = 0;
    uint32_t num_non_conflicting_chains = 0;
    uint32_t num_conflicting_chains = 0;
    
    // Load statistics
    uint64_t total_load = 0;
    double avg_shard_load = 0.0;
    uint64_t max_shard_load = 0;
    uint64_t min_shard_load = 0;
    
    // LPT algorithm statistics
    double lpt_initial_assignment_time_ms = 0.0;
    double lpt_greedy_assignment_time_ms = 0.0;
    
    // Transaction counts
    uint32_t num_independent_txs = 0;
    
    // Optimization statistics
    double initial_std_dev = 0.0;
    double final_std_dev = 0.0;
    double load_balance_improvement = 0.0;
    uint32_t optimization_iterations = 0;
    uint32_t num_migrations = 0;
    uint32_t num_swaps = 0;
    bool optimization_converged = false;
    
    // Timing statistics (in milliseconds)
    double sharding_time_ms = 0.0;
    double optimization_time_ms = 0.0;
    double total_time_ms = 0.0;
};

// Structure to hold optimization result
struct OptimizationResult {
    double initial_std_dev = 0.0;      // Initial standard deviation
    double final_std_dev = 0.0;        // Final standard deviation
    double improvement = 0.0;          // Improvement ratio: (initial - final) / initial
    uint32_t iterations = 0;           // Number of iterations performed
    uint32_t num_migrations = 0;       // Number of chain migrations
    uint32_t num_swaps = 0;            // Number of chain swaps
    bool converged = false;            // Whether optimization converged
};

// Structure to hold sharding result
struct ShardingResult {
    std::vector<Shard> shards;
    ShardingStatistics statistics;
    bool success = false;
    std::string error_message;
};

class ShardManager {
public:
    // Constructor
    // Requirement 15.1, 15.2, 15.3: Support configurable parameters
    explicit ShardManager(uint32_t num_shards = 8, 
                         double optimization_threshold = 0.01,
                         uint32_t max_iterations = 100);
    
    // Destructor
    ~ShardManager() = default;
    
    // Main sharding function
    // Requirements: 1-13 (complete sharding pipeline)
    // Requirement 12.1, 12.2, 12.3, 12.4: Return sharding result with statistics
    // Requirement 14.4: Error handling and rollback
    ShardingResult performSharding(
        const std::vector<DependencyChain>& non_conflicting_chains,
        const std::vector<DependencyChain>& conflicting_chains,
        const std::vector<std::shared_ptr<TransactionNode>>& independent_txs);
    
    // Getter methods for configuration parameters
    [[nodiscard]] uint32_t getNumShards() const { return num_shards_; }
    [[nodiscard]] double getOptimizationThreshold() const { return optimization_threshold_; }
    [[nodiscard]] uint32_t getMaxOptimizationIterations() const { return max_iterations_; }
    
    // Setter methods for dynamic configuration (for experiments)
    void setNumShards(uint32_t num_shards) { num_shards_ = num_shards; }
    void setOptimizationThreshold(double threshold) { optimization_threshold_ = threshold; }
    void setMaxOptimizationIterations(uint32_t iterations) { max_iterations_ = iterations; }
    
    // Enable/disable optimization
    void setOptimizationEnabled(bool enabled) { optimization_enabled_ = enabled; }
    
    // Enable/disable LPT algorithm
    void setLPTAlgorithmEnabled(bool enabled) { lpt_algorithm_enabled_ = enabled; }
    
private:
    // Configuration parameters
    uint32_t num_shards_;
    double optimization_threshold_;
    uint32_t max_iterations_;
    bool optimization_enabled_;
    bool lpt_algorithm_enabled_;
    
    // Internal data structures
    std::vector<Shard> shards_;
    std::vector<DependencyChain> all_chains_;
    ShardingStatistics statistics_;
    
    // Timing information
    double sharding_start_time_;
    double optimization_start_time_;
    
    // Phase 1: Initialization
    // Requirement 1.1, 1.2, 1.3: Create k empty shards
    void initializeShards();
    
    // Validate parameters
    // Requirement 14.1, 14.2: Validate input parameters
    bool validateParameters(std::string& error_message);
    
    // Validate input data
    // Requirement 14.3: Validate input data
    bool validateInputData(
        const std::vector<DependencyChain>& non_conflicting_chains,
        const std::vector<DependencyChain>& conflicting_chains,
        std::string& error_message);
    
    // Phase 2: Classification and sorting
    // Requirement 2.1: Classify chains by conflict attribute
    // Requirement 3.1, 3.2, 3.3: Sort chains by load descending
    void classifyAndSortChains(
        const std::vector<DependencyChain>& non_conflicting_chains,
        const std::vector<DependencyChain>& conflicting_chains);
    
    // Phase 3: Initial assignment using LPT algorithm
    // Requirement 4.1, 4.2, 4.3, 4.4: Assign first k chains to k shards using LPT
    void lptInitialAssignment();
    
    // Phase 4: Greedy assignment using LPT algorithm
    // Requirement 5.1, 5.2, 5.3, 5.4: Assign remaining chains greedily using LPT
    void lptGreedyAssignment();
    
    // Phase 5: Independent transaction assignment
    // Requirement 6.1, 6.2, 6.3: Assign independent transactions using round-robin
    void assignIndependentTransactions(
        const std::vector<std::shared_ptr<TransactionNode>>& independent_txs);
    
    // Phase 6: Optimization
    // Requirements 7-11: Iterative optimization to minimize load standard deviation
    void optimizeSharding();
    
    // Collect statistics
    void collectStatistics();
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_SHARD_MANAGER_H