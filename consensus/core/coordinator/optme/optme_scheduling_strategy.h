#pragma once

#include "../serial/scheduling_strategy.h"
#include "../../../../executor/core/simulated_transaction.h"
#include "../../../../executor/address_based_conflict_graph.h"
#include "../../../../executor/kdg/dependency_chain.h"
#include "../../../../executor/kdg/shard.h"
#include "../../../../executor/kdg/dependency_detector.h"
#include <memory>
#include <vector>

namespace consensus {
namespace coordinator {
namespace optme {

// Dependency information structure
struct DependencyInfo {
    size_t tx1_index;
    size_t tx2_index;
    scheduling::DependencyDetector::DependencyType type;
    
    DependencyInfo(size_t idx1, size_t idx2, scheduling::DependencyDetector::DependencyType dep_type)
        : tx1_index(idx1), tx2_index(idx2), type(dep_type) {}
};

class OptmeSchedulingStrategy : public scheduling::SchedulingStrategy {
public:
    OptmeSchedulingStrategy();
    ~OptmeSchedulingStrategy() override = default;

    // Implementation of SchedulingStrategy interface
    std::vector<scheduling::SimulatedTransaction> simulateTransactions(
        const std::vector<Transaction*>& transactions) override;
        
    scheduling::ScheduledInfo buildKdgAndSchedule(
        const std::vector<scheduling::SimulatedTransaction>& sim_txs) override;
        
    bool performSharding(scheduling::ScheduledInfo& schedule_info) override;
        
    size_t executeScheduled(
        const std::vector<std::vector<scheduling::FinalizedTransaction>>& scheduled_txs,
        blp::StateCacheManager* cache_manager = nullptr) override;
        
    size_t executeWithSharding(
        const std::vector<scheduling::Shard>& shards,
        blp::StateCacheManager* cache_manager = nullptr) override;
        
    size_t handleAbortedTransactions(
        const std::vector<scheduling::AbortedTransaction>& aborted_txs,
        blp::StateCacheManager* cache_manager = nullptr) override;

    const char* getName() const override {
        return "OptME";
    }
    
    const char* getDescription() const override {
        return "OptME Static KDG-based Scheduling";
    }
    
    bool supportsSharding() const override {
        return true;
    }

private:
    // Helper methods for dependency analysis
    void analyzeDependencies(
        const std::vector<scheduling::SimulatedTransaction>& sim_txs,
        std::vector<DependencyInfo>& dependencies);
    
    bool hasConflict(
        const scheduling::SimulatedTransaction& tx1,
        const scheduling::SimulatedTransaction& tx2);

    // Statistics for OptME strategy
    struct OptmeStats {
        size_t shard_executions = 0;
        double avg_load_balance = 0.0;
    };
    
    OptmeStats optme_stats_;
};

} // namespace optme
} // namespace coordinator
} // namespace consensus