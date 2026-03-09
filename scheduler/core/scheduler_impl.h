#pragma once

#include "../interface/scheduler_interface.h"
#include "block_executive.h"
#include "../../executor/pipeline/pipeline_executor.h"
#include "../../executor/kdg/address_based_conflict_graph.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <vector>
#include <thread>
#include <condition_variable>

namespace parachain {

class SchedulerImpl : public SchedulerInterface {
public:
    explicit SchedulerImpl();
    virtual ~SchedulerImpl() = default;

    // SchedulerInterface implementation
    void executeBlock(std::shared_ptr<Block> block, bool verify,
        std::function<void(std::shared_ptr<Error>, std::shared_ptr<BlockHeader>, bool _sysBlock)> callback) override;

    void commitBlock(std::shared_ptr<BlockHeader> header,
        std::function<void(std::shared_ptr<Error>, std::shared_ptr<LedgerConfig>)> callback) override;

    void preExecuteBlock(std::shared_ptr<Block> block, bool verify,
        std::function<void(std::shared_ptr<Error>)> callback) override;

    void call(std::shared_ptr<Transaction> tx,
        std::function<void(std::shared_ptr<Error>, std::shared_ptr<TransactionReceipt>)> callback) override;

    void getCode(
        std::string_view contract_address,
        std::function<void(std::shared_ptr<Error>, bcos::bytes)> callback) override;

    // Additional methods for ParaChain's KDG-based approach
    void registerExecutor(std::shared_ptr<PipelineExecutor> executor);
    void updateBlockNumber(BlockNumber block_number);

private:
    // Manage active block executives
    void manageBlockExecutives();
    
    // Create block executive for a block
    std::shared_ptr<BlockExecutive> createBlockExecutive(std::shared_ptr<Block> block);

    // Member variables
    std::atomic<BlockNumber> m_current_block_number{0};
    std::atomic<BlockNumber> m_committed_block_number{0};
    std::atomic<bool> m_running{true};
    std::unique_ptr<std::thread> m_management_thread;
    std::mutex m_block_executives_mutex;
    std::map<BlockNumber, std::shared_ptr<BlockExecutive>> m_block_executives;
    std::unique_ptr<blp::PipelineExecutor> m_pipeline_executor;
    std::shared_ptr<blp::StateCacheManager> m_cache_manager;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
};

} // namespace parachain