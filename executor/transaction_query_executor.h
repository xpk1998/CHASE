#ifndef NEUBLOCKCHAIN_TRANSACTIONQUERYEXECUTOR_H
#define NEUBLOCKCHAIN_TRANSACTIONQUERYEXECUTOR_H

#include "transaction/transaction.h"
#include "transaction_executor.h"
#include <vector>

class TransactionQueryExecutor : public TransactionExecutor {
public:
    TransactionQueryExecutor();
    virtual ~TransactionQueryExecutor();
    
    // Inherit pure virtual functions from TransactionExecutor
    bool executeList(const std::vector<Transaction*>& transactionList) override;
    bool commitList(const std::vector<Transaction*>& transactionList) override;

    // Query transactions by various criteria
    std::vector<Transaction*> queryByStatus(TransactionResult status);
    std::vector<Transaction*> queryByEpoch(epoch_size_t epoch);
    std::vector<Transaction*> queryByTimeRange(uint64_t startTime, uint64_t endTime);
    
    // Get transaction statistics
    size_t getTotalTransactionCount();
    size_t getTransactionCountByStatus(TransactionResult status);
    
    // Get specific transaction
    Transaction* getTransactionById(tid_size_t transactionId);
};

#endif //NEUBLOCKCHAIN_TRANSACTIONQUERYEXECUTOR_H