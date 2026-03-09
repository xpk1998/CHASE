//
// KeyPageAggregator Implementation for Enhanced Two-Layer Cache System
//

#include "key_page_aggregator.h"
#include <glog/logging.h>

namespace blp {

// 默认页面大小为100个条目
const size_t DEFAULT_PAGE_SIZE = 100;

KeyPageAggregator::KeyPageAggregator(size_t max_page_size, bool enable_copy_on_write)
    : max_page_size_(max_page_size), 
      enable_copy_on_write_(enable_copy_on_write),
      page_version_(0),
      page_id_counter_(0) {}

void KeyPageAggregator::addUpdate(const std::string& table, const std::string& key, const std::string& value) {
    // 添加更新到适当的KeyPage
    // 如果当前页面已满，则创建新页面
    auto& pages = pages_[table];
    
    // 如果没有页面或最后一个页面已满，创建新页面
    if (pages.empty() || pages.back().size() >= max_page_size_) {
        KeyPage new_page;
        new_page.table = table;
        new_page.page_id = ++page_id_counter_;
        new_page.version = page_version_;
        pages.push_back(new_page);
    }
    
    // 添加条目到最后一个页面
    pages.back().addEntry(key, value);
}

void KeyPageAggregator::addSequenceUpdates(
    [[maybe_unused]] uint32_t sequence_id,
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& updates) {
    
    for (const auto& table_entry : updates) {
        const std::string& table = table_entry.first;
        for (const auto& key_entry : table_entry.second) {
            const std::string& key = key_entry.first;
            const std::string& value = key_entry.second;
            
            // 添加更新到适当的KeyPage
            addUpdate(table, key, value);
        }
    }
}

void KeyPageAggregator::addChainUpdates(
    uint64_t chain_id,
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& updates) {
    
    // Add updates to chain-specific pages
    auto& chain_table_pages = chain_pages_[chain_id];
    
    for (const auto& table_entry : updates) {
        const std::string& table = table_entry.first;
        auto& pages = chain_table_pages[table];
        
        for (const auto& key_entry : table_entry.second) {
            const std::string& key = key_entry.first;
            const std::string& value = key_entry.second;
            
            // 如果没有页面或最后一个页面已满，创建新页面
            if (pages.empty() || pages.back().size() >= max_page_size_) {
                KeyPage new_page;
                new_page.table = table;
                new_page.page_id = ++page_id_counter_;
                new_page.version = page_version_;
                pages.push_back(new_page);
            }
            
            // 添加条目到最后一个页面
            pages.back().addEntry(key, value);
        }
    }
}

std::vector<KeyPage> KeyPageAggregator::getKeyPages() const {
    std::vector<KeyPage> result;
    
    // Collect regular pages
    for (const auto& table_entry : pages_) {
        for (const auto& page : table_entry.second) {
            if (!page.entries.empty()) {  // 只返回非空页面
                result.push_back(page);
            }
        }
    }
    
    // Collect chain-specific pages
    for (const auto& chain_entry : chain_pages_) {
        for (const auto& table_entry : chain_entry.second) {
            for (const auto& page : table_entry.second) {
                if (!page.entries.empty()) {  // 只返回非空页面
                    result.push_back(page);
                }
            }
        }
    }
    
    return result;
}

void KeyPageAggregator::clear() {
    // Clear regular pages
    for (auto& table_entry : pages_) {
        for (auto& page : table_entry.second) {
            page.clear();
        }
        table_entry.second.clear();
    }
    pages_.clear();
    
    // Clear chain-specific pages
    for (auto& chain_entry : chain_pages_) {
        for (auto& table_entry : chain_entry.second) {
            for (auto& page : table_entry.second) {
                page.clear();
            }
            table_entry.second.clear();
        }
        chain_entry.second.clear();
    }
    chain_pages_.clear();
}

size_t KeyPageAggregator::totalEntries() const {
    size_t total = 0;
    
    // Count regular pages
    for (const auto& table_entry : pages_) {
        for (const auto& page : table_entry.second) {
            total += page.size();
        }
    }
    
    // Count chain-specific pages
    for (const auto& chain_entry : chain_pages_) {
        for (const auto& table_entry : chain_entry.second) {
            for (const auto& page : table_entry.second) {
                total += page.size();
            }
        }
    }
    
    return total;
}

} // namespace blp