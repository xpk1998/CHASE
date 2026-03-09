//
// Created by ParaChain Team on 2025/11/26.
//

#ifndef PARACHAIN_CACHE_MANAGER_H
#define PARACHAIN_CACHE_MANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace storage {
namespace cache {

template<typename Key, typename Value>
class CacheManager {
public:
    static CacheManager* getInstance();
    
    // 添加键值对到缓存
    void put(const Key& key, const Value& value);
    
    // 从缓存获取值
    Value get(const Key& key);
    
    // 检查键是否在缓存中
    bool contains(const Key& key);
    
    // 从缓存移除键值对
    void remove(const Key& key);
    
    // 清空缓存
    void clear();
    
    // 获取缓存大小
    size_t size();
    
public:
    CacheManager() = default;
    ~CacheManager() = default;
    
    static std::unique_ptr<CacheManager> instance_;
    std::unordered_map<Key, Value> cache_;
    mutable std::mutex mutex_;
};

} // namespace cache
} // namespace storage

#endif //PARACHAIN_CACHE_MANAGER_H