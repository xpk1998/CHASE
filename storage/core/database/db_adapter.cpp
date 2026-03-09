//
// Database Adapter Implementation for New Scheduling Scheme
//

#include "db_adapter.h"
#include "database.h"
#include "../../../executor/transaction/transaction.h"
#include "../../database/db_storage.h"
#include "kv_rwset.pb.h"
#include <algorithm>
#include <unordered_set>

namespace scheduling {

bool DBAdapter::applyWrites(Transaction* tx, Database* db) {
    // Get the RW set from the transaction
    ::KVRWSet* rwset = tx->getRWSet();
    if (!rwset) {
        return false;
    }
    
    // Apply all write operations
    int writesSize = rwset->writes_size();
    for (int i = 0; i < writesSize; ++i) {
        const ::KVWrite& write = rwset->writes(i);
        if (!applyWrite(write.table(), write.key(), write.value(), db)) {
            return false;
        }
    }
    
    return true;
}

bool DBAdapter::applyWrite(const std::string& table,
                          const std::string& key,
                          const std::string& value,
                          Database* db) {
    // Apply the write operation to the database
    // Database doesn't have a put method, we need to get the storage and use it
    DBStorage* storage = db->getStorage();
    if (storage) {
        storage->updateWriteSet(key, value, table);
        return true;
    }
    return false;
}

bool DBAdapter::readValue(const std::string& table,
                         const std::string& key,
                         std::string& value,
                         DBStorage* storage) {
    // Read value from storage
    if (!storage) {
        return false;
    }
    
    // Use the get method from DBStorage
    return storage->get(table, key, value);
}

size_t DBAdapter::batchApplyWrites(const std::vector<Transaction*>& transactions,
                                   Database* db) {
    size_t success_count = 0;
    
    for (auto* tx : transactions) {
        if (applyWrites(tx, db)) {
            success_count++;
        }
    }
    
    return success_count;
}

bool DBAdapter::verifyReadSet(Transaction* tx, DBStorage* storage) {
    ::KVRWSet* rwset = tx->getRWSet();
    if (!rwset) {
        return false;
    }
    
    // Verify all read operations
    int readsSize = rwset->reads_size();
    for (int i = 0; i < readsSize; ++i) {
        const ::KVRead& read = rwset->reads(i);
        std::string current_value;
        // Read the actual value from storage
        if (storage && storage->get(read.table(), read.key(), current_value)) {
            // Key exists, we just verify that it can be read
            // No need to check value for read operations
        } else {
            return false; // Key doesn't exist
        }
    }
    
    return true;
}

std::vector<scheduling::WriteOperation> DBAdapter::extractWriteOperations(const ::KVRWSet* rw_set) {
    std::vector<scheduling::WriteOperation> writeOps;
    
    if (!rw_set) {
        return writeOps;
    }
    
    // Extract write operations from the RW set
    int writesSize = rw_set->writes_size();
    for (int i = 0; i < writesSize; ++i) {
        const ::KVWrite& write = rw_set->writes(i);
        scheduling::WriteOperation op;
        op.table = write.table();
        op.key = write.key();
        op.value = write.value();
        writeOps.push_back(op);
    }
    
    return writeOps;
}

std::vector<std::string> DBAdapter::extractWriteKeys(Transaction* tx) {
    std::vector<std::string> writeKeys;
    
    if (!tx) {
        return writeKeys;
    }
    
    ::KVRWSet* rwset = tx->getRWSet();
    if (!rwset) {
        return writeKeys;
    }
    
    // Extract write keys from the transaction
    int writesSize = rwset->writes_size();
    for (int i = 0; i < writesSize; ++i) {
        const ::KVWrite& write = rwset->writes(i);
        writeKeys.push_back(write.table() + ":" + write.key());
    }
    
    return writeKeys;
}

std::vector<std::string> DBAdapter::extractReadKeys(Transaction* tx) {
    std::vector<std::string> readKeys;
    
    if (!tx) {
        return readKeys;
    }
    
    ::KVRWSet* rwset = tx->getRWSet();
    if (!rwset) {
        return readKeys;
    }
    
    // Extract read keys from the transaction
    int readsSize = rwset->reads_size();
    for (int i = 0; i < readsSize; ++i) {
        const ::KVRead& read = rwset->reads(i);
        readKeys.push_back(read.table() + ":" + read.key());
    }
    
    return readKeys;
}

bool DBAdapter::hasWriteConflict(Transaction* tx1, Transaction* tx2) {
    if (!tx1 || !tx2) {
        return false;
    }
    
    // Extract write keys from both transactions
    auto writeKeys1 = extractWriteKeys(tx1);
    auto writeKeys2 = extractWriteKeys(tx2);
    
    // Convert second vector to unordered_set for O(1) lookup
    std::unordered_set<std::string> writeSet2(writeKeys2.begin(), writeKeys2.end());
    
    // Check for intersection
    for (const auto& key : writeKeys1) {
        if (writeSet2.find(key) != writeSet2.end()) {
            return true; // Conflict found
        }
    }
    
    return false; // No conflict
}

} // namespace scheduling