//
// UncommittedStateCache Implementation for BLP Two-Layer Cache System
//

#include "uncommitted_state_cache.h"
#include "../database/database.h"
//#include "database/db_storage.h"
#include <glog/logging.h>
#include <mutex>

namespace blp {

UncommittedStateCache::UncommittedStateCache(epoch_size_t epoch) : epoch_(epoch) {}

void UncommittedStateCache::put(uint64_t chain_id,
                               const std::string& table,
                               const std::string& key,
                               const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // If chain_id is 0, treat as non-chain-specific state change
    if (chain_id == 0) {
        state_changes_[table][key] = value;
        return;
    }
    
    // Find or create chain diff for the specified chain
    ChainDiff* chain_diff = nullptr;
    for (auto& diff : chain_diffs_) {
        if (diff->chainId() == chain_id) {
            chain_diff = diff.get();
            break;
        }
    }
    
    // If chain diff doesn't exist, create it
    if (!chain_diff) {
        chain_diffs_.emplace_back(std::make_unique<ChainDiff>(chain_id));
        chain_diff = chain_diffs_.back().get();
    }
    
    // Add the state change to the chain diff
    chain_diff->write(table, key, value);
}

bool UncommittedStateCache::get(uint64_t chain_id,
                               const std::string& table,
                               const std::string& key,
                               std::string& value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // First, check chain-level diffs (in reverse order for proper precedence)
    for (auto it = chain_diffs_.rbegin(); it != chain_diffs_.rend(); ++it) {
        // If chain_id is specified, only check that chain
        if (chain_id != 0 && (*it)->chainId() != chain_id) {
            continue;
        }
        
        if ((*it)->read(table, key, value)) {
            return true;
        }
    }
    
    // Then check base state changes
    auto table_it = state_changes_.find(table);
    if (table_it != state_changes_.end()) {
        auto key_it = table_it->second.find(key);
        if (key_it != table_it->second.end()) {
            value = key_it->second;
            return true;
        }
    }
    
    return false;
}

bool UncommittedStateCache::contains(uint64_t chain_id,
                                    const std::string& table,
                                    const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // First, check chain-level diffs
    for (const auto& diff : chain_diffs_) {
        // If chain_id is specified, only check that chain
        if (chain_id != 0 && diff->chainId() != chain_id) {
            continue;
        }
        
        if (diff->contains(table, key)) {
            return true;
        }
    }
    
    // Then check base state changes
    auto table_it = state_changes_.find(table);
    if (table_it != state_changes_.end()) {
        return table_it->second.find(key) != table_it->second.end();
    }
    
    return false;
}

const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& 
UncommittedStateCache::getAllChanges() const {
    return state_changes_;
}

size_t UncommittedStateCache::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t total = 0;
    
    // Count base state changes
    for (const auto& table_entry : state_changes_) {
        total += table_entry.second.size();
    }
    
    // Count chain-level diffs
    for (const auto& diff : chain_diffs_) {
        total += diff->size();
    }
    
    return total;
}

void UncommittedStateCache::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    state_changes_.clear();
    chain_diffs_.clear();
}

void UncommittedStateCache::commit(VersionedDB* db) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (!db) {
        LOG(ERROR) << "Cannot commit to null database";
        return;
    }
    
    // Apply base state changes directly to the database
    for (const auto& table_entry : state_changes_) {
        const std::string& table = table_entry.first;
        for (const auto& key_entry : table_entry.second) {
            const std::string& key = key_entry.first;
            const std::string& value = key_entry.second;
            db->updateWriteSet(key, value, table);
        }
    }
    
    // Apply chain-level diffs
    for (const auto& diff : chain_diffs_) {
        const auto& changes = diff->getAllChanges();
        for (const auto& table_entry : changes) {
            const std::string& table = table_entry.first;
            for (const auto& key_entry : table_entry.second) {
                const std::string& key = key_entry.first;
                const std::string& value = key_entry.second;
                db->updateWriteSet(key, value, table);
            }
        }
    }
    
    // Clear the cache after successful commit
    state_changes_.clear();
    chain_diffs_.clear();
}

void UncommittedStateCache::addChainDiff(std::unique_ptr<ChainDiff> chain_diff) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    chain_diffs_.push_back(std::move(chain_diff));
}

void UncommittedStateCache::batchMergeChainDiffs(const std::vector<uint64_t>& chain_order) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // Create a map for quick lookup of chain order
    std::unordered_map<uint64_t, size_t> chain_order_map;
    for (size_t i = 0; i < chain_order.size(); ++i) {
        chain_order_map[chain_order[i]] = i;
    }
    
    // Sort chain diffs according to the deterministic chain order
    std::sort(chain_diffs_.begin(), chain_diffs_.end(),
              [&chain_order_map](const std::unique_ptr<ChainDiff>& a, const std::unique_ptr<ChainDiff>& b) {
                  auto it_a = chain_order_map.find(a->chainId());
                  auto it_b = chain_order_map.find(b->chainId());
                  
                  // If both chains are in the order map, sort by their order
                  if (it_a != chain_order_map.end() && it_b != chain_order_map.end()) {
                      return it_a->second < it_b->second;
                  }
                  
                  // If only one chain is in the order map, prioritize it
                  if (it_a != chain_order_map.end()) {
                      return true;
                  }
                  if (it_b != chain_order_map.end()) {
                      return false;
                  }
                  
                  // If neither chain is in the order map, sort by chain ID
                  return a->chainId() < b->chainId();
              });
    
    // Merge chain diffs in order
    for (const auto& diff : chain_diffs_) {
        const auto& changes = diff->getAllChanges();
        for (const auto& table_entry : changes) {
            const std::string& table = table_entry.first;
            for (const auto& key_entry : table_entry.second) {
                const std::string& key = key_entry.first;
                const std::string& value = key_entry.second;
                state_changes_[table][key] = value;
            }
        }
    }
    
    // Clear chain diffs after merging
    chain_diffs_.clear();
}

// ChainDiff implementation
void ChainDiff::write(const std::string& table, const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    ensureWritable();
    state_changes_[table][key] = value;
    size_ = 0; // Reset size counter, will be recalculated when needed
    for (const auto& table_entry : state_changes_) {
        size_ += table_entry.second.size();
    }
}

bool ChainDiff::read(const std::string& table, const std::string& key, std::string& value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto table_it = state_changes_.find(table);
    if (table_it != state_changes_.end()) {
        auto key_it = table_it->second.find(key);
        if (key_it != table_it->second.end()) {
            value = key_it->second;
            return true;
        }
    }
    return false;
}

bool ChainDiff::contains(const std::string& table, const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto table_it = state_changes_.find(table);
    if (table_it != state_changes_.end()) {
        return table_it->second.find(key) != table_it->second.end();
    }
    return false;
}

void ChainDiff::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    state_changes_.clear();
    size_ = 0;
}

void ChainDiff::ensureWritable() {
    if (is_copied_) {
        // In a full implementation, we would need to copy the data here
        // For now, we just reset the flag
        is_copied_ = false;
    }
}

} // namespace blp