#ifndef NEUBLOCKCHAIN_ERROR_MONITOR_H
#define NEUBLOCKCHAIN_ERROR_MONITOR_H

#include "transaction/transaction.h"
#include <atomic>
#include <map>
#include <string>

namespace executor {

class ErrorMonitor {
public:
    static ErrorMonitor& getInstance();
    
    void recordTransactionResult(TransactionResult result);
    std::map<std::string, uint64_t> getErrorStatistics();
    void resetStatistics();
    
    uint64_t getErrorCount(TransactionResult result);
    uint64_t getTotalTransactionCount();
    uint64_t getFailedTransactionCount();

private:
    ErrorMonitor() = default;
    
    std::atomic<uint64_t> commit_count_{0};
    std::atomic<uint64_t> pending_count_{0};
    std::atomic<uint64_t> abort_count_{0};
    std::atomic<uint64_t> abort_no_retry_count_{0};
    std::atomic<uint64_t> abort_gas_exhausted_count_{0};
    std::atomic<uint64_t> abort_execution_error_count_{0};
    std::atomic<uint64_t> abort_validation_fail_count_{0};
};

} // namespace executor

#endif //NEUBLOCKCHAIN_ERROR_MONITOR_H