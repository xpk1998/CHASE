//
// Database Adapter for New Scheduling Scheme
// Provides a unified interface for database operations
//

#ifndef NEUBLOCKCHAIN_DB_ADAPTER_H
#define NEUBLOCKCHAIN_DB_ADAPTER_H

#include <string>
#include <vector>
#include <memory>

// Include complete definitions instead of forward declarations
#include "../../../executor/transaction/transaction.h"
#include "../../../storage/database/storage_interface.h"
#include "../../../storage/database/db_storage.h"
#include "../../../storage/core/database/database.h"
#include "../../../build/proto_gen/kv_rwset.pb.h"
#include "kv_rwset.pb.h"

namespace scheduling {

/**
 * @brief Write operation structure
 * 
 * Represents a single write operation extracted from a transaction's RW set
 */
struct WriteOperation {
    std::string table;  // Table name
    std::string key;    // Key
    std::string value;  // Value to write
    
    WriteOperation() = default;
    
    WriteOperation(const std::string& t, const std::string& k, const std::string& v)
        : table(t), key(k), value(v) {}
};

/**
 * @brief Database Adapter for Scheduling System
 * 
 * This class provides a bridge between the execution model
 * and NeuChain's database system.
 */
class DBAdapter {
public:
    /**
     * @brief Apply transaction's write set to database
     * 
     * @param tx Transaction to apply
     * @param db Database instance
     * @return true if successful, false otherwise
     */
    static bool applyWrites(Transaction* tx, Database* db);
    
    /**
     * @brief Apply a single write operation
     * 
     * @param table Table name
     * @param key Key
     * @param value Value
     * @param db Database instance
     * @return true if successful
     */
    static bool applyWrite(const std::string& table,
                          const std::string& key,
                          const std::string& value,
                          Database* db);
    
    /**
     * @brief Read a value from database
     * 
     * @param table Table name
     * @param key Key
     * @param value Output value
     * @param storage Storage instance
     * @return true if key exists
     */
    static bool readValue(const std::string& table,
                         const std::string& key,
                         std::string& value,
                         DBStorage* storage);
    
    /**
     * @brief Batch apply writes
     * 
     * @param transactions List of transactions
     * @param db Database instance
     * @return Number of successfully applied transactions
     */
    static size_t batchApplyWrites(const std::vector<Transaction*>& transactions,
                                   Database* db);
    
    /**
     * @brief Verify read set against current database state
     * 
     * @param tx Transaction to verify
     * @param storage Storage instance
     * @return true if read set is still valid
     */
    static bool verifyReadSet(Transaction* tx, DBStorage* storage);
    
    /**
     * @brief Extract write operations from transaction's RW set
     * 
     * @param rw_set Transaction's RW set
     * @return Vector of write operations
     */
    static std::vector<WriteOperation> extractWriteOperations(const ::KVRWSet* rw_set);
    
    /**
     * @brief Extract write keys from transaction
     * 
     * @param tx Transaction
     * @return Set of "table:key" strings
     */
    static std::vector<std::string> extractWriteKeys(Transaction* tx);
    
    /**
     * @brief Extract read keys from transaction
     * 
     * @param tx Transaction
     * @return Set of "table:key" strings
     */
    static std::vector<std::string> extractReadKeys(Transaction* tx);
    
    /**
     * @brief Check if two transactions have conflicting writes
     * 
     * @param tx1 First transaction
     * @param tx2 Second transaction
     * @return true if they conflict
     */
    static bool hasWriteConflict(Transaction* tx1, Transaction* tx2);
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_DB_ADAPTER_H