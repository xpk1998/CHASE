#include "serial_scheduling_strategy.h"
#include <glog/logging.h>

namespace consensus {
namespace coordinator {
namespace serial {

SerialSchedulingStrategy::SerialSchedulingStrategy() {
    LOG(INFO) << "Serial Scheduling Strategy initialized";
}

std::vector<scheduling::SimulatedTransaction> SerialSchedulingStrategy::simulateTransactions(
    const std::vector<Transaction*>& transactions) {
    
    std::vector<scheduling::SimulatedTransaction> sim_txs;
    sim_txs.reserve(transactions.size());
    
    // Even for serial execution, we simulate transactions to extract read/write sets
    // This allows for consistent interfaces across all scheduling strategies
    
    for (size_t i = 0; i < transactions.size(); ++i) {
        scheduling::SimulatedTransaction sim_tx(i, transactions[i]);
        
        // For serial execution, we still need to simulate read/write sets
        // but we won't use them for scheduling since everything is sequential
        sim_tx.simulation_success = true;
        sim_tx.gas_used = 1000; // Simulated gas usage
        
        // Extract read/write sets from transaction simulation
        // In a real implementation, these would come from actual transaction execution
        // For demonstration, we'll create realistic read/write patterns
        
        // Simulate reading account balances
        std::string account_key = "account_" + std::to_string(i % 10);
        sim_tx.rw_set.addRead("accounts", account_key, "balance_value");
        
        // Simulate reading contract code
        std::string contract_key = "contract_" + std::to_string(i % 5);
        sim_tx.rw_set.addRead("contracts", contract_key, "contract_code");
        
        // Simulate writing updated account balances
        std::string updated_account_key = "account_" + std::to_string((i + 1) % 10);
        sim_tx.rw_set.addWrite("accounts", updated_account_key, "updated_balance");
        
        // Simulate writing contract storage
        std::string storage_key = "storage_" + std::to_string(i % 3);
        sim_tx.rw_set.addWrite("storage", storage_key, "storage_value");
        
        sim_txs.push_back(std::move(sim_tx));
    }
    
    LOG(INFO) << "Simulated " << sim_txs.size() << " transactions for Serial scheduling";
    return sim_txs;
}

scheduling::ScheduledInfo SerialSchedulingStrategy::buildKdgAndSchedule(
    const std::vector<scheduling::SimulatedTransaction>& sim_txs) {
    
    scheduling::ScheduledInfo schedule_info;
    
    // For serial execution, all transactions are in a single group
    // executed one after another in the original order
    std::vector<scheduling::FinalizedTransaction> sequence;
    sequence.reserve(sim_txs.size());
    
    for (const auto& sim_tx : sim_txs) {
        if (sim_tx.isValid()) {
            // In serial execution, each transaction gets the same group ID
            // and they are executed in the order they appear
            scheduling::FinalizedTransaction finalized_tx(sim_tx.tx_id, 0, sim_tx.raw_transaction);
            sequence.push_back(finalized_tx);
        }
    }
    
    // Put all transactions in a single sequence to ensure sequential execution
    if (!sequence.empty()) {
        schedule_info.non_conflicting_zone_txs.push_back(sequence);
    }
    
    LOG(INFO) << "Built schedule for " << sequence.size() << " transactions in Serial strategy";
    
    return schedule_info;
}

size_t SerialSchedulingStrategy::executeScheduled(
    const std::vector<std::vector<scheduling::FinalizedTransaction>>& scheduled_txs,
    blp::StateCacheManager* cache_manager) {
    
    size_t executed_count = 0;
    
    // Execute each group sequentially
    // In serial execution, all transactions are executed one by one
    for (const auto& group : scheduled_txs) {
        // In serial execution, each group is executed sequentially
        for (size_t i = 0; i < group.size(); ++i) {
            const auto& tx = group[i];
            
            // Execute the transaction using the cache-aware executor
            // In serial execution, this is straightforward since there are no concurrency concerns
            
            // For demonstration, we'll simulate execution
            if (cache_manager) {
                // Use cache manager for state access if provided
            }
            
            executed_count++;
            serial_stats_.transactions_executed++;
        }
    }
    
    LOG(INFO) << "Executed " << executed_count << " transactions using Serial strategy";
    return executed_count;
}

size_t SerialSchedulingStrategy::handleAbortedTransactions(
    const std::vector<scheduling::AbortedTransaction>& aborted_txs,
    blp::StateCacheManager* cache_manager) {
    
    // In serial execution, aborted transactions are rare but possible
    // This might happen due to:
    // 1. State inconsistencies
    // 2. Contract execution failures
    // 3. Gas limit exceeded
    
    size_t re_executed_count = 0;
    
    for (const auto& aborted_tx : aborted_txs) {
        // In serial execution, re-execution is straightforward
        // since there are no concurrency concerns
        
        // For demonstration, we'll simulate re-execution
        if (cache_manager) {
            // Use cache manager for state access if provided
        }
        
        re_executed_count++;
    }
    
    LOG(INFO) << "Handling " << aborted_txs.size() << " aborted transactions in Serial strategy, "
              << re_executed_count << " re-executed";
    return re_executed_count;
}

}  // namespace serial
}  // namespace coordinator
}  // namespace consensus