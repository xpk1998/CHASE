//
// Created for New Scheduling Scheme Dependency Chain Sharding
// LoadCalculator: Calculates shard load and standard deviation
// Requirements: 7.1, 7.2, 7.3, 8.1, 8.2, 8.3
//

#ifndef NEUBLOCKCHAIN_LOAD_CALCULATOR_H
#define NEUBLOCKCHAIN_LOAD_CALCULATOR_H

#include <vector>
#include <cstdint>
#include "shard.h"

namespace scheduling {

// LoadCalculator: Calculates load statistics for shards
class LoadCalculator {
public:
    // Calculate average load
    // Requirement 7.1: Calculate average load across all shards
    // Formula: AvgLoad = (1/k) * Σ Load(Si)
    static double calculateAverageLoad(const std::vector<Shard>& shards);
    
    // Calculate load standard deviation
    // Requirement 7.2: Calculate standard deviation using formula
    // Formula: D = sqrt((1/k) * Σ(Load(Si) - AvgLoad)²)
    static double calculateStandardDeviation(const std::vector<Shard>& shards);
    
    // Calculate new standard deviation after load changes (incremental)
    // Requirement 7.3: Use incremental formula when only two shards change
    // Only shards p and q have load changes, others remain the same
    // Formula: D' = sqrt((1/k) * [Σ(unchanged) + (Load'(Sp) - AvgLoad')² + (Load'(Sq) - AvgLoad')²])
    static double calculateNewStandardDeviation(
        const std::vector<Shard>& shards,
        uint32_t shard_p_id,
        uint32_t shard_q_id,
        uint64_t new_load_p,
        uint64_t new_load_q);
    
    // Get max-load shard ID
    // Requirement 8.1: Find shard with maximum load
    static uint32_t getMaxLoadShardId(const std::vector<Shard>& shards);
    
    // Get min-load shard ID
    // Requirement 8.2: Find shard with minimum load
    static uint32_t getMinLoadShardId(const std::vector<Shard>& shards);
    
    // Get load difference between two shards
    // Requirement 8.3: Calculate load difference
    static uint64_t getLoadDifference(const Shard& shard_p, const Shard& shard_q);

private:
    // Calculate variance
    static double calculateVariance(const std::vector<Shard>& shards, double avg_load);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_LOAD_CALCULATOR_H
