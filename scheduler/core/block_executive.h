#pragma once

#include "../../framework/protocol/Block.h"
#include "../../framework/protocol/Transaction.h"
#include "../../executor/kdg/address_based_conflict_graph.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>

namespace parachain {

class BlockExecutive {
public:
    explicit BlockExecutive(std::shared_ptr<Block> block);
    virtual ~BlockExecutive() = default;

    // Get block
    std::shared_ptr<Block> getBlock() const { return m_block; }

    // Prepare execution - build KDG and schedule transactions
    void prepareExecution();

    // Execute asynchronously
    void asyncExecute(std::function<void(bool success)> callback);

    // Execute a specific transaction
    bool executeTransaction(size_t tx_index);

    // Get execution results
    std::vector<bool> getExecutionResults() const;

    // Get KDG for this block
    std::shared_ptr<scheduling::AddressBasedConflictGraph> getKDG() const { return m_kdg; }

    // Get block number
    BlockNumber getBlockNumber() const { return m_block->getBlockHeader()->getNumber(); }

    // Check execution status
    bool isExecuted() const { return m_executed.load(); }
    bool isCommitted() const { return m_committed.load(); }

    // Set cache manager
    void setCacheManager(std::shared_ptr<blp::StateCacheManager> cache_manager) { m_cache_manager = cache_manager; }

    // Get execution statistics
    size_t getTransactionCount() const { return m_block->getTransactionCount(); }

private:
    // Build KDG from block transactions
    void buildKDG();

    // Schedule transactions based on KDG
    void scheduleTransactions();

    // Execute transactions in parallel using KDG
    void executeWithKDG(std::function<void(bool success)> callback);

    // Member variables
    std::shared_ptr<Block> m_block;
    std::shared_ptr<scheduling::AddressBasedConflictGraph> m_kdg;
    std::vector<bool> m_execution_results;
    std::atomic<bool> m_executed{false};
    std::atomic<bool> m_committed{false};
    std::atomic<bool> m_running{false};
    mutable std::mutex m_mutex;
    std::shared_ptr<blp::StateCacheManager> m_cache_manager;
    std::vector<std::shared_ptr<Transaction>> m_transactions;
};

} // namespace parachain