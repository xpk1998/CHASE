#ifndef NEUBLOCKCHAIN_SCHEDULING_STRATEGY_H
#define NEUBLOCKCHAIN_SCHEDULING_STRATEGY_H

#include <vector>
#include <memory>
#include <cstdint>
#include "../../../executor/core/simulated_transaction.h"
#include "../../../executor/address_based_conflict_graph.h"
#include "../../../executor/kdg/dependency_chain.h"
#include "../../../executor/kdg/shard.h"

class Transaction;

namespace blp {
    class StateCacheManager;
}

namespace scheduling {

// ============================================================================
// Scheduling Strategy Interface
// ============================================================================
class SchedulingStrategy {
public:
    virtual ~SchedulingStrategy() = default;
    
    // ========================================================================
    // Core Scheduling Pipeline Methods
    // ========================================================================
    
    /**
     * Simulate transactions to extract read/write sets
     * 
     * @param transactions Input transactions to simulate
     * @return Vector of simulated transactions with RW sets
     */
    virtual std::vector<SimulatedTransaction> simulateTransactions(
        const std::vector<Transaction*>& transactions) = 0;
    
    /**
     * Build dependency graph and schedule transactions
     * 
     * @param sim_txs Simulated transactions with RW sets
     * @return Scheduled transaction info
     */
    virtual ScheduledInfo buildKdgAndSchedule(
        const std::vector<SimulatedTransaction>& sim_txs) = 0;
    
    /**
     * Perform sharding (optional, strategy-dependent)
     * 
     * @param schedule_info Schedule info to apply sharding to
     * @return true if sharding was performed, false otherwise
     */
    virtual bool performSharding(ScheduledInfo& schedule_info) {
        (void)schedule_info;  // Suppress unused parameter warning
        return false;  // Default: no sharding
    }
    
    /**
     * Execute scheduled transactions (Phase 1)
     * 
     * @param scheduled_txs Scheduled transactions grouped by execution order
     * @param cache_manager Optional state cache manager (e.g., for BLP pipeline)
     * @return Number of successfully executed transactions
     */
    virtual size_t executeScheduled(
        const std::vector<std::vector<FinalizedTransaction>>& scheduled_txs,
        blp::StateCacheManager* cache_manager = nullptr) = 0;
    
    /**
     * Execute shard-based parallel execution (optional)
     * 
     * @param shards Shards to execute in parallel
     * @param cache_manager Optional state cache manager
     * @return Number of successfully executed transactions
     */
    virtual size_t executeWithSharding(
        const std::vector<scheduling::Shard>& shards,
        blp::StateCacheManager* cache_manager = nullptr) {
        (void)shards;  // Suppress unused parameter warning
        (void)cache_manager;  // Suppress unused parameter warning
        return 0;  // Default: no sharding support
    }
    
    /**
     * Handle aborted transactions (Phase 2)
     * Retry or re-execute aborted transactions
     * 
     * @param aborted_txs Transactions that were aborted in Phase 1
     * @param cache_manager Optional state cache manager
     * @return Number of transactions re-executed
     */
    virtual size_t handleAbortedTransactions(
        const std::vector<scheduling::AbortedTransaction>& aborted_txs,
        blp::StateCacheManager* cache_manager = nullptr) = 0;
    
    /**
     * Handle aborted transactions with dependency chain information
     * (Optional, may not be supported by all strategies)
     * 
     * @param aborted_txs Transactions that were aborted
     * @param conflicting_chains Dependency chains for aborted txs
     * @param cache_manager Optional state cache manager
     * @return Number of transactions re-executed
     */
    virtual size_t handleAbortedTransactionsWithChains(
        const std::vector<scheduling::AbortedTransaction>& aborted_txs,
        const std::vector<scheduling::DependencyChain>& conflicting_chains,
        blp::StateCacheManager* cache_manager = nullptr) {
        (void)aborted_txs;  // Suppress unused parameter warning
        (void)conflicting_chains;  // Suppress unused parameter warning
        (void)cache_manager;  // Suppress unused parameter warning
        // Default: fallback to basic aborted transaction handling
        return handleAbortedTransactions(aborted_txs, cache_manager);
    }
    
    /**
     * Validate optimistic assumptions (optional)
     * Verify if speculative/optimistic execution was correct
     * 
     * @param re_executed_txs Transactions that were re-executed
     * @param valid_txs Output: transactions that passed validation
     * @param invalid_txs Output: transactions that failed validation
     * @return true if validation succeeded, false if conflicts detected
     */
    virtual bool validateOptimisticAssumption(
        const std::vector<Transaction*>& re_executed_txs,
        std::vector<Transaction*>& valid_txs,
        std::vector<Transaction*>& invalid_txs) {
        (void)re_executed_txs;  // Suppress unused parameter warning
        (void)valid_txs;  // Suppress unused parameter warning
        (void)invalid_txs;  // Suppress unused parameter warning
        // Default: all transactions are valid
        valid_txs = re_executed_txs;
        invalid_txs.clear();
        return true;
    }
    
    // ========================================================================
    // Configuration and Statistics
    // ========================================================================
    
    /**
     * Get strategy name for logging/debugging
     * @return Human-readable strategy name (e.g., "BLP", "Serial", "Block-STM", "OptME")
     */
    virtual const char* getName() const = 0;
    
    /**
     * Get strategy description
     * @return Human-readable strategy description
     */
    virtual const char* getDescription() const = 0;
    
    /**
     * Check if strategy supports dependency chains
     * @return true if strategy can leverage dependency chain information
     */
    virtual bool supportsDependencyChains() const {
        return false;
    }
    
    /**
     * Check if strategy supports sharding
     * @return true if strategy can perform sharding optimization
     */
    virtual bool supportsSharding() const {
        return false;
    }
    
    /**
     * Check if strategy supports speculative/optimistic execution
     * @return true if strategy performs optimistic execution with validation
     */
    virtual bool supportsOptimisticExecution() const {
        return false;
    }
    
    // ========================================================================
    // Optional Methods for Advanced Features
    // ========================================================================
    
    /**
     * Initialize strategy with configuration
     * May be called before first use
     */
    virtual void initialize() {}
    
    /**
     * Cleanup and finalize strategy
     * May be called before shutdown
     */
    virtual void finalize() {}
};

// ============================================================================
// Strategy Factory
// ============================================================================
/**
 * Factory for creating scheduling strategy instances
 * Supports:
 * - "blp" or "neuchain": NeuChain BLP Flow Pipeline
 * - "serial": Serial Sequential Execution
 * - "blockstm": Block-STM Dynamic Scheduling
 * - "optme": OptME Static Scheduling with KDG
 */
class SchedulingStrategyFactory {
public:
    /**
     * Create a scheduling strategy by name
     * @param name Strategy name (case-insensitive): "blp", "serial", "blockstm", "optme"
     * @return Unique pointer to strategy instance, or nullptr if name not recognized
     */
    static std::unique_ptr<SchedulingStrategy> create(const std::string& name);
    
    /**
     * Check if strategy name is valid
     * @param name Strategy name to check
     * @return true if name is a recognized strategy
     */
    static bool isValidStrategyName(const std::string& name);
    
    /**
     * Get list of available strategy names
     * @return Vector of available strategy names
     */
    static std::vector<std::string> getAvailableStrategies();
};

}  // namespace scheduling

#endif  // NEUBLOCKCHAIN_SCHEDULING_STRATEGY_H