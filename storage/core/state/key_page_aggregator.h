//
// Created for Enhanced Two-Layer Cache System
// KeyPageAggregator: Aggregates sequence updates for batch I/O optimization
//

#ifndef NEUBLOCKCHAIN_KEY_PAGE_AGGREGATOR_H
#define NEUBLOCKCHAIN_KEY_PAGE_AGGREGATOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <list>
#include "../../../utilities/types/aria_types.h"

namespace blp {

/**
 * KeyPage: A page containing aggregated key-value updates
 * Reduces random I/O by batching updates from multiple sequences
 * Implements fixed-size pages for better memory management
 */
struct KeyPage {
    std::string table;  // Table name
    std::vector<std::pair<std::string, std::string>> entries;  // (key, value) pairs
    uint64_t page_id;  // Unique page identifier
    uint64_t version;  // Page version for copy-on-write semantics
    
    size_t size() const { return entries.size(); }
    
    void addEntry(const std::string& key, const std::string& value) {
        entries.emplace_back(key, value);
    }
    
    void clear() {
        entries.clear();
    }
};

/**
 * KeyPageAggregator: Aggregates state updates into KeyPages
 * 
 * Purpose:
 * - Reduce random I/O by grouping updates by table
 * - Optimize sequential write patterns
 * - Minimize lock overhead during batch commit
 * - Support fixed-size pages for better memory management
 * - Implement copy-on-write semantics for efficient versioning
 * 
 * Usage:
 * 1. Accumulate sequence updates via addSequenceUpdates()
 * 2. Call getKeyPages() to retrieve aggregated pages
 * 3. Write all pages in batch to L2 cache
 * 4. Call clear() to reset for next batch
 */
class KeyPageAggregator {
public:
    /**
     * Constructor with configurable page size
     * @param max_page_size Maximum number of entries per page (default: 100)
     * @param enable_copy_on_write Whether to enable copy-on-write semantics (default: true)
     */
    explicit KeyPageAggregator(size_t max_page_size = 100, bool enable_copy_on_write = true);
    ~KeyPageAggregator() = default;
    
    /**
     * Add a single update to the aggregator.
     * @param table The table name
     * @param key The key
     * @param value The value
     */
    void addUpdate(const std::string& table, const std::string& key, const std::string& value);
    
    /**
     * Add updates from a sequence to the aggregator.
     * @param sequence_id The sequence ID
     * @param updates Map of table -> (key -> value)
     */
    void addSequenceUpdates(
        uint32_t sequence_id,
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& updates);
    
    /**
     * Add chain-level updates to the aggregator.
     * This supports the chain-structured storage model.
     * @param chain_id The chain ID
     * @param updates Map of table -> (key -> value)
     */
    void addChainUpdates(
        uint64_t chain_id,
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& updates);
    
    /**
     * Get all aggregated KeyPages for batch writing.
     * @return Vector of KeyPages grouped by table
     */
    std::vector<KeyPage> getKeyPages() const;
    
    /**
     * Clear all accumulated updates.
     */
    void clear();
    
    /**
     * Get total number of entries across all pages.
     * @return Total entry count
     */
    size_t totalEntries() const;
    
    /**
     * Get number of tables with updates.
     * @return Table count
     */
    size_t tableCount() const { return pages_.size(); }
    
    /**
     * Get the current page ID counter.
     * @return Current page ID
     */
    uint64_t getCurrentPageId() const { return page_id_counter_; }
    
    /**
     * Set the page version for copy-on-write semantics.
     * @param version The page version
     */
    void setPageVersion(uint64_t version) { page_version_ = version; }
    
private:
    // Maximum number of entries per page
    size_t max_page_size_;
    
    // Whether to enable copy-on-write semantics
    bool enable_copy_on_write_;
    
    // Current page version for copy-on-write
    uint64_t page_version_;
    
    // Page ID counter for unique identification
    mutable uint64_t page_id_counter_;
    
    // Map of table -> list of KeyPages (fixed-size pages)
    // Using list instead of vector for efficient insertion at the end
    std::unordered_map<std::string, std::list<KeyPage>> pages_;
    
    // Map of chain ID -> table -> list of KeyPages
    // This supports the chain-structured storage model
    std::unordered_map<uint64_t, std::unordered_map<std::string, std::list<KeyPage>>> chain_pages_;
};

} // namespace blp

#endif //NEUBLOCKCHAIN_KEY_PAGE_AGGREGATOR_H