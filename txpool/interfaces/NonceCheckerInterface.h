/**
 * @file NonceCheckerInterface.h
 * @brief Nonce checker interface for Parachain
 * @author Parachain Team
 * @date 2025
 */
#pragma once

#include <string>
#include <memory>
#include <set>
#include "transaction.pb.h"

namespace parachain {
namespace txpool {

class NonceCheckerInterface {
public:
    using Ptr = std::shared_ptr<NonceCheckerInterface>;
    
    NonceCheckerInterface() = default;
    virtual ~NonceCheckerInterface() = default;
    
    /**
     * @brief Check if nonce exists
     * @param nonce Nonce to check
     * @return true if nonce exists, false otherwise
     */
    virtual bool exists(const std::string& nonce) = 0;
    
    /**
     * @brief Insert nonce
     * @param nonce Nonce to insert
     */
    virtual void insert(const std::string& nonce) = 0;
    
    /**
     * @brief Remove nonce
     * @param nonce Nonce to remove
     */
    virtual void remove(const std::string& nonce) = 0;
    
    /**
     * @brief Batch insert nonces
     * @param nonces Nonces to insert
     */
    virtual void batchInsert(const std::set<std::string>& nonces) = 0;
    
    /**
     * @brief Batch remove nonces
     * @param nonces Nonces to remove
     */
    virtual void batchRemove(const std::set<std::string>& nonces) = 0;
};

} // namespace txpool
} // namespace parachain