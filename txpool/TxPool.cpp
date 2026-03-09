/**
 * @file TxPool.cpp
 * @brief Transaction pool implementation for Parachain
 * @author Parachain Team
 * @date 2025
 */
#include "TxPool.h"
#include "txpool/validator/TxValidator.h"
#include "txpool/validator/TxPoolNonceChecker.h"
#include "../../utilities/log/logger.h"
#include <chrono>

namespace parachain {
namespace txpool {

TxPool::TxPool(TxPoolConfig::Ptr config)
    : m_config(std::move(config)) {
    
    // Create validator with nonce checker
    auto nonceChecker = std::make_shared<TxPoolNonceChecker>();
    m_validator = std::make_shared<TxValidator>(nonceChecker);
    
    // Create storage with validator
    m_storage = std::make_shared<MemoryStorage>(m_validator);
    
    // Create sync with storage
    m_sync = std::make_shared<sync::TransactionSync>(m_storage);
}

TxPool::~TxPool() {
    stop();
}

TransactionStatus TxPool::submitTransaction(const TransactionPayload& tx) {
    if (!m_running) {
        return TransactionStatus::Unknown;
    }
    
    // Validate transaction
    if (m_validator) {
        auto status = m_validator->verify(tx);
        if (status != TransactionStatus::None) {
            return status;
        }
    }
    
    // Insert transaction to storage
    if (m_storage) {
        auto status = m_storage->insert(tx);
        if (status != TransactionStatus::None) {
            return status;
        }
    }
    
    // Broadcast transaction to network
    if (m_sync) {
        m_sync->broadcastTransaction(tx);
    }
    
    return TransactionStatus::None;
}

void TxPool::removeTransaction(const std::string& txHash) {
    if (m_storage) {
        m_storage->remove(txHash);
    }
}

std::shared_ptr<TransactionPayload> TxPool::getTransaction(const std::string& txHash) {
    if (m_storage) {
        return m_storage->get(txHash);
    }
    return nullptr;
}

std::vector<TransactionPayload> TxPool::getPendingTransactions(size_t maxCount) {
    if (m_storage) {
        auto txs = m_storage->getPendingTransactions(maxCount);
        std::vector<TransactionPayload> result;
        result.reserve(txs.size());
        for (const auto& tx : txs) {
            result.push_back(*tx);
        }
        return result;
    }
    return {};
}

size_t TxPool::getTransactionCount() {
    if (m_storage) {
        return m_storage->size();
    }
    return 0;
}

void TxPool::broadcastTransaction(const TransactionPayload& tx) {
    if (m_sync) {
        m_sync->broadcastTransaction(tx);
    }
}

void TxPool::onReceiveTransaction(const TransactionPayload& tx) {
    if (m_sync) {
        m_sync->onReceiveTransaction(tx);
    }
}

void TxPool::start() {
    if (m_running.exchange(true)) {
        return; // Already started
    }
    
    LOG(INFO) << "Starting transaction pool";
    
    // Start storage
    if (m_storage) {
        m_storage->start();
    }
    
    // Start sync
    if (m_sync) {
        m_sync->start();
    }
    
    // Start cleanup thread
    m_cleanupThread = std::make_unique<std::thread>(&TxPool::cleanupThread, this);
    
    LOG(INFO) << "Transaction pool started successfully";
}

void TxPool::stop() {
    if (!m_running.exchange(false)) {
        return; // Already stopped
    }
    
    LOG(INFO) << "Stopping transaction pool";
    
    // Stop sync
    if (m_sync) {
        m_sync->stop();
    }
    
    // Stop storage
    if (m_storage) {
        m_storage->stop();
    }
    
    // Join cleanup thread
    if (m_cleanupThread && m_cleanupThread->joinable()) {
        m_cleanupThread->join();
    }
    
    LOG(INFO) << "Transaction pool stopped successfully";
}

void TxPool::setValidator(TxValidatorInterface::Ptr validator) {
    m_validator = std::move(validator);
    
    // Update validator in storage
    if (auto memoryStorage = std::dynamic_pointer_cast<MemoryStorage>(m_storage)) {
        // Note: In a real implementation, we would need a setter for the validator in MemoryStorage
        // For now, we'll just log that the validator has been set
        LOG(INFO) << "Validator set for transaction pool";
    }
}

void TxPool::cleanupThread() {
    pthread_setname_np(pthread_self(), "txpool_cleanup");
    
    while (m_running) {
        try {
            // In a real implementation, we would clean up expired transactions here
            // For now, we'll just sleep
            
            // Sleep for cleanup interval
            std::this_thread::sleep_for(std::chrono::seconds(30));
        } catch (const std::exception& e) {
            LOG(ERROR) << "Error in transaction pool cleanup thread: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace txpool
} // namespace parachain