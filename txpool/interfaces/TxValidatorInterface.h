/**
 * @file TxValidatorInterface.h
 * @brief Transaction validator interface for Parachain
 * @author Parachain Team
 * @date 2025
 */
#pragma once

#include <string>
#include <memory>
#include "transaction.pb.h"

namespace parachain {
namespace txpool {

enum class TransactionStatus {
    None = 0,
    InvalidSignature,          // Invalid transaction signature
    InvalidNonce,             // Transaction nonce is invalid
    NotEnoughCash,            // Not enough cash to cover transaction cost
    NonceCheckFail,           // Nonce check failed
    TransactionAlreadyInChain, // Transaction is already in chain
    ExpirationTimestamp,      // Transaction expired
    AlreadyInTxPool,          // Transaction is already in txpool
    TxPoolIsFull,             // Transaction pool is full
    Malformed,                // Malformed transaction
    OverGasLimit,             // Transaction gas exceeds limit
    ReceiptAlreadyInChain,    // Receipt is already in chain
    TxPoolNonceCheckFail,     // TxPool nonce check failed
    TransactionPoolTimeout,   // Transaction pool timeout
    TimeLimitExceeded,        // Time limit exceeded
    GasPriceTooLow,           // Gas price too low
    Unknown                  // Unknown error
};

class TxValidatorInterface {
public:
    using Ptr = std::shared_ptr<TxValidatorInterface>;
    
    TxValidatorInterface() = default;
    virtual ~TxValidatorInterface() = default;
    
    /**
     * @brief Verify transaction validity
     * @param tx Transaction to verify
     * @return TransactionStatus verification result
     */
    virtual TransactionStatus verify(const TransactionPayload& tx) = 0;
    
    /**
     * @brief Check transaction nonce
     * @param tx Transaction to check
     * @return TransactionStatus check result
     */
    virtual TransactionStatus checkNonce(const TransactionPayload& tx) = 0;
    
    /**
     * @brief Check transaction signature
     * @param tx Transaction to check
     * @return TransactionStatus check result
     */
    virtual TransactionStatus checkSignature(const TransactionPayload& tx) = 0;
};

} // namespace txpool
} // namespace parachain