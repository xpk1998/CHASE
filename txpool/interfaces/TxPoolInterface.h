/**
 * @file TxPoolInterface.h
 * @brief Transaction pool interface for Parachain
 * @author Parachain Team
 * @date 2025
 */
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include "../../build/proto_gen/transaction.pb.h"
#include "txpool/interfaces/TxValidatorInterface.h"

namespace parachain {
namespace txpool {

class TxPoolInterface {
public:
    using Ptr = std::shared_ptr<TxPoolInterface>;
    
    TxPoolInterface() = default;
    virtual ~TxPoolInterface() = default;
    
    /**
     * @brief Submit transaction to pool
     * @param tx Transaction to submit
     * @return TransactionStatus submission result
     */
    virtual TransactionStatus submitTransaction(const TransactionPayload& tx) = 0;
    
    /**
     * @brief Remove transaction from pool
     * @param txHash Transaction hash to remove
     */
    virtual void removeTransaction(const std::string& txHash) = 0;
    
    /**
     * @brief Get transaction by hash
     * @param txHash Transaction hash
     * @return Transaction, nullptr if not found
     */
    virtual std::shared_ptr<TransactionPayload> getTransaction(const std::string& txHash) = 0;
    
    /**
     * @brief Get pending transactions
     * @param maxCount Maximum number of transactions to return
     * @return Vector of pending transactions
     */
    virtual std::vector<TransactionPayload> getPendingTransactions(size_t maxCount) = 0;
    
    /**
     * @brief Get transaction count
     * @return Number of transactions in pool
     */
    virtual size_t getTransactionCount() = 0;
    
    /**
     * @brief Broadcast transaction to network
     * @param tx Transaction to broadcast
     */
    virtual void broadcastTransaction(const TransactionPayload& tx) = 0;
    
    /**
     * @brief Handle received transaction from network
     * @param tx Transaction received from network
     */
    virtual void onReceiveTransaction(const TransactionPayload& tx) = 0;
    
    /**
     * @brief Start transaction pool
     */
    virtual void start() = 0;
    
    /**
     * @brief Stop transaction pool
     */
    virtual void stop() = 0;
    
    /**
     * @brief Set transaction validator
     */
    virtual void setValidator(TxValidatorInterface::Ptr validator) = 0;
};

} // namespace txpool
} // namespace parachain