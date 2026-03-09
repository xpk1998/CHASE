#pragma once

#include "scheduler_impl.h"
#include "../../executor/kdg/kdg_executor.h"
#include "../../sealer/core/sealer.h"
#include "../../txpool/blp/blp_txpool.h"
#include <memory>
#include <map>
#include <mutex>
#include <atomic>

namespace parachain {

class SchedulerManager : public SchedulerInterface {
public:
    explicit SchedulerManager();
    virtual ~SchedulerManager() = default;

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

    // Scheduler management methods
    void addScheduler(const std::string& name, std::shared_ptr<SchedulerImpl> scheduler);
    std::shared_ptr<SchedulerImpl> getCurrentScheduler() const;
    void setCurrentScheduler(const std::string& name);

    // Update block number across all schedulers
    void updateBlockNumber(BlockNumber block_number);

    // Pipeline coordination methods
    void initializePipeline(
        std::shared_ptr<Sealer> sealer,
        std::shared_ptr<executor::KDGTransactionExecutor> executor,
        std::shared_ptr<BLPTxPool> txpool);
    
    void startPipeline();
    void stopPipeline();
    bool isPipelineRunning() const { return m_pipeline_running.load(); }
    
    // Sealer to Scheduler block transmission
    void transmitBlockToScheduler(std::shared_ptr<Block> block,
        std::function<void(std::shared_ptr<Error>)> callback);
    
    // Scheduler to Executor transaction distribution
    void distributeTransactionsToExecutor(
        const std::vector<Transaction*>& transactions,
        std::function<void(bool success)> callback);
    
    // TxPool to Sealer transaction supply
    void supplyTransactionsToSealer(
        size_t max_count,
        std::function<void(std::vector<std::shared_ptr<Transaction>>)> callback);
    
    // State synchronization between modules
    void synchronizeState();

private:
    // Member variables
    std::atomic<BlockNumber> m_current_scheduler_term{0};
    mutable std::mutex m_schedulers_mutex;
    std::map<std::string, std::shared_ptr<SchedulerImpl>> m_schedulers;
    std::string m_current_scheduler_name;
    
    // Pipeline coordination components
    std::shared_ptr<Sealer> m_sealer;
    std::shared_ptr<executor::KDGTransactionExecutor> m_executor;
    std::shared_ptr<BLPTxPool> m_txpool;
    
    // Pipeline state
    std::atomic<bool> m_pipeline_running{false};
    mutable std::mutex m_pipeline_mutex;
};

} // namespace parachain