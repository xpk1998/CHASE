/**
 * @file TxPoolConfig.h
 * @brief Transaction pool configuration for Parachain
 * @author Parachain Team
 * @date 2025
 */
#pragma once

#include <string>
#include <memory>

namespace parachain {
namespace txpool {

class TxPoolConfig {
public:
    using Ptr = std::shared_ptr<TxPoolConfig>;
    
    TxPoolConfig() = default;
    virtual ~TxPoolConfig() = default;
    
    /**
     * @brief Set pool limit
     * @param limit Maximum number of transactions in pool
     */
    void setPoolLimit(size_t limit) { m_poolLimit = limit; }
    
    /**
     * @brief Get pool limit
     * @return Maximum number of transactions in pool
     */
    size_t poolLimit() const { return m_poolLimit; }
    
    /**
     * @brief Set transaction expiration time
     * @param expirationTimeMs Expiration time in milliseconds
     */
    void setTxsExpirationTime(uint64_t expirationTimeMs) { m_txsExpirationTime = expirationTimeMs; }
    
    /**
     * @brief Get transaction expiration time
     * @return Expiration time in milliseconds
     */
    uint64_t txsExpirationTime() const { return m_txsExpirationTime; }
    
    /**
     * @brief Set block limit
     * @param blockLimit Block limit
     */
    void setBlockLimit(int64_t blockLimit) { m_blockLimit = blockLimit; }
    
    /**
     * @brief Get block limit
     * @return Block limit
     */
    int64_t blockLimit() const { return m_blockLimit; }

private:
    size_t m_poolLimit = 10000;  // Default pool limit
    uint64_t m_txsExpirationTime = 300000;  // Default 5 minutes
    int64_t m_blockLimit = 1000;  // Default block limit
};

} // namespace txpool
} // namespace parachain