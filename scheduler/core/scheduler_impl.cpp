#include "scheduler_impl.h"
#include "block_executive.h"
#include "../../executor/pipeline/pipeline_executor.h"
#include <glog/logging.h>
#include <thread>

namespace parachain {

SchedulerImpl::SchedulerImpl() {
    LOG(INFO) << "[SCHEDULER] Creating SchedulerImpl instance";
    
    // Initialize pipeline executor
    m_pipeline_executor = std::make_unique<blp::PipelineExecutor>(nullptr);
    
    // Start management thread
    m_management_thread = std::make_unique<std::thread>(&SchedulerImpl::manageBlockExecutives, this);
}

void SchedulerImpl::executeBlock(std::shared_ptr<Block> block, bool verify,
    std::function<void(std::shared_ptr<Error>, std::shared_ptr<BlockHeader>, bool _sysBlock)> callback) {
    LOG(INFO) << "[SCHEDULER] Starting block execution, block number: " << block->getBlockHeader()->getNumber();
    
    if (!block) {
        auto error = std::make_shared<Error>();
        error->errorCode = -1;
        error->errorMessage = "Invalid block";
        callback(error, nullptr, false);
        return;
    }

    // Create block executive for this block
    auto block_executive = createBlockExecutive(block);
    if (!block_executive) {
        auto error = std::make_shared<Error>();
        error->errorCode = -1;
        error->errorMessage = "Failed to create block executive";
        callback(error, nullptr, false);
        return;
    }

    // Update current block number
    m_current_block_number.store(block->getBlockHeader()->getNumber());

    // Execute the block
    block_executive->asyncExecute([this, block_executive, callback, verify](bool success) {
        if (success) {
            auto header = block_executive->getBlock()->getBlockHeader();
            LOG(INFO) << "[SCHEDULER] Block execution completed successfully: " << header->getNumber();
            callback(nullptr, header, false); // Not a system block
        } else {
            auto error = std::make_shared<Error>();
            error->errorCode = -1;
            error->errorMessage = "Block execution failed";
            callback(error, nullptr, false);
        }
    });
}

void SchedulerImpl::commitBlock(std::shared_ptr<BlockHeader> header,
    std::function<void(std::shared_ptr<Error>, std::shared_ptr<LedgerConfig>)> callback) {
    LOG(INFO) << "[SCHEDULER] Committing block: " << header->getNumber();
    
    // Update committed block number
    m_committed_block_number.store(header->getNumber());
    
    // Perform commit operations
    // In a real implementation, this would persist the block to storage
    LOG(INFO) << "[SCHEDULER] Block committed: " << header->getNumber();
    
    // Create a simple ledger config for the callback
    auto ledger_config = std::make_shared<LedgerConfig>();
    ledger_config->setBlockNumber(header->getNumber());
    
    callback(nullptr, ledger_config);
}

void SchedulerImpl::preExecuteBlock(std::shared_ptr<Block> block, bool verify,
    std::function<void(std::shared_ptr<Error>)> callback) {
    LOG(INFO) << "[SCHEDULER] Pre-executing block: " << block->getBlockHeader()->getNumber();
    
    // In a real implementation, this would prepare the block for execution
    // without actually executing it, allowing for pipeline optimization
    callback(nullptr);
}

void SchedulerImpl::call(std::shared_ptr<Transaction> tx,
    std::function<void(std::shared_ptr<Error>, std::shared_ptr<TransactionReceipt>)> callback) {
    LOG(INFO) << "[SCHEDULER] Calling transaction";
    
    // For direct calls, execute the transaction immediately
    // This is typically used for read-only operations
    auto error = std::make_shared<Error>();
    error->errorCode = -1;
    error->errorMessage = "Call method not implemented";
    callback(error, nullptr);
}

void SchedulerImpl::getCode(
    std::string_view contract_address,
    std::function<void(std::shared_ptr<Error>, bcos::bytes)> callback) {
    LOG(INFO) << "[SCHEDULER] Getting code for contract: " << contract_address;
    
    // In a real implementation, this would retrieve contract code from storage
    auto error = std::make_shared<Error>();
    error->errorCode = -1;
    error->errorMessage = "Get code method not implemented";
    callback(error, {});
}

void SchedulerImpl::registerExecutor(std::shared_ptr<PipelineExecutor> executor) {
    LOG(INFO) << "[SCHEDULER] Registering pipeline executor";
    // In a real implementation, this would register the executor
}

void SchedulerImpl::updateBlockNumber(BlockNumber block_number) {
    LOG(INFO) << "[SCHEDULER] Updating block number to: " << block_number;
    m_current_block_number.store(block_number);
}

std::shared_ptr<BlockExecutive> SchedulerImpl::createBlockExecutive(std::shared_ptr<Block> block) {
    LOG(INFO) << "[SCHEDULER] Creating block executive for block: " << block->getBlockHeader()->getNumber();
    
    auto block_executive = std::make_shared<BlockExecutive>(block);
    if (m_cache_manager) {
        block_executive->setCacheManager(m_cache_manager);
    }
    
    // Add to active block executives
    {
        std::lock_guard<std::mutex> lock(m_block_executives_mutex);
        m_block_executives[block->getBlockHeader()->getNumber()] = block_executive;
    }
    
    return block_executive;
}

void SchedulerImpl::manageBlockExecutives() {
    while (m_running.load()) {
        // Clean up completed block executives periodically
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // In a real implementation, this would clean up completed block executives
        // and manage memory usage
    }
}

} // namespace parachain