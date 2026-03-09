/**
 * @file TxPoolStorageInterface.h
 * @brief Transaction pool storage interface for Parachain
 * @author Parachain Team
 * @date 2025
 */
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <set>
#include "transaction.pb.h"
#include "txpool/interfaces/TxValidatorInterface.h"

namespace parachain {
namespace txpool {

class TxPoolStorageInterface {
public:
    using Ptr = std::shared_ptr<TxPoolStorageInterface>;
    
    TxPoolStorageInterface() = default;
    virtual ~TxPoolStorageInterface() = default;
    
    /**
     * @brief Insert transaction to storage
     * @param tx Transaction to insert
     * @return TransactionStatus insertion result
     */
    virtual TransactionStatus insert(const TransactionPayload& tx) = 0;
    
    /**
     * @brief Remove transaction from storage
     * @param txHash Transaction hash to remove
     * @return Removed transaction, nullptr if not found
     */
    virtual std::shared_ptr<TransactionPayload> remove(const std::string& txHash) = 0;
    
    /**
     * @brief Get transaction by hash
     * @param txHash Transaction hash
     * @return Transaction, nullptr if not found
     */
    virtual std::shared_ptr<TransactionPayload> get(const std::string& txHash) = 0;
    
    /**
     * @brief Get pending transactions
     * @param maxCount Maximum number of transactions to return
     * @return Vector of pending transactions
     */
    virtual std::vector<std::shared_ptr<TransactionPayload>> getPendingTransactions(size_t maxCount) = 0;
    
    /**
     * @brief Check if transaction exists
     * @param txHash Transaction hash
     * @return true if transaction exists, false otherwise
     */
    virtual bool exist(const std::string& txHash) = 0;
    
    /**
     * @brief Get transaction count
     * @return Number of transactions in storage
     */
    virtual size_t size() const = 0;
    
    /**
     * @brief Clear all transactions
     */
    virtual void clear() = 0;
    
    /**
     * @brief Batch remove transactions
     * @param txHashes Transaction hashes to remove
     */
    virtual void batchRemove(const std::set<std::string>& txHashes) = 0;
    
    /**
     * @brief Start storage
     */
    virtual void start() = 0;
    
    /**
     * @brief Stop storage
     */
    virtual void stop() = 0;
};

} // namespace txpool
} // namespace parachain