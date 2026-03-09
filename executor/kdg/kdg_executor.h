#pragma once

#include "address_based_conflict_graph.h"
#include "../pipeline/pipeline_executor.h"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>

namespace parachain {
namespace executor {

// KDG-based transaction executor
class KDGTransactionExecutor : public blp::PipelineExecutor {
public:
    explicit KDGTransactionExecutor(blp::StateCacheManager* cache_manager);
    virtual ~KDGTransactionExecutor() = default;

    // Execute transactions using KDG for dependency management
    void executeTransactions(
        const std::vector<Transaction*>& transactions,
        std::function<void(bool success)> callback);

    // Execute block using KDG
    void executeBlock(epoch_size_t epoch, 
                     const std::vector<Transaction*>& transactions,
                     std::function<void(bool success)> callback) override;

    // Additional KDG-specific methods
    void setKDG(std::shared_ptr<scheduling::AddressBasedConflictGraph> kdg) { m_kdg = kdg; }
    std::shared_ptr<scheduling::AddressBasedConflictGraph> getKDG() const { return m_kdg; }

    // Conflict detection using KDG
    bool hasConflict(size_t tx1_index, size_t tx2_index) const;

    // Get execution statistics
    size_t getExecutedCount() const { return m_executed_count.load(); }
    size_t getFailedCount() const { return m_failed_count.load(); }

private:
    // Execute transactions in parallel respecting KDG dependencies
    void executeInParallel(const std::vector<Transaction*>& transactions,
                          std::function<void(bool success)> callback);

    // Execute transactions sequentially (fallback)
    void executeSequentially(const std::vector<Transaction*>& transactions,
                            std::function<void(bool success)> callback);

    // Process transaction result
    void processTransactionResult(size_t tx_index, bool success);

    // Member variables
    std::shared_ptr<scheduling::AddressBasedConflictGraph> m_kdg;
    blp::StateCacheManager* m_cache_manager;
    std::atomic<size_t> m_executed_count{0};
    std::atomic<size_t> m_failed_count{0};
    mutable std::mutex m_stats_mutex;
};

} // namespace executor
} // namespace parachain