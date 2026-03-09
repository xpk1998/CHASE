//
// Created for Enhanced Fault Tolerance
// Transaction Exception Handler declaration
//

#ifndef NEUBLOCKCHAIN_TRANSACTION_EXCEPTION_HANDLER_H
#define NEUBLOCKCHAIN_TRANSACTION_EXCEPTION_HANDLER_H

#include "transaction.h"
#include <string>

namespace executor {

class TransactionExceptionHandler {
public:
    // Handle EVM execution result and map to appropriate TransactionResult
    static TransactionResult handleEVMExecutionResult(
        Transaction* tx,
        const std::string& error_message);

    // Record detailed error information for debugging
    static void recordDetailedErrorInfo(
        Transaction* tx,
        const std::string& error_category,
        const std::string& error_message);

    // Check if an error is retryable
    static bool isRetryableError(TransactionResult result);

    // Get human-readable description of transaction result
    static std::string getErrorDescription(TransactionResult result);
};

} // namespace executor

#endif //NEUBLOCKCHAIN_TRANSACTION_EXCEPTION_HANDLER_H