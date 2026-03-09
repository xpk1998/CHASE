//
// Created for BLP Two-Layer Cache System
//

#ifndef NEUBLOCKCHAIN_UNCOMMITTED_STATE_CACHE_H
#define NEUBLOCKCHAIN_UNCOMMITTED_STATE_CACHE_H

#include <string>
#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <memory>
#include "../../../utilities/types/aria_types.h"
#include "../database/database.h"  // Add this include for VersionedDB
#include "../../../storage/database/db_storage.h"  // Add this include for DBStorage

namespace blp {

// Forward declaration
class DependencyChain;

/**
 * ChainDiff: Represents state differences for a dependency chain
 * Implements copy-on-write semantics for efficient memory usage
 */
class ChainDiff {
public:
    explicit ChainDiff(uint64_t chain_id) : chain_id_(chain_id), is_copied_(false) {}
    
    // Copy-on-write implementation
    void write(const std::string& table, const std::string& key, const std::string& value);
    
    bool read(const std::string& table, const std::string& key, std::string& value) const;
    
    bool contains(const std::string& table, const std::string& key) const;
    
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& 
        getAllChanges() const { return state_changes_; }
    
    size_t size() const { return size_; }
    
    uint64_t chainId() const { return chain_id_; }
    
    void clear();

private:
    uint64_t chain_id_;
    mutable std::shared_mutex mutex_;
    
    // State storage: table -> (key -> value)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> state_changes_;
    size_t size_ = 0;
    
    // Copy-on-write flag
    bool is_copied_;
    
    // Ensure copy-on-write semantics
    void ensureWritable();
};

/**
 * UncommittedStateCache stores state changes for a single Epoch in the BLP pipeline.
 * 
 * This cache holds uncommitted state modifications during the Execute stage,
 * allowing subsequent Epochs to read these changes before they are committed
 * to the persistent CommittedState layer.
 * 
 * Thread-safety: Uses shared_mutex to support concurrent reads and exclusive writes.
 * 
 * Enhanced for chain-structured storage:
 * - Stores state differences in chain-level granularity
 * - Supports copy-on-write semantics for efficient memory usage
 */
class UncommittedStateCache {
public:
    /**
     * Construct a cache for the specified Epoch.
     * @param epoch The Epoch number this cache belongs to
     */
    explicit UncommittedStateCache(epoch_size_t epoch);
    
    ~UncommittedStateCache() = default;
    
    // Disable copy and move to prevent accidental duplication
    UncommittedStateCache(const UncommittedStateCache&) = delete;
    UncommittedStateCache& operator=(const UncommittedStateCache&) = delete;
    UncommittedStateCache(UncommittedStateCache&&) = delete;
    UncommittedStateCache& operator=(UncommittedStateCache&&) = delete;
    
    /**
     * Get the mutex for this cache.
     * @return Reference to the mutex
     */
    std::shared_mutex& getMutex() { return mutex_; }
    
    /**
     * Write a state value to the cache for a specific chain.
     * If the key already exists, it will be overwritten.
     * 
     * @param chain_id The chain ID
     * @param table The table name
     * @param key The key to write
     * @param value The value to write
     */
    void put(uint64_t chain_id,
             const std::string& table,
             const std::string& key,
             const std::string& value);
    
    /**
     * Read a state value from the cache.
     * Searches in chain-level diffs first, then in the base cache.
     * 
     * @param chain_id The chain ID (can be 0 for general search)
     * @param table The table name
     * @param key The key to read
     * @param value Output parameter to store the value if found
     * @return true if the key exists in the cache, false otherwise
     */
    bool get(uint64_t chain_id,
             const std::string& table,
             const std::string& key,
             std::string& value) const;
    
    /**
     * Check if a key exists in the cache.
     * 
     * @param chain_id The chain ID (can be 0 for general search)
     * @param table The table name
     * @param key The key to check
     * @return true if the key exists, false otherwise
     */
    bool contains(uint64_t chain_id,
                  const std::string& table,
                  const std::string& key) const;
    
    /**
     * Get all state changes stored in this cache.
     * Used during commit to merge changes into CommittedState.
     * 
     * @return Reference to the internal state_changes_ map
     */
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& 
        getAllChanges() const;
    
    /**
     * Get the Epoch number this cache belongs to.
     * @return The Epoch number
     */
    epoch_size_t epoch() const { return epoch_; }
    
    /**
     * Get the total number of state changes in this cache.
     * @return Total number of key-value pairs across all tables
     */
    size_t size() const;
    
    /**
     * Clear all state changes from the cache.
     * Used during rollback operations.
     */
    void clear();
    
    /**
     * Commit all state changes to the database.
     * @param db The database to commit to
     */
    void commit(VersionedDB* db);
    
    /**
     * Add a chain-level difference to the cache.
     * @param chain_diff The chain difference to add
     */
    void addChainDiff(std::unique_ptr<ChainDiff> chain_diff);
    
    /**
     * Get all chain-level differences.
     * @return Vector of chain differences
     */
    const std::vector<std::unique_ptr<ChainDiff>>& getChainDiffs() const { return chain_diffs_; }
    
    /**
     * Batch merge chain differences according to deterministic chain order.
     * This implements the partition synchronization point mechanism.
     * 
     * @param chain_order Vector of chain IDs in deterministic order
     */
    void batchMergeChainDiffs(const std::vector<uint64_t>& chain_order);

private:
    // The Epoch number this cache belongs to
    epoch_size_t epoch_;
    
    // Base state storage: table -> (key -> value)
    // This is for non-chain-specific state changes
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> state_changes_;
    
    // Chain-level differences
    std::vector<std::unique_ptr<ChainDiff>> chain_diffs_;
    
    // Mutex for thread-safe access
    // Uses shared_mutex to allow multiple concurrent readers
    mutable std::shared_mutex mutex_;
};

} // namespace blp

#endif //NEUBLOCKCHAIN_UNCOMMITTED_STATE_CACHE_H