//
// Created by ParaChain Team on 2025/11/26.
//

#ifndef PARACHAIN_LEDGER_MANAGER_H
#define PARACHAIN_LEDGER_MANAGER_H

#include <string>
#include <memory>
#include <map>
#include <cstdint>

class Block;

namespace core {
namespace ledger {

class LedgerManager {
public:
    static LedgerManager* getInstance();
    
    // 账本初始化
    bool initialize();
    
    // 添加区块到账本
    bool addBlock(std::shared_ptr<Block> block);
    
    // 获取最新区块
    std::shared_ptr<Block> getLatestBlock();
    
    // 根据区块高度获取区块
    std::shared_ptr<Block> getBlockByHeight(uint64_t height);
    
    // 获取账本高度
    uint64_t getLedgerHeight();
    
    // 验证区块
    bool validateBlock(std::shared_ptr<Block> block);
    
private:
    LedgerManager() = default;
    ~LedgerManager() = default;
    
    static std::unique_ptr<LedgerManager> instance_;
    
    // 账本数据结构
    std::map<uint64_t, std::shared_ptr<Block>> blocks_;  // 按高度索引的区块存储
    uint64_t latest_block_height_ = 0;  // 最新区块高度
};

} // namespace ledger
} // namespace core

#endif //PARACHAIN_LEDGER_MANAGER_H