#include "transaction_manager_impl.h"
#include "../transaction/transaction.h"
#include "glog/logging.h"

TransactionManagerImpl::TransactionManagerImpl() {
    LOG(INFO) << "TransactionManagerImpl created";
}

TransactionManagerImpl::~TransactionManagerImpl() {
    LOG(INFO) << "TransactionManagerImpl destroyed";
}

size_t TransactionManagerImpl::addTransaction(std::unique_ptr<std::vector<Transaction*>> trWrapper) {
    if (!trWrapper) {
        LOG(ERROR) << "TransactionManagerImpl: Cannot add null transaction wrapper";
        return 0;
    }
    
    size_t addedCount = 0;
    
    // Lock for thread safety
    std::lock_guard<std::mutex> lock(transactions_mutex_);
    
    for (auto* transaction : *trWrapper) {
        if (!transaction) {
            LOG(WARNING) << "TransactionManagerImpl: Null transaction in wrapper, skipping";
            continue;
        }
        
        uint64_t txId = transaction->getTransactionID();
        transactions_[txId] = transaction;
        addedCount++;
        
        LOG(INFO) << "TransactionManagerImpl: Added transaction " << txId;
    }
    
    LOG(INFO) << "TransactionManagerImpl: Added " << addedCount << " transactions";
    
    return addedCount;
}

void TransactionManagerImpl::setFinishSignal() {
    LOG(INFO) << "TransactionManagerImpl: Finish signal received";
    // In a real implementation, this would signal the end of transaction submission
    // For now, we'll just log the event
}