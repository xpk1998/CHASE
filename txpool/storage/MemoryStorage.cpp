/**
 * @file MemoryStorage.cpp
 * @brief Memory storage implementation for Parachain txpool
 * @author Parachain Team
 * @date 2025
 */
#include "MemoryStorage.h"
#include "../../crypto/util/sha256_helper.h"
#include "../../../utilities/log/logger.h"

namespace parachain {
namespace txpool {

MemoryStorage::MemoryStorage(TxValidatorInterface::Ptr validator)
    : m_validator(std::move(validator)) {}

TransactionStatus MemoryStorage::insert(const TransactionPayload& tx) {
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
    
    // Generate transaction hash
    std::string txData = tx.SerializeAsString();
    SHA256Helper sha256Helper;
    std::string txHash;
    sha256Helper.hash(txData, &txHash);
    
    // Check if transaction already exists
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_transactions.find(txHash) != m_transactions.end()) {
            return TransactionStatus::AlreadyInTxPool;
        }
    }
    
    // Add transaction to storage
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_transactions[txHash] = std::make_shared<TransactionPayload>(tx);
    }
    
    LOG(INFO) << "Inserted transaction to storage, hash: " << txHash;
    
    return TransactionStatus::None;
}

std::shared_ptr<TransactionPayload> MemoryStorage::remove(const std::string& txHash) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_transactions.find(txHash);
    if (it != m_transactions.end()) {
        auto tx = it->second;
        m_transactions.erase(it);
        LOG(INFO) << "Removed transaction from storage, hash: " << txHash;
        return tx;
    }
    
    return nullptr;
}

std::shared_ptr<TransactionPayload> MemoryStorage::get(const std::string& txHash) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_transactions.find(txHash);
    if (it != m_transactions.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<TransactionPayload>> MemoryStorage::getPendingTransactions(size_t maxCount) {
    std::vector<std::shared_ptr<TransactionPayload>> result;
    result.reserve(std::min(maxCount, m_transactions.size()));
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = 0;
    for (const auto& pair : m_transactions) {
        if (count >= maxCount) {
            break;
        }
        
        result.push_back(pair.second);
        count++;
    }
    
    return result;
}

bool MemoryStorage::exist(const std::string& txHash) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transactions.find(txHash) != m_transactions.end();
}

size_t MemoryStorage::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transactions.size();
}

void MemoryStorage::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_transactions.clear();
    LOG(INFO) << "Cleared all transactions from storage";
}

void MemoryStorage::batchRemove(const std::set<std::string>& txHashes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& txHash : txHashes) {
        m_transactions.erase(txHash);
    }
    
    LOG(INFO) << "Batch removed " << txHashes.size() << " transactions from storage";
}

void MemoryStorage::start() {
    m_running = true;
    LOG(INFO) << "Memory storage started";
}

void MemoryStorage::stop() {
    m_running = false;
    LOG(INFO) << "Memory storage stopped";
}

} // namespace txpool
} // namespace parachain