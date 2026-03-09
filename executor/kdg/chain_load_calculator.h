//
// Created for New Scheduling Scheme Dependency Chain Extraction
// ChainLoadCalculator: Calculate Gas load for dependency chains
// Requirements: 5.1, 5.2, 5.3, 5.4, 5.5
//

#ifndef NEUBLOCKCHAIN_CHAIN_LOAD_CALCULATOR_H
#define NEUBLOCKCHAIN_CHAIN_LOAD_CALCULATOR_H

#include <vector>
#include <cstdint>
#include <cmath>
#include "dependency_chain.h"

namespace scheduling {

/**
 * ChainLoadCalculator: Gas-based Load Calculation for Dependency Chains
 * 
 * Calculates the computational load (workload) of each dependency chain
 * by summing the Gas consumption of all transactions within the chain.
 * 
 * ========================================================================
 * CHAIN LOAD FORMULA
 * ========================================================================
 * 
 * For a dependency chain L_i containing transactions {T_1, T_2, ..., T_n}:
 * 
 *     Load(L_i) = Σ(j=1 to n) GasUsed(T_j)
 * 
 * Where:
 *   GasUsed(T_j) = Actual Gas consumed during EVM simulation execution
 * 
 * ========================================================================
 * GAS DATA SOURCE
 * ========================================================================
 * 
 * 1. EVM Transactions (Smart Contracts):
 *    - GasUsed obtained from EVM simulation during read-write set extraction
 *    - Reflects actual computational cost of smart contract execution
 *    - Includes:
 *      · Base transaction cost (21,000 Gas)
 *      · Contract execution cost (opcodes, storage, memory)
 *      · Data payload cost
 * 
 * 2. Non-EVM Transactions (Simple transfers):
 *    - Use default Gas estimate: 21,000 Gas
 *    - Standard Ethereum transaction base cost
 *    - Provides consistent estimation baseline
 * 
 * ========================================================================
 * LOAD USAGE IN SYSTEM
 * ========================================================================
 * 
 * 1. Shard Load Balancing:
 *    - Distribute chains across shards to balance total Gas load
 *    - Minimize load standard deviation between shards
 *    - Maximize parallel execution efficiency
 * 
 * 2. Execution Scheduling:
 *    - Prioritize high-load chains for earlier execution
 *    - Optimize resource utilization
 *    - Reduce overall execution latency
 * 
 * 3. Performance Optimization:
 *    - Identify bottleneck chains (high load)
 *    - Guide iterative shard optimization
 *    - Provide metrics for system tuning
 * 
 * ========================================================================
 * IMPLEMENTATION REQUIREMENTS
 * ========================================================================
 * 
 * Requirement 5.1: Sum Gas consumption of all transactions in chain
 * Requirement 5.2: Use actual Gas from EVM simulation when available
 * Requirement 5.3: Use default estimate (21,000) when Gas data missing
 * Requirement 5.4: Efficiently batch calculate loads for all chains
 * Requirement 5.5: Provide load distribution statistics for monitoring
 * 
 * ========================================================================
 */
class ChainLoadCalculator {
public:
    // Default Gas estimate for transactions without Gas data
    static constexpr uint64_t DEFAULT_GAS_ESTIMATE = 21000;
    
    // Calculate load for a single chain
    // Requirement 5.1: Sum Gas consumption of all transactions in chain
    // Requirement 5.2: Use actual Gas from simulation when available
    // Requirement 5.3: Use default estimate when Gas data missing
    // Returns: Total Gas consumption (load) of the chain
    static uint64_t calculateChainLoad(const DependencyChain& chain);
    
    // Batch calculate loads for multiple chains
    // Requirement 5.4: Efficiently calculate loads for all chains
    // Updates the load value in each chain object
    static void calculateAllLoads(std::vector<DependencyChain>& chains);
    
    // Load distribution statistics
    struct LoadDistribution {
        uint64_t total_gas;      // Total Gas across all chains
        uint64_t max_load;       // Maximum chain load
        uint64_t min_load;       // Minimum chain load
        double avg_load;         // Average chain load
        double std_dev;          // Standard deviation of loads
        size_t num_chains;       // Number of chains
        
        LoadDistribution() 
            : total_gas(0), max_load(0), min_load(UINT64_MAX),
              avg_load(0.0), std_dev(0.0), num_chains(0) {}
    };
    
    // Get load distribution statistics
    // Requirement 5.5: Provide load distribution for monitoring
    static LoadDistribution getLoadDistribution(
        const std::vector<DependencyChain>& chains);

private:
    // Extract Gas used from a transaction
    // Requirement 5.2: Get Gas from EVM simulation results
    // Requirement 5.3: Return default estimate if data unavailable
    static uint64_t extractGasUsed(const std::shared_ptr<TransactionNode>& tx);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_CHAIN_LOAD_CALCULATOR_H
