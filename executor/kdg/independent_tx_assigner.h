//
// Created for New Scheduling Scheme Dependency Chain Sharding
// IndependentTxAssigner: Assigns independent transactions using round-robin
// Requirements: 6.1, 6.2, 6.3
//

#ifndef NEUBLOCKCHAIN_INDEPENDENT_TX_ASSIGNER_H
#define NEUBLOCKCHAIN_INDEPENDENT_TX_ASSIGNER_H

#include <vector>
#include <memory>
#include <cstdint>
#include "shard.h"
#include "../transaction/transaction.h"

namespace scheduling {

// IndependentTxAssigner: Assigns independent transactions to shards
class IndependentTxAssigner {
public:
    explicit IndependentTxAssigner(std::vector<Shard>& shards);
    
    // Assign independent transactions using round-robin
    // Requirement 6.1: Sort transactions by ID ascending
    // Requirement 6.2: Use round-robin to assign to k shards
    // Requirement 6.3: Update shard load
    void assignIndependentTransactions(
        const std::vector<std::shared_ptr<TransactionNode>>& independent_txs);

private:
    std::vector<Shard>& shards_;
    uint32_t current_shard_index_;  // Current shard index for round-robin
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_INDEPENDENT_TX_ASSIGNER_H