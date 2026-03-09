#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <string>
#include "../../executor/pipeline/parallel_pipeline.h"
#include "../../storage/core/cache/state_cache_manager.h"
#include "../../framework/protocol/Transaction.h"
#include "../../framework/protocol/TransactionReceipt.h"

namespace parachain {
namespace executor {

// BLP执行器接口
class BLPExecutorInterface {
public:
    virtual ~BLPExecutorInterface() = default;

    // 执行交易列表
    virtual void executeTransactions(
        const std::vector<std::shared_ptr<protocol::Transaction>>& transactions,
        std::shared_ptr<blp::StateCacheManager> cache_manager,
        std::function<void(bool success)> callback) = 0;

    // 预执行交易（用于优化）
    virtual void preExecuteTransactions(
        const std::vector<std::shared_ptr<protocol::Transaction>>& transactions,
        std::shared_ptr<blp::StateCacheManager> cache_manager,
        std::function<void(bool success)> callback) = 0;

    // 获取执行统计信息
    virtual pipeline::ParallelPipeline::Stats getStats() const = 0;

    // 重置统计信息
    virtual void resetStats() = 0;
};

} // namespace executor
} // namespace parachain