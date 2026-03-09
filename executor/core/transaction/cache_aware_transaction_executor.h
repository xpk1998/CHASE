#ifndef NEUBLOCKCHAIN_CACHE_AWARE_TRANSACTION_EXECUTOR_H
#define NEUBLOCKCHAIN_CACHE_AWARE_TRANSACTION_EXECUTOR_H

#include "../transaction/transaction.h"
#include "sequence_cache.h"
#include "state_cache_manager.h"
#include <string>
#include <memory>
#include <vector>
#include "../../../scheduler/serial/rw_set.h"

namespace blp {

/**
 * ExecutionResult: Result of transaction execution
 */
struct ExecutionResult {
    bool success = false;
    uint64_t gasUsed = 0;
    std::string errorMessage;
    std::vector<std::string> logs;
};

/**
 * CacheAwareTransactionExecutor: Execute transactions with two-layer cache support
 * 
 * This executor integrates transactions with the two-layer cache system:
 * - Reads: from StateCacheManager (L1: UncommittedCache + L2: CommittedState)
 * - Writes: to SequenceCache (sequence-level temporary cache)
 * 
 * Workflow:
 * 1. Transaction reads state via getState() -> StateCacheManager
 * 2. Transaction writes state via putState() -> SequenceCache
 * 3. After sequence completes, SequenceCache is merged to L1 (UncommittedCache)
 * 4. At global sync points, L1 is batch-written to L2 (CommittedState)
 * 
 * BLP (Block Level Parallelism) Integration:
 * - Works with ledger pipeline for batch processing
 * - Integrates with consensus layer (PBFT) for block synchronization
 * - Supports bulk transaction execution in executor module
 * - Configured and initialized in node module
 */
class CacheAwareTransactionExecutor {
public:
    /**
     * Execute a transaction with cache support
     * 
     * @param tx The transaction to execute
     * @param zone_type Zone type (NC_ZONE or C_ZONE)
     * @param sequence_cache Sequence-level cache for writes
     * @param cache_manager State cache manager for reads
     * @return ExecutionResult containing execution status and metrics
     */
    static ExecutionResult execute(
        Transaction* tx,
        ZoneType zone_type,
        SequenceCache* sequence_cache,
        StateCacheManager* cache_manager);
    
    /**
     * Execute a batch of transactions with cache support
     * 
     * @param transactions Vector of transactions to execute
     * @param zone_type Zone type (NC_ZONE or C_ZONE)
     * @param sequence_cache Sequence-level cache for writes
     * @param cache_manager State cache manager for reads
     * @return Vector of ExecutionResult for each transaction
     */
    static std::vector<ExecutionResult> executeBatch(
        const std::vector<Transaction*>& transactions,
        ZoneType zone_type,
        SequenceCache* sequence_cache,
        StateCacheManager* cache_manager);
    
    /**
     * Read state from cache hierarchy
     * 
     * @param zone_type Zone type (NC_ZONE or C_ZONE)
     * @param table Table name
     * @param key Key to read
     * @param value Output parameter for the value
     * @param sequence_cache Sequence-level cache (checked first)
     * @param cache_manager State cache manager (L1 + L2)
     * @return true if value found, false otherwise
     */
    static bool getState(
        ZoneType zone_type,
        const std::string& table,
        const std::string& key,
        std::string& value,
        SequenceCache* sequence_cache,
        StateCacheManager* cache_manager);
    
    /**
     * Write state to sequence cache
     * 
     * @param table Table name
     * @param key Key to write
     * @param value Value to write
     * @param sequence_cache Sequence-level cache for writes
     */
    static void putState(
        const std::string& table,
        const std::string& key,
        const std::string& value,
        SequenceCache* sequence_cache);
    
private:
    /**
     * Execute transaction logic with full implementation
     * 
     * @param tx The transaction to execute
     * @param zone_type Zone type
     * @param sequence_cache Sequence cache for writes
     * @param cache_manager Cache manager for reads
     * @return ExecutionResult with execution status and metrics
     */
    static ExecutionResult executeTransactionLogic(
        Transaction* tx,
        ZoneType zone_type,
        SequenceCache* sequence_cache,
        StateCacheManager* cache_manager);
    
    /**
     * Process read operations in transaction
     * 
     * @param rw_set Read-write set containing read operations
     * @param zone_type Zone type
     * @param sequence_cache Sequence cache for writes
     * @param cache_manager Cache manager for reads
     * @param gas_used Reference to accumulate gas usage
     * @return true if all read operations succeed, false otherwise
     */
    static bool processReadOperations(
        scheduling::KVRWSet* kv_rw_set,
        ZoneType zone_type,
        SequenceCache* sequence_cache,
        StateCacheManager* cache_manager,
        uint64_t& gas_used);
    
    /**
     * Process write operations in transaction
     * 
     * @param rw_set Read-write set containing write operations
     * @param sequence_cache Sequence cache for writes
     * @param gas_used Reference to accumulate gas usage
     */
    static void processWriteOperations(
        scheduling::KVRWSet* kv_rw_set,
        SequenceCache* sequence_cache,
        uint64_t& gas_used);
    
    /**
     * Validate transaction before execution
     * 
     * @param tx The transaction to validate
     * @return true if valid, false otherwise
     */
    static bool validateTransaction(Transaction* tx);
};

} // namespace blp

#endif //NEUBLOCKCHAIN_CACHE_AWARE_TRANSACTION_EXECUTOR_H