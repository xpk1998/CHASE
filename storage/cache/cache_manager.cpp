/**
 * @file cache_manager.cpp
 * @brief Cache manager implementation for Parachain
 * @author Parachain Team
 * @date 2025
 */

#include "storage/cache/cache_manager.h"
#include "../../utilities/log/logger.h"
#include <chrono>
#include <mutex>

namespace storage {
namespace cache {

// Static instance pointer
template<typename Key, typename Value>
std::unique_ptr<CacheManager<Key, Value>> CacheManager<Key, Value>::instance_ = nullptr;

template<typename Key, typename Value>
CacheManager<Key, Value>* CacheManager<Key, Value>::getInstance() {
    if (!instance_) {
        instance_ = std::make_unique<CacheManager<Key, Value>>();
    }
    return instance_.get();
}

template<typename Key, typename Value>
void CacheManager<Key, Value>::put(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[key] = value;
}

template<typename Key, typename Value>
Value CacheManager<Key, Value>::get(const Key& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }
    return Value{}; // Return default constructed value
}

template<typename Key, typename Value>
bool CacheManager<Key, Value>::contains(const Key& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
}

template<typename Key, typename Value>
void CacheManager<Key, Value>::remove(const Key& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(key);
}

template<typename Key, typename Value>
void CacheManager<Key, Value>::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

template<typename Key, typename Value>
size_t CacheManager<Key, Value>::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

// Explicit template instantiation for common types
template class CacheManager<std::string, std::string>;
template class CacheManager<std::string, int>;
template class CacheManager<std::string, std::vector<uint8_t>>;

} // namespace cache
} // namespace storage