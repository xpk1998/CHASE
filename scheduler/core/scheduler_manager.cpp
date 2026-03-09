#include "scheduler_manager.h"
#include <glog/logging.h>

namespace parachain {

SchedulerManager::SchedulerManager() {
    LOG(INFO) << "[SCHEDULER_MANAGER] Creating SchedulerManager instance";
    m_current_scheduler_term = 0;
}

void SchedulerManager::executeBlock(std::shared_ptr<Block> block, bool verify,
    std::function<void(std::shared_ptr<Error>, std::shared_ptr<BlockHeader>, bool _sysBlock)> callback) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Executing block: " << block->getBlockHeader()->getNumber();
    
    auto scheduler = getCurrentScheduler();
    if (!scheduler) {
        auto error = std::make_shared<Error>();
        callback(error, nullptr, false);
        return;
    }
    
    scheduler->executeBlock(block, verify, callback);
}

void SchedulerManager::commitBlock(std::shared_ptr<BlockHeader> header,
    std::function<void(std::shared_ptr<Error>, std::shared_ptr<LedgerConfig>)> callback) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Committing block: " << header->getNumber();
    
    auto scheduler = getCurrentScheduler();
    if (!scheduler) {
        auto error = std::make_shared<Error>();
        callback(error, nullptr);
        return;
    }
    
    scheduler->commitBlock(header, callback);
}

void SchedulerManager::preExecuteBlock(std::shared_ptr<Block> block, bool verify,
    std::function<void(std::shared_ptr<Error>)> callback) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Pre-executing block: " << block->getBlockHeader()->getNumber();
    
    auto scheduler = getCurrentScheduler();
    if (!scheduler) {
        auto error = std::make_shared<Error>();
        callback(error);
        return;
    }
    
    scheduler->preExecuteBlock(block, verify, callback);
}

void SchedulerManager::call(std::shared_ptr<Transaction> tx,
    std::function<void(std::shared_ptr<Error>, std::shared_ptr<TransactionReceipt>)> callback) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Calling transaction";
    
    auto scheduler = getCurrentScheduler();
    if (!scheduler) {
        auto error = std::make_shared<Error>();
        callback(error, nullptr);
        return;
    }
    
    scheduler->call(tx, callback);
}

void SchedulerManager::getCode(
    std::string_view contract_address,
    std::function<void(std::shared_ptr<Error>, bcos::bytes)> callback) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Getting code for contract: " << contract_address;
    
    auto scheduler = getCurrentScheduler();
    if (!scheduler) {
        auto error = std::make_shared<Error>();
        callback(error, {});
        return;
    }
    
    scheduler->getCode(contract_address, callback);
}

void SchedulerManager::addScheduler(const std::string& name, std::shared_ptr<SchedulerImpl> scheduler) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Adding scheduler: " << name;
    
    std::lock_guard<std::mutex> lock(m_schedulers_mutex);
    m_schedulers[name] = scheduler;
    
    if (m_current_scheduler_name.empty()) {
        m_current_scheduler_name = name;
    }
}

std::shared_ptr<SchedulerImpl> SchedulerManager::getCurrentScheduler() const {
    std::lock_guard<std::mutex> lock(m_schedulers_mutex);
    
    auto it = m_schedulers.find(m_current_scheduler_name);
    if (it != m_schedulers.end()) {
        return it->second;
    }
    
    // Return the first available scheduler if current one is not found
    if (!m_schedulers.empty()) {
        return m_schedulers.begin()->second;
    }
    
    return nullptr;
}

void SchedulerManager::setCurrentScheduler(const std::string& name) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Setting current scheduler to: " << name;
    
    std::lock_guard<std::mutex> lock(m_schedulers_mutex);
    if (m_schedulers.find(name) != m_schedulers.end()) {
        m_current_scheduler_name = name;
    } else {
        LOG(WARNING) << "[SCHEDULER_MANAGER] Scheduler not found: " << name;
    }
}

void SchedulerManager::updateBlockNumber(BlockNumber block_number) {
    LOG(INFO) << "[SCHEDULER_MANAGER] Updating block number to: " << block_number;
    
    std::lock_guard<std::mutex> lock(m_schedulers_mutex);
    for (auto& [name, scheduler] : m_schedulers) {
        scheduler->updateBlockNumber(block_number);
    }
}

} // namespace parachain