/**
 * @file TxPool.h
 * @brief Transaction pool implementation for Parachain
 * @author Parachain Team
 * @date 2025
 */
#pragma once

#include "txpool/interfaces/TxPoolInterface.h"
#include "txpool/storage/MemoryStorage.h"
#include "txpool/sync/TransactionSync.h"
#include "txpool/TxPoolConfig.h"
#include <atomic>
#include <thread>

namespace parachain {
namespace txpool {

class TxPool : public TxPoolInterface {
public:
    using Ptr = std::shared_ptr<TxPool>;
    
    explicit TxPool(TxPoolConfig::Ptr config);
    virtual ~TxPool();
    
    /**
     * @brief Submit transaction to pool
     * @param tx Transaction to submit
     * @return TransactionStatus submission result
     */
    TransactionStatus submitTransaction(const TransactionPayload& tx) override;
    
    /**
     * @brief Remove transaction from pool
     * @param txHash Transaction hash to remove
     */
    void removeTransaction(const std::string& txHash) override;
    
    /**
     * @brief Get transaction by hash
     * @param txHash Transaction hash
     * @return Transaction, nullptr if not found
     */
    std::shared_ptr<TransactionPayload> getTransaction(const std::string& txHash) override;
    
    /**
     * @brief Get pending transactions
     * @param maxCount Maximum number of transactions to return
     * @return Vector of pending transactions
     */
    std::vector<TransactionPayload> getPendingTransactions(size_t maxCount) override;
    
    /**
     * @brief Get transaction count
     * @return Number of transactions in pool
     */
    size_t getTransactionCount() override;
    
    /**
     * @brief Broadcast transaction to network
     * @param tx Transaction to broadcast
     */
    void broadcastTransaction(const TransactionPayload& tx) override;
    
    /**
     * @brief Handle received transaction from network
     * @param tx Transaction received from network
     */
    void onReceiveTransaction(const TransactionPayload& tx) override;
    
    /**
     * @brief Start transaction pool
     */
    void start() override;
    
    /**
     * @brief Stop transaction pool
     */
    void stop() override;
    
    /**
     * @brief Set transaction validator
     */
    void setValidator(TxValidatorInterface::Ptr validator) override;
    
    /**
     * @brief Get storage
     */
    TxPoolStorageInterface::Ptr storage() { return m_storage; }
    
    /**
     * @brief Get sync
     */
    sync::TransactionSyncInterface::Ptr sync() { return m_sync; }

private:
    TxPoolConfig::Ptr m_config;
    TxPoolStorageInterface::Ptr m_storage;
    sync::TransactionSyncInterface::Ptr m_sync;
    TxValidatorInterface::Ptr m_validator;
    std::atomic<bool> m_running{false};
    
    // Cleanup thread for expired transactions
    std::unique_ptr<std::thread> m_cleanupThread;
    void cleanupThread();
};

} // namespace txpool
} // namespace parachain