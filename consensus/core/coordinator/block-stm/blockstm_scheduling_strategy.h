#ifndef BLOCKSTM_SCHEDULING_STRATEGY_H
#define BLOCKSTM_SCHEDULING_STRATEGY_H

#include "../serial/scheduling_strategy.h"
#include "../../../../executor/core/simulated_transaction.h"
#include "../../../../executor/address_based_conflict_graph.h"
#include "../../../../executor/kdg/dependency_chain.h"
#include "../../../../executor/kdg/shard.h"
#include <vector>

namespace consensus {
namespace coordinator {
namespace blockstm {

class BlockStmSchedulingStrategy : public scheduling::SchedulingStrategy {
public:
    BlockStmSchedulingStrategy();
    ~BlockStmSchedulingStrategy() override = default;

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
        
    size_t handleAbortedTransactionsWithChains(
        const std::vector<scheduling::AbortedTransaction>& aborted_txs,
        const std::vector<scheduling::DependencyChain>& conflicting_chains,
        blp::StateCacheManager* cache_manager = nullptr) override;
        
    bool validateOptimisticAssumption(
        const std::vector<Transaction*>& re_executed_txs,
        std::vector<Transaction*>& valid_txs,
        std::vector<Transaction*>& invalid_txs) override;

    const char* getName() const override {
        return "BlockSTM";
    }
    
    const char* getDescription() const override {
        return "Block-STM Dynamic OCC-based Scheduling";
    }
    
    bool supportsOptimisticExecution() const override {
        return true;
    }

private:
    // Helper methods for dependency detection
    void detectDependencies(
        const std::vector<scheduling::SimulatedTransaction>& sim_txs,
        std::vector<std::vector<size_t>>& dependencies);

    // Statistics for Block-STM strategy
    struct BlockStmStats {
        size_t re_executions = 0;
        size_t failed_validations = 0;
        size_t successful_validations = 0;
        size_t validation_rounds = 0;
    };
    
    BlockStmStats blockstm_stats_;
};

} // namespace blockstm
} // namespace coordinator
} // namespace consensus

#endif // BLOCKSTM_SCHEDULING_STRATEGY_H