//
// Created for Enhanced Two-Layer Cache System
// SequenceCache: Temporary cache for sequence-level execution (c1/c2)
//

#ifndef NEUBLOCKCHAIN_SEQUENCE_CACHE_H
#define NEUBLOCKCHAIN_SEQUENCE_CACHE_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include "../../../utilities/types/aria_types.h"

namespace blp {

/**
 * SequenceCache: Temporary cache for a single sequence execution
 * Used as c1 (non-conflicting zone) or c2 (conflicting zone)
 * 
 * Workflow:
 * 1. Transactions in a sequence write to this temporary cache
 * 2. When sequence commits, merge to L1 (UncommittedCache)
 * 3. Clear this cache for next sequence
 * 
 * Thread-safety: Uses mutex for exclusive access per sequence
 * 
 * Enhanced for chain-structured storage:
 * - Supports chain-level state changes
 * - Maintains chain ID for each state change
 */
class SequenceCache {
public:
    explicit SequenceCache(uint32_t sequence_id);
    ~SequenceCache() = default;
    
    // Disable copy and move
    SequenceCache(const SequenceCache&) = delete;
    SequenceCache& operator=(const SequenceCache&) = delete;
    SequenceCache(SequenceCache&&) = delete;
    SequenceCache& operator=(SequenceCache&&) = delete;
    
    /**
     * Write a state value to the sequence cache.
     * @param table The table name
     * @param key The key to write
     * @param value The value to write
     */
    void put(const std::string& table,
             const std::string& key,
             const std::string& value);
    
    /**
     * Write a state value to the sequence cache with chain ID.
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
     * Read a state value from the sequence cache.
     * @param table The table name
     * @param key The key to read
     * @param value Output parameter
     * @return true if found, false otherwise
     */
    bool get(const std::string& table,
             const std::string& key,
             std::string& value) const;
    
    /**
     * Get all state changes for merging to L1.
     * @return Reference to the internal state map
     */
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& 
        getAllChanges() const;
    
    /**
     * Get all chain-level state changes for merging to L1.
     * @return Reference to the internal chain-level state map
     */
    const std::unordered_map<uint64_t, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>&
        getAllChainChanges() const;
    
    /**
     * Clear all state changes after merging to L1.
     */
    void clear();
    
    /**
     * Get the total number of state changes.
     * @return Total number of key-value pairs
     */
    size_t size() const;
    
    /**
     * Get sequence ID.
     * @return The sequence ID
     */
    uint32_t sequenceId() const { return sequence_id_; }
    
private:
    uint32_t sequence_id_;
    
    // State storage: table -> (key -> value)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> state_changes_;
    
    // Chain-level state storage: chain_id -> (table -> (key -> value))
    std::unordered_map<uint64_t, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> chain_state_changes_;
    
    // Mutex for thread-safe access
    mutable std::mutex mutex_;
};

} // namespace blp

#endif //NEUBLOCKCHAIN_SEQUENCE_CACHE_H