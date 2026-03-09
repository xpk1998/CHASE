//
// Created by peng on 2021/1/14.
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_MANAGER_H
#define NEUBLOCKCHAIN_TRANSACTION_MANAGER_H

#include <vector>
#include <memory>
#include "../../utilities/types/aria_types.h"

class Transaction;

class TransactionManager {
public:
    virtual ~TransactionManager() = default;

    [[nodiscard]] virtual size_t addTransaction(std::unique_ptr<std::vector<Transaction*>> trWrapper) = 0;
    virtual void setFinishSignal() = 0;
    
    // Additional virtual functions that TransactionManagerImpl tries to implement
    virtual bool submitTransaction(std::unique_ptr<Transaction> transaction) = 0;
    virtual std::vector<std::unique_ptr<Transaction>> getPendingTransactions(size_t maxCount) = 0;
    virtual size_t getPendingTransactionCount() const = 0;
    virtual bool removeTransaction(uint64_t transactionId) = 0;
};

#endif //NEUBLOCKCHAIN_TRANSACTION_MANAGER_H