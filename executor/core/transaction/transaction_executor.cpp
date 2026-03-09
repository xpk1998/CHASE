#include "transaction_executor.h"
#include "../impl/transaction_executor_impl.h"
#include "../impl/aggregation_transaction_executor.h"
#include "../impl/simulation_executor.h"
#include "../impl/transaction_query_executor.h"
#include <memory>

std::unique_ptr<TransactionExecutor> TransactionExecutor::transactionExecutorFactory(ExecutorType type) {
    std::unique_ptr<TransactionExecutor> executor;

    switch (type) {
        case ExecutorType::normal:
            executor = std::make_unique<TransactionExecutorImpl>();
            break;
        case ExecutorType::aggregate:
            executor = std::make_unique<AggregationTransactionExecutor>();
            break;
        case ExecutorType::query:
            // For QUERY type, we return a TransactionQueryExecutor
            executor = std::make_unique<TransactionQueryExecutor>();
            break;
        default:
            executor = std::make_unique<TransactionExecutorImpl>();
            break;
    }

    return executor;
}