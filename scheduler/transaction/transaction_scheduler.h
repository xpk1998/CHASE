#pragma once

#include "../interface/scheduler_interface.h"
#include "../../executor/kdg/address_based_conflict_graph.h"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>

namespace parachain {

// Transaction scheduler interface
class TransactionSchedulerInterface {
public:
    virtual ~TransactionSchedulerInterface() = default;

    // Schedule transactions using KDG
    virtual void scheduleTransactions(
        const std::vector<std::shared_ptr<Transaction>>& transactions,
        std::function<void(std::shared_ptr<scheduling::ScheduledInfo>)> callback) = 0;

    // Execute scheduled transactions
    virtual void executeScheduled(
        const std::shared_ptr<scheduling::ScheduledInfo>& schedule_info,
        std::function<void(bool success)> callback) = 0;

    // Handle aborted transactions
    virtual void handleAbortedTransactions(
        const std::vector<size_t>& aborted_txs,
        std::function<void(bool success)> callback) = 0;
};

// Parallel transaction scheduler implementation using KDG
class ParallelTransactionScheduler : public TransactionSchedulerInterface {
public:
    explicit ParallelTransactionScheduler();
    virtual ~ParallelTransactionScheduler() = default;

    // TransactionSchedulerInterface implementation
    void scheduleTransactions(
        const std::vector<std::shared_ptr<Transaction>>& transactions,
        std::function<void(std::shared_ptr<scheduling::ScheduledInfo>)> callback) override;

    void executeScheduled(
        const std::shared_ptr<scheduling::ScheduledInfo>& schedule_info,
        std::function<void(bool success)> callback) override;

    void handleAbortedTransactions(
        const std::vector<size_t>& aborted_txs,
        std::function<void(bool success)> callback) override;

private:
    // Build KDG from transactions
    std::shared_ptr<scheduling::AddressBasedConflictGraph> buildKDG(
        const std::vector<std::shared_ptr<Transaction>>& transactions);

    // Schedule transactions based on KDG
    std::shared_ptr<scheduling::ScheduledInfo> scheduleWithKDG(
        std::shared_ptr<scheduling::AddressBasedConflictGraph> kdg,
        const std::vector<std::shared_ptr<Transaction>>& transactions);

    // Execute transactions in parallel respecting KDG dependencies
    void executeInParallel(
        const std::shared_ptr<scheduling::ScheduledInfo>& schedule_info,
        std::function<void(bool success)> callback);

    // Member variables
    size_t m_num_threads;
    std::atomic<size_t> m_transaction_count{0};
    std::atomic<size_t> m_executed_count{0};
    std::atomic<size_t> m_failed_count{0};
};

// Serial transaction scheduler implementation
class SerialTransactionScheduler : public TransactionSchedulerInterface {
public:
    explicit SerialTransactionScheduler();
    virtual ~SerialTransactionScheduler() = default;

    // TransactionSchedulerInterface implementation
    void scheduleTransactions(
        const std::vector<std::shared_ptr<Transaction>>& transactions,
        std::function<void(std::shared_ptr<scheduling::ScheduledInfo>)> callback) override;

    void executeScheduled(
        const std::shared_ptr<scheduling::ScheduledInfo>& schedule_info,
        std::function<void(bool success)> callback) override;

    void handleAbortedTransactions(
        const std::vector<size_t>& aborted_txs,
        std::function<void(bool success)> callback) override;

private:
    // Execute transactions sequentially
    void executeSequentially(
        const std::shared_ptr<scheduling::ScheduledInfo>& schedule_info,
        std::function<void(bool success)> callback);

    // Member variables
    std::atomic<size_t> m_transaction_count{0};
    std::atomic<size_t> m_executed_count{0};
    std::atomic<size_t> m_failed_count{0};
};

} // namespace parachain