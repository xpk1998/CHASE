#include <vector>
#include "../../kdg/shard.h"
#include "../../storage/core/cache/state_cache_manager.h"

class ConflictingZoneExecutor {
public:
    // Execute one sequence (may have multiple transactions if write sets are disjoint)
    // Requirement 3.4: Re-execute all transactions in sequence
    // Requirement 3.5: Use parallel execution if multiple transactions with disjoint write sets
    static void executeSequence(
        const scheduling::Shard::Sequence& sequence,
        blp::StateCacheManager* cache_manager,
        std::vector<void*>& executed_txs,
        size_t& executed_count,
        size_t& failed_count);
    
    // Execute one sequence with sharding support
    // Requirement 7.1: Apply sharding within conflicting zone sequences
    // Requirement 7.4: Execute shards sequentially by sequence, parallel within sequence
    static void executeSequenceWithSharding(
        const scheduling::Shard::Sequence& sequence,
        blp::StateCacheManager* cache_manager,
        size_t num_shards,
        std::vector<void*>& executed_txs,
        size_t& executed_count,
        size_t& failed_count,
        std::vector<double>& shard_parallelism);
    
    // Create shards from sequence transactions
    // Requirement 7.1: Apply sharding within conflicting zone sequences
    // Requirement 7.2: Ensure same shard transactions have disjoint write sets
    static std::vector<scheduling::Shard> createShards(
        const scheduling::Shard::Sequence& sequence,
        size_t num_shards);
};