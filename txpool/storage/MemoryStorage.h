/**
 * @file MemoryStorage.h
 * @brief Memory storage implementation for Parachain txpool
 * @author Parachain Team
 * @date 2025
 */
#pragma once

#include "txpool/interfaces/TxPoolStorageInterface.h"
#include "txpool/interfaces/TxValidatorInterface.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace parachain {
namespace txpool {

class MemoryStorage : public TxPoolStorageInterface {
public:
    explicit MemoryStorage(TxValidatorInterface::Ptr validator);
    virtual ~MemoryStorage() = default;
    
    /**
     * @brief Insert transaction to storage
     * @param tx Transaction to insert
     * @return TransactionStatus insertion result
     */
    TransactionStatus insert(const TransactionPayload& tx) override;
    
    /**
     * @brief Remove transaction from storage
     * @param txHash Transaction hash to remove
     * @return Removed transaction, nullptr if not found
     */
    std::shared_ptr<TransactionPayload> remove(const std::string& txHash) override;
    
    /**
     * @brief Get transaction by hash
     * @param txHash Transaction hash
     * @return Transaction, nullptr if not found
     */
    std::shared_ptr<TransactionPayload> get(const std::string& txHash) override;
    
    /**
     * @brief Get pending transactions
     * @param maxCount Maximum number of transactions to return
     * @return Vector of pending transactions
     */
    std::vector<std::shared_ptr<TransactionPayload>> getPendingTransactions(size_t maxCount) override;
    
    /**
     * @brief Check if transaction exists
     * @param txHash Transaction hash
     * @return true if transaction exists, false otherwise
     */
    bool exist(const std::string& txHash) override;
    
    /**
     * @brief Get transaction count
     * @return Number of transactions in storage
     */
    size_t size() const override;
    
    /**
     * @brief Clear all transactions
     */
    void clear() override;
    
    /**
     * @brief Batch remove transactions
     * @param txHashes Transaction hashes to remove
     */
    void batchRemove(const std::set<std::string>& txHashes) override;
    
    /**
     * @brief Start storage
     */
    void start() override;
    
    /**
     * @brief Stop storage
     */
    void stop() override;

private:
    std::unordered_map<std::string, std::shared_ptr<TransactionPayload>> m_transactions;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    TxValidatorInterface::Ptr m_validator;
};

} // namespace txpool
} // namespace parachain