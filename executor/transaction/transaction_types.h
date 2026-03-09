//
// Created for EVM Integration
// Transaction types and extensions for smart contract support
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_TYPES_H
#define NEUBLOCKCHAIN_TRANSACTION_TYPES_H

#include <string>
#include <vector>
#include <array>
#include "../../runtime/vm/types.h"

// Transaction type enumeration
enum class TransactionType {
    CHAINCODE,      // 原有的 Chaincode 交易
    EVM_CONTRACT,   // 新的 EVM 智能合约交易
};

// EVM Transaction extension data
// This structure holds EVM-specific transaction data
struct EVMTransactionData {
    TransactionType tx_type = TransactionType::CHAINCODE;
    
    // EVM 合约相关字段
    neuchain::evm::Address sender;            // 发送方地址
    neuchain::evm::Address contract_address;  // 合约地址（20 字节）
    neuchain::evm::bytes contract_input;      // 合约输入数据
    int64_t gas_limit = 0;                    // Gas 限制
    int64_t gas_used = 0;                     // 实际消耗的 Gas
    int64_t gas_price = 0;                    // Gas 价格
    uint64_t value = 0;                       // 转账金额
    
    // 执行结果
    neuchain::evm::bytes output;              // 执行输出
    std::vector<neuchain::evm::LogEntry> logs; // 事件日志
    int32_t status_code = 0;                  // EVMC 状态码
    std::string error_message;                // 错误消息
    
    // 判断是否为智能合约交易
    bool isSmartContract() const {
        return tx_type == TransactionType::EVM_CONTRACT;
    }
    
    // 判断是否为合约创建交易
    bool isContractCreation() const {
        if (!isSmartContract()) {
            return false;
        }
        // 如果合约地址为空（全零），则为创建交易
        for (uint8_t byte : contract_address) {
            if (byte != 0) {
                return false;
            }
        }
        return true;
    }
    
    // 获取发送方地址
    const neuchain::evm::Address& getSender() const {
        return sender;
    }
    
    // 设置发送方地址
    void setSender(const neuchain::evm::Address& addr) {
        sender = addr;
    }
    
    // 获取合约地址
    const neuchain::evm::Address& getContractAddress() const {
        return contract_address;
    }
    
    // 设置合约地址
    void setContractAddress(const neuchain::evm::Address& addr) {
        contract_address = addr;
    }
    
    // 获取合约输入数据
    const neuchain::evm::bytes& getContractInput() const {
        return contract_input;
    }
    
    // 设置合约输入数据
    void setContractInput(const neuchain::evm::bytes& input) {
        contract_input = input;
    }
    
    // 设置执行结果
    void setExecutionResult(const neuchain::evm::ExecutionResult& result) {
        gas_used = result.gas_left > 0 ? (gas_limit - result.gas_left) : gas_limit;
        output = result.output;
        logs = result.logs;
        status_code = result.status_code;
        
        // 如果是合约创建，保存创建的合约地址
        if (isContractCreation() && result.isSuccess()) {
            contract_address = result.create_address;
        }
    }
    
    // 获取 Gas 限制
    int64_t getGasLimit() const {
        return gas_limit;
    }
    
    // 设置 Gas 限制
    void setGasLimit(int64_t limit) {
        gas_limit = limit;
    }
    
    // 获取 Gas 价格
    int64_t getGasPrice() const {
        return gas_price;
    }
    
    // 设置 Gas 价格
    void setGasPrice(int64_t price) {
        gas_price = price;
    }
    
    // 获取转账金额
    uint64_t getValue() const {
        return value;
    }
    
    // 设置转账金额
    void setValue(uint64_t val) {
        value = val;
    }
    
    // 获取执行输出
    const neuchain::evm::bytes& getOutput() const {
        return output;
    }
    
    // 获取事件日志
    const std::vector<neuchain::evm::LogEntry>& getLogs() const {
        return logs;
    }
    
    // 获取状态码
    int32_t getStatusCode() const {
        return status_code;
    }
    
    // 获取错误消息
    const std::string& getErrorMessage() const {
        return error_message;
    }
    
    // 设置错误消息
    void setErrorMessage(const std::string& msg) {
        error_message = msg;
    }
    
    // 清空数据
    void clear() {
        tx_type = TransactionType::CHAINCODE;
        contract_address = {};
        contract_input.clear();
        gas_limit = 0;
        gas_used = 0;
        gas_price = 0;
        value = 0;
        output.clear();
        logs.clear();
        status_code = 0;
        error_message.clear();
    }
};

#endif //NEUBLOCKCHAIN_TRANSACTION_TYPES_H

