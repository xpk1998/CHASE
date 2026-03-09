//
// Created for New Scheduling Scheme Dependency Chain Sharding
// ShardAssigner: Assigns dependency chains to shards
// Requirements: 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.3, 5.4
//

#ifndef NEUBLOCKCHAIN_SHARD_ASSIGNER_H
#define NEUBLOCKCHAIN_SHARD_ASSIGNER_H

#include <vector>
#include <queue>
#include <cstdint>
#include "shard.h"
#include "dependency_chain.h"

namespace scheduling {

// ShardAssigner: Assigns dependency chains to shards using greedy strategy
class ShardAssigner {
public:
    explicit ShardAssigner(std::vector<Shard>& shards);
    
    // Initial assignment: Assign first k chains to k shards
    // Requirement 4.1, 4.2: Handle l < k and l >= k cases
    // Requirement 4.3: Assign to appropriate zone based on chain type
    // Requirement 4.4: Update shard load
    void initialAssignment(const std::vector<DependencyChain>& sorted_chains);
    
    // Greedy assignment: Assign remaining chains to min-load shard
    // Requirement 5.1: Process each unassigned chain
    // Requirement 5.2: Select min-load shard
    // Requirement 5.3: Assign to appropriate zone and update load
    // Requirement 5.4: Keep chain atomic (no splitting)
    void greedyAssignment(const std::vector<DependencyChain>& remaining_chains);
    
    // Assign single chain to specified shard
    void assignChainToShard(const DependencyChain& chain, uint32_t shard_id);
    
    // Get min-load shard ID
    // Requirement 5.2: Find shard with minimum load
    uint32_t getMinLoadShardId() const;

private:
    // Rebuild min-load heap after load changes
    void rebuildMinLoadHeap();
    
    std::vector<Shard>& shards_;
    
    // Min-heap for efficient min-load shard query: O(log k)
    // Pair: (load, shard_id)
    std::priority_queue<std::pair<uint64_t, uint32_t>,
                       std::vector<std::pair<uint64_t, uint32_t>>,
                       std::greater<>> min_load_heap_;
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_SHARD_ASSIGNER_H
