#pragma once

#include "scheduling_strategy.h"
#include <memory>

namespace consensus {
namespace coordinator {
namespace serial {

class SerialSchedulingStrategy : public scheduling::SchedulingStrategy {
public:
    SerialSchedulingStrategy();
    ~SerialSchedulingStrategy() override = default;

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
        return "Serial";
    }
    
    const char* getDescription() const override {
        return "Serial Sequential Execution";
    }

private:
    // Statistics for Serial strategy
    struct SerialStats {
        size_t transactions_executed = 0;
    };
    
    SerialStats serial_stats_;
};

} // namespace serial
} // namespace coordinator
} // namespace consensus