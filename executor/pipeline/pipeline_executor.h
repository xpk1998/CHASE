//
// PipelineExecutor: BLP流水线执行器
// 负责管理区块执行流水线，与调度层分离
//

#ifndef PARACHAIN_PIPELINE_EXECUTOR_H
#define PARACHAIN_PIPELINE_EXECUTOR_H

#include "parallel_pipeline.h"
#include "../../storage/core/cache/state_cache_manager.h"
#include "../../utilities/types/aria_types.h"
#include <functional>
#include <memory>
#include <vector>

class Transaction;

namespace blp {

class PipelineExecutor {
public:
    explicit PipelineExecutor(blp::StateCacheManager* cache_manager);
    ~PipelineExecutor() = default;
    
    // 删除拷贝构造和赋值操作
    PipelineExecutor(const PipelineExecutor&) = delete;
    PipelineExecutor& operator=(const PipelineExecutor&) = delete;
    PipelineExecutor(PipelineExecutor&&) = delete;
    PipelineExecutor& operator=(PipelineExecutor&&) = delete;
    
    /**
     * 执行区块流水线
     * @param epoch 区块高度
     * @param transactions 交易列表
     * @param callback 执行完成回调
     */
    void executeBlock(epoch_size_t epoch, 
                     const std::vector<Transaction*>& transactions,
                     std::function<void(bool success)> callback);
    
    /**
     * 获取流水线统计信息
     */
    pipeline::ParallelPipeline::Stats getStats() const;
    
    /**
     * 重置统计信息
     */
    void resetStats();

private:
    /**
     * 执行流水线阶段
     */
    void executePipelineStage(pipeline::StageType stage,
                            const std::vector<pipeline::PipelineTransaction>& transactions);
    
    /**
     * 转换交易为流水线交易
     */
    std::vector<pipeline::PipelineTransaction> convertToPipelineTransactions(
        const std::vector<Transaction*>& transactions);
    
    /**
     * 处理模拟阶段结果
     */
    void handleSimulationResults(std::vector<pipeline::PipelineTransaction>& transactions);
    
    /**
     * 处理调度阶段结果
     */
    void handleSchedulingResults(std::vector<pipeline::PipelineTransaction>& transactions);
    
    /**
     * 处理执行阶段结果
     */
    void handleExecutionResults(const std::vector<pipeline::PipelineTransaction>& transactions);
    
    /**
     * 处理提交阶段结果
     */
    void handleCommitResults(const std::vector<pipeline::PipelineTransaction>& transactions);
    
    // 成员变量
    blp::StateCacheManager* cache_manager_;
    std::unique_ptr<pipeline::ParallelPipeline> pipeline_;
};

} // namespace blp

#endif // PARACHAIN_PIPELINE_EXECUTOR_H