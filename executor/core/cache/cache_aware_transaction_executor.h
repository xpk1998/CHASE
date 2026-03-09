//
// Created for Two-Zone Cache Integration
// CacheAwareTransactionExecutor: Executes transactions with cache support
//

#ifndef NEUBLOCKCHAIN_CACHE_AWARE_TRANSACTION_EXECUTOR_H
#define NEUBLOCKCHAIN_CACHE_AWARE_TRANSACTION_EXECUTOR_H

#include "../transaction/transaction.h"
#include "sequence_cache.h"
#include "state_cache_manager.h"
#include <string>

namespace blp {

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
     * @return true if execution succeeds, false otherwise
     */
    static bool execute(
        Transaction* tx,
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
     * Execute transaction logic (placeholder for actual implementation)
     * In a real implementation, this would:
     * 1. Parse transaction payload
     * 2. Execute smart contract/business logic
     * 3. Use getState() for reads and putState() for writes
     * 4. Return execution result
     * 
     * @param tx The transaction to execute
     * @param zone_type Zone type
     * @param sequence_cache Sequence cache for writes
     * @param cache_manager Cache manager for reads
     * @return true if execution succeeds
     */
    static bool executeTransactionLogic(
        Transaction* tx,
        ZoneType zone_type,
        SequenceCache* sequence_cache,
        StateCacheManager* cache_manager);
};

} // namespace blp

#endif //NEUBLOCKCHAIN_CACHE_AWARE_TRANSACTION_EXECUTOR_H
