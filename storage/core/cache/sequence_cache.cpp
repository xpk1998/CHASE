//
// SequenceCache Implementation for Enhanced Two-Layer Cache System
//

#include "sequence_cache.h"
#include <shared_mutex>

namespace blp {

SequenceCache::SequenceCache(uint32_t sequence_id) : sequence_id_(sequence_id) {}

void SequenceCache::put(const std::string& table,
                       const std::string& key,
                       const std::string& value) {
    std::unique_lock<std::mutex> lock(mutex_);
    state_changes_[table][key] = value;
}

void SequenceCache::put(uint64_t chain_id,
                       const std::string& table,
                       const std::string& key,
                       const std::string& value) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (chain_id == 0) {
        // Treat as non-chain-specific state change
        state_changes_[table][key] = value;
    } else {
        // Store as chain-level state change
        chain_state_changes_[chain_id][table][key] = value;
    }
}

bool SequenceCache::get(const std::string& table,
                       const std::string& key,
                       std::string& value) const {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // First check chain-level changes
    for (const auto& chain_entry : chain_state_changes_) {
        const auto& chain_changes = chain_entry.second;
        auto table_it = chain_changes.find(table);
        if (table_it != chain_changes.end()) {
            auto key_it = table_it->second.find(key);
            if (key_it != table_it->second.end()) {
                value = key_it->second;
                return true;
            }
        }
    }
    
    // Then check non-chain-specific changes
    auto table_it = state_changes_.find(table);
    if (table_it == state_changes_.end()) {
        return false;
    }
    
    auto key_it = table_it->second.find(key);
    if (key_it == table_it->second.end()) {
        return false;
    }
    
    value = key_it->second;
    return true;
}

const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& 
SequenceCache::getAllChanges() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return state_changes_;
}

const std::unordered_map<uint64_t, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>&
SequenceCache::getAllChainChanges() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return chain_state_changes_;
}

void SequenceCache::clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    state_changes_.clear();
    chain_state_changes_.clear();
}

size_t SequenceCache::size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    
    size_t total = 0;
    
    // Count non-chain-specific changes
    for (const auto& table_entry : state_changes_) {
        total += table_entry.second.size();
    }
    
    // Count chain-level changes
    for (const auto& chain_entry : chain_state_changes_) {
        const auto& chain_changes = chain_entry.second;
        for (const auto& table_entry : chain_changes) {
            total += table_entry.second.size();
        }
    }
    
    return total;
}

} // namespace blp