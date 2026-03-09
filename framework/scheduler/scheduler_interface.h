#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <string>
// Removed references to the protocol classes as they should be defined elsewhere in ParaChain

namespace parachain {

// Forward declarations - these should be defined in appropriate protocol headers
class Block;
class Transaction;
class LedgerConfig;
class Error;
class BlockHeader;
class TransactionReceipt;

// 调度器接口定义
class SchedulerInterface {
public:
    virtual ~SchedulerInterface() = default;

    // 执行区块 - 由共识模块调用
    virtual void executeBlock(std::shared_ptr<Block> block, bool verify,
        std::function<void(std::shared_ptr<Error>, std::shared_ptr<BlockHeader>, bool _sysBlock)> callback) = 0;

    // 提交区块 - 由共识模块调用
    virtual void commitBlock(std::shared_ptr<BlockHeader> header,
        std::function<void(std::shared_ptr<Error>, std::shared_ptr<LedgerConfig>)> callback) = 0;

    // 预执行区块 - 用于性能优化
    virtual void preExecuteBlock(std::shared_ptr<Block> block, bool verify,
        std::function<void(std::shared_ptr<Error>)> callback) = 0;

    // 调用交易 - 用于查询
    virtual void call(std::shared_ptr<Transaction> tx,
        std::function<void(std::shared_ptr<Error>, std::shared_ptr<TransactionReceipt>)> callback) = 0;

    // 获取合约代码
    virtual void getCode(const std::string& contract,
        std::function<void(std::shared_ptr<Error>, std::vector<uint8_t>)> callback) = 0;

    // 获取ABI
    virtual void getABI(const std::string& contract,
        std::function<void(std::shared_ptr<Error>, std::string)> callback) = 0;

    // 启动调度器
    virtual void start() = 0;

    // 停止调度器
    virtual void stop() = 0;

    // 重置调度器状态
    virtual void reset(std::function<void(std::shared_ptr<Error>)> callback) = 0;
};

} // namespace parachain