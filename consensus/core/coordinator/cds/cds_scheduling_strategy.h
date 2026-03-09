#pragma once

#include "../serial/scheduling_strategy.h"
#include "../../../../executor/core/simulated_transaction.h"
#include "../../../../executor/address_based_conflict_graph.h"
#include <memory>
#include <vector>

namespace consensus {
namespace coordinator {
namespace lcdps {

class LcdpsSchedulingStrategy : public scheduling::SchedulingStrategy {
public:
    LcdpsSchedulingStrategy();
    ~LcdpsSchedulingStrategy() override = default;

    // Implementation of SchedulingStrategy interface
    std::vector<scheduling::SimulatedTransaction> simulateTransactions(
        const std::vector<Transaction*>& transactions) override;
        
    scheduling::ScheduledInfo buildKdgAndSchedule(
        const std::vector<scheduling::SimulatedTransaction>& sim_txs) override;
        
    size_t executeScheduled(
        const std::vector<std::vector<scheduling::FinalizedTransaction>>& scheduled_txs,
        blp::StateCacheManager* cache_manager = nullptr) override;
        
    size_t handleAbortedTransactions(
        const std::vector<scheduling::AbortedTransaction>& aborted_txs,
        blp::StateCacheManager* cache_manager = nullptr) override;

    const char* getName() const override {
        return "LCDPS";
    }
    
    const char* getDescription() const override {
        return "LCDPS Leader-based Conflict Detection Scheduling";
    }

private:
    // Helper methods for conflict graph construction
    void buildConflictGraph(
        const std::vector<scheduling::SimulatedTransaction>& sim_txs,
        std::vector<std::vector<int>>& conflict_graph);
    
    bool hasReadWriteConflict(
        const scheduling::SimulatedTransaction& tx1,
        const scheduling::SimulatedTransaction& tx2);
    
    bool hasWriteReadConflict(
        const scheduling::SimulatedTransaction& tx1,
        const scheduling::SimulatedTransaction& tx2);
    
    bool hasWriteWriteConflict(
        const scheduling::SimulatedTransaction& tx1,
        const scheduling::SimulatedTransaction& tx2);

    // Statistics for LCDPS strategy
    struct LcdpsStats {
        size_t re_executions = 0;
        size_t aborted_transactions = 0;
    };
    
    LcdpsStats lcdps_stats_;
};

} // namespace lcdps
} // namespace coordinator
} // namespace consensus