#ifndef NEUBLOCKCHAIN_TRANSACTIONMANAGERIMPL_H
#define NEUBLOCKCHAIN_TRANSACTIONMANAGERIMPL_H

#include "transaction/transaction_manager.h"
#include <unordered_map>
#include <vector>
#include <mutex>

class TransactionManagerImpl : public TransactionManager {
public:
    TransactionManagerImpl();
    virtual ~TransactionManagerImpl();

    // Implement the pure virtual functions from TransactionManager
    [[nodiscard]] virtual size_t addTransaction(std::unique_ptr<std::vector<Transaction*>> trWrapper) override;
    virtual void setFinishSignal() override;

private:
    // Internal data structure to store transactions
    std::unordered_map<tid_size_t, Transaction*> transactions_;
    
    // Mutex for thread safety
    mutable std::mutex transactions_mutex_;
};

#endif //NEUBLOCKCHAIN_TRANSACTIONMANAGERIMPL_H