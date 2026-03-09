//
// Created for Two-Zone Cache Integration
// CacheAwareTransactionExecutor Implementation
//

#include "cache_aware_transaction_executor.h"
#include <glog/logging.h>
#include "../../../scheduler/serial/rw_set.h"

namespace blp {

ExecutionResult CacheAwareTransactionExecutor::execute(
    Transaction* tx,
    ZoneType zone_type,
    SequenceCache* sequence_cache,
    StateCacheManager* cache_manager) {
    
    if (tx == nullptr) {
        LOG(ERROR) << "CacheAwareTransactionExecutor: Null transaction pointer";
        return {false, 0, "Null transaction pointer", {}};
    }
    
    if (sequence_cache == nullptr) {
        LOG(ERROR) << "CacheAwareTransactionExecutor: Null sequence cache pointer";
        return {false, 0, "Null sequence cache pointer", {}};
    }
    
    if (cache_manager == nullptr) {
        LOG(ERROR) << "CacheAwareTransactionExecutor: Null cache manager pointer";
        return {false, 0, "Null cache manager pointer", {}};
    }
    
    try {
        // Validate transaction before execution
        if (!validateTransaction(tx)) {
            LOG(WARNING) << "CacheAwareTransactionExecutor: Transaction validation failed for transaction " 
                         << tx->getTransactionID();
            return {false, 0, "Transaction validation failed", {}};
        }
        
        // Execute transaction logic with cache support
        ExecutionResult result = executeTransactionLogic(tx, zone_type, sequence_cache, cache_manager);
        
        if (result.success) {
            DLOG(INFO) << "CacheAwareTransactionExecutor: Transaction " 
                      << tx->getTransactionID() << " executed successfully with "
                      << result.gasUsed << " gas used";
        } else {
            LOG(WARNING) << "CacheAwareTransactionExecutor: Transaction " 
                        << tx->getTransactionID() << " execution failed: " << result.errorMessage;
        }
        
        return result;
        
    } catch (const std::exception& e) {
        LOG(ERROR) << "CacheAwareTransactionExecutor: Exception during transaction " 
                  << tx->getTransactionID() << " execution: " << e.what();
        return {false, 0, std::string("Exception: ") + e.what(), {}};
    }
}

std::vector<ExecutionResult> CacheAwareTransactionExecutor::executeBatch(
    const std::vector<Transaction*>& transactions,
    ZoneType zone_type,
    SequenceCache* sequence_cache,
    StateCacheManager* cache_manager) {
    
    std::vector<ExecutionResult> results;
    results.reserve(transactions.size());
    
    for (Transaction* tx : transactions) {
        ExecutionResult result = execute(tx, zone_type, sequence_cache, cache_manager);
        results.push_back(result);
    }
    
    return results;
}

bool CacheAwareTransactionExecutor::getState(
    ZoneType zone_type,
    const std::string& table,
    const std::string& key,
    std::string& value,
    SequenceCache* sequence_cache,
    StateCacheManager* cache_manager) {
    
    // Cache hierarchy for reads:
    // 1. Check SequenceCache (current sequence's writes)
    // 2. Check L1 (UncommittedCache via StateCacheManager)
    // 3. Check L2 (CommittedState via StateCacheManager)
    
    // Step 1: Try to read from SequenceCache first (current sequence's writes)
    if (sequence_cache != nullptr && sequence_cache->get(table, key, value)) {
        DLOG(INFO) << "CacheAwareTransactionExecutor: Read hit in SequenceCache - "
                   << "table=" << table << " key=" << key;
        return true;
    }
    
    // Step 2 & 3: Read from StateCacheManager (L1 + L2)
    if (cache_manager != nullptr && 
        cache_manager->getState(zone_type, table, key, value)) {
        DLOG(INFO) << "CacheAwareTransactionExecutor: Read hit in StateCacheManager - "
                   << "table=" << table << " key=" << key;
        return true;
    }
    
    // Cache miss - value not found
    DLOG(INFO) << "CacheAwareTransactionExecutor: Read miss - "
               << "table=" << table << " key=" << key;
    return false;
}

void CacheAwareTransactionExecutor::putState(
    const std::string& table,
    const std::string& key,
    const std::string& value,
    SequenceCache* sequence_cache) {
    
    if (sequence_cache == nullptr) {
        LOG(ERROR) << "CacheAwareTransactionExecutor: Cannot write to null SequenceCache";
        return;
    }
    
    // Write to SequenceCache (sequence-level temporary cache)
    sequence_cache->put(table, key, value);
    
    DLOG(INFO) << "CacheAwareTransactionExecutor: Write to SequenceCache - "
               << "table=" << table << " key=" << key << " value_size=" << value.size();
}

ExecutionResult CacheAwareTransactionExecutor::executeTransactionLogic(
    Transaction* tx,
    ZoneType zone_type,
    SequenceCache* sequence_cache,
    StateCacheManager* cache_manager) {
    
    if (!tx || !sequence_cache || !cache_manager) {
        LOG(ERROR) << "CacheAwareTransactionExecutor: Invalid parameters for transaction " 
                   << (tx ? tx->getTransactionID() : 0);
        return {false, 0, "Invalid parameters", {}};
    }
    
    DLOG(INFO) << "CacheAwareTransactionExecutor: Executing transaction " 
               << tx->getTransactionID() << " in zone " << static_cast<int>(zone_type);
    
    // Get transaction payload
    auto* payload = tx->getTransactionPayload();
    if (!payload) {
        LOG(ERROR) << "CacheAwareTransactionExecutor: Transaction payload is null for transaction " 
                   << tx->getTransactionID();
        return {false, 0, "Transaction payload is null", {}};
    }
    
    // Track execution metrics
    uint64_t gas_used = 0;
    std::vector<std::string> logs;
    
    // Process read operations
    auto* kv_rw_set = tx->getRWSet();
    if (kv_rw_set) {
        if (!processReadOperations(reinterpret_cast<scheduling::KVRWSet*>(kv_rw_set), zone_type, sequence_cache, cache_manager, gas_used)) {
            return {false, gas_used, "Failed to process read operations", logs};
        }
        
        // Process write operations
        processWriteOperations(reinterpret_cast<scheduling::KVRWSet*>(kv_rw_set), sequence_cache, gas_used);
    }
    
    // Execute the actual transaction logic (EVM execution, etc.)
    // In a real implementation, this would call the EVM executor or other execution engines
    
    // For demonstration purposes, we'll simulate more realistic execution
    // Simulate computation complexity
    gas_used += 500; // Base computation cost
    
    // Simulate potential execution failures
    // In a real implementation, this could be due to:
    // - Insufficient funds
    // - Contract execution errors
    // - Gas limit exceeded
    // - Invalid opcodes
    
    // For this example, we'll assume most transactions succeed
    bool execution_success = (gas_used < 10000); // Arbitrary gas limit
    std::string error_message = "";
    
    if (execution_success) {
        LOG(INFO) << "Transaction " << tx->getTransactionID() << " executed successfully with "
                  << gas_used << " gas used";
        
        // In a real implementation, we would:
        // 1. Update transaction receipt with execution results
        // 2. Record gas usage
        // 3. Handle logs and events
        // 4. Update transaction status
    } else {
        error_message = "Gas limit exceeded";
        LOG(WARNING) << "Transaction " << tx->getTransactionID() << " execution failed with "
                     << gas_used << " gas used";
        
        // In a real implementation, we would:
        // 1. Mark transaction as failed
        // 2. Record error information
        // 3. Refund appropriate gas
        // 4. Handle state reversion if needed
    }
    
    return {execution_success, gas_used, error_message, logs};
}

bool CacheAwareTransactionExecutor::processReadOperations(
    scheduling::KVRWSet* kv_rw_set,
    ZoneType zone_type,
    SequenceCache* sequence_cache,
    StateCacheManager* cache_manager,
    uint64_t& gas_used) {
    
    if (!kv_rw_set) {
        return true; // No read operations to process
    }
    
    // Process read set
    int readsSize = kv_rw_set->reads_size();
    for (int i = 0; i < readsSize; ++i) {
        const ::scheduling::KVRead& read_op = kv_rw_set->reads(i);
        const std::string& table = read_op.table();
        const std::string& key = read_op.key();
        
        std::string value;
        if (getState(zone_type, table, key, value, sequence_cache, cache_manager)) {
            DLOG(INFO) << "CacheAwareTransactionExecutor: Read operation successful - "
                       << "table=" << table << " key=" << key;
            // In a real implementation, we would store the read value for validation
            // and potentially track gas costs
            gas_used += 100; // Simulated gas cost for read operation
        } else {
            DLOG(INFO) << "CacheAwareTransactionExecutor: Read operation failed (key not found) - "
                       << "table=" << table << " key=" << key;
            // Key not found - this might be valid depending on the application logic
            // In some cases, this could cause transaction failure
            // For now, we'll continue processing but track it
            gas_used += 100; // Still charge gas for the attempt
        }
    }
    
    return true;
}

void CacheAwareTransactionExecutor::processWriteOperations(
    scheduling::KVRWSet* kv_rw_set,
    SequenceCache* sequence_cache,
    uint64_t& gas_used) {
    
    if (!kv_rw_set) {
        return; // No write operations to process
    }
    
    // Process write operations
    int writesSize = kv_rw_set->writes_size();
    for (int i = 0; i < writesSize; ++i) {
        const ::scheduling::KVWrite& write_op = kv_rw_set->writes(i);
        const std::string& table = write_op.table();
        const std::string& key = write_op.key();
        const std::string& value = write_op.value();
        
        putState(table, key, value, sequence_cache);
        DLOG(INFO) << "CacheAwareTransactionExecutor: Write operation successful - "
                   << "table=" << table << " key=" << key << " value_size=" << value.size();
        
        // Track gas costs for write operations
        gas_used += 200; // Simulated gas cost for write operation
    }
}

bool CacheAwareTransactionExecutor::validateTransaction(Transaction* tx) {
    if (!tx) {
        return false;
    }
    
    // Basic validation checks
    // In a real implementation, this would include:
    // 1. Signature validation
    // 2. Gas limit validation
    // 3. Nonce validation
    // 4. Balance validation
    // 5. Transaction format validation
    
    // For this example, we'll just check that the transaction has a valid ID
    if (tx->getTransactionID() <= 0) {
        LOG(WARNING) << "CacheAwareTransactionExecutor: Transaction validation failed - invalid transaction ID";
        return false;
    }
    
    return true;
}

} // namespace blp