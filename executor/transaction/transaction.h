//
// Created by peng on 2021/1/17.
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_H
#define NEUBLOCKCHAIN_TRANSACTION_H

#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include "../../utilities/types/aria_types.h"
#include "../../build/proto_gen/kv_rwset.pb.h"

class TransactionPayload;

enum class TransactionResult { 
    COMMIT,               // 交易成功提交
    PENDING,              // 交易待处理
    ABORT,                // 交易中止，可重试
    ABORT_NO_RETRY,       // 交易中止，不可重试
    ABORT_GAS_EXHAUSTED,  // Gas耗尽导致的中止
    ABORT_EXECUTION_ERROR,// 执行错误导致的中止
    ABORT_VALIDATION_FAIL // 验证失败导致的中止
};

class Transaction {
public:
    virtual ~Transaction() = default;
    // notice: tid start from 1, not equal 0
    virtual tid_size_t getTransactionID() = 0;
    virtual void setTransactionID(tid_size_t tid) = 0;
    // search op, and execute op must be done with worker or db!
    // transaction object can not hold a db reference!

    virtual void resetRWSet() = 0;
    virtual KVRWSet* getRWSet() = 0;

    virtual void resetTransaction() = 0;    // if a transaction abort, it must be reset.
    virtual bool readOnly() = 0;
    virtual void readOnly(bool flag) = 0;

    virtual void setEpoch(epoch_size_t epoch) = 0;
    [[nodiscard]] virtual epoch_size_t getEpoch() const = 0;

    virtual TransactionResult getTransactionResult() = 0;
    virtual void setTransactionResult(TransactionResult transactionResult) = 0;

    virtual TransactionPayload* getTransactionPayload() = 0;
    virtual void resetTransactionPayload() = 0;

    virtual bool serializeToString(std::string* transactionRaw) = 0;
    virtual bool deserializeFromString(const std::string& transactionRaw) = 0;

    virtual bool serializeResultToString(std::string* transactionRaw) = 0;
    virtual bool deserializeResultFromString(const std::string& transactionRaw) = 0;

    // Error message handling
    virtual std::string getErrorMessage() const = 0;
    virtual void setErrorMessage(const std::string& errorMessage) = 0;

    void setInvalidBit(std::pair<epoch_size_t, std::atomic<bool>>* flag) {
        invalidBit = flag;
    }

    [[nodiscard]] bool isValid() const {
        if (invalidBit && !invalidBit->second.load(std::memory_order_acquire)) {
            if (invalidBit->first <= getEpoch()) {
                return false;
            }
        }
        return true;
    }

private:
    std::pair<epoch_size_t, std::atomic<bool>>* invalidBit = nullptr;

};

#endif //NEUBLOCKCHAIN_TRANSACTION_H