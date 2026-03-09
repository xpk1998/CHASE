#include "transaction_executor_impl.h"
#include "../transaction/transaction.h"
#include "../transaction/evm_transaction_interface.h"
#include "../../storage/database/db_storage.h"
#include "glog/logging.h"
#include <evmc/evmc.h>
#include <evmc/instructions.h>

TransactionExecutorImpl::TransactionExecutorImpl() : storage_(nullptr) {
    LOG(INFO) << "TransactionExecutorImpl created";
}

TransactionExecutorImpl::~TransactionExecutorImpl() {
    LOG(INFO) << "TransactionExecutorImpl destroyed";
}

bool TransactionExecutorImpl::executeList(const std::vector<Transaction*>& transactionList) {
    if (transactionList.empty()) {
        LOG(WARNING) << "TransactionExecutorImpl: Empty transaction list provided";
        return true;
    }
    
    LOG(INFO) << "TransactionExecutorImpl: Executing " << transactionList.size() << " transactions";
    
    bool allSuccess = true;
    
    for (auto* transaction : transactionList) {
        if (!transaction) {
            LOG(WARNING) << "TransactionExecutorImpl: Null transaction in list, skipping";
            continue;
        }
        
        if (!executeTransaction(transaction)) {
            LOG(ERROR) << "TransactionExecutorImpl: Failed to execute transaction " 
                      << transaction->getTransactionID();
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool TransactionExecutorImpl::executeTransaction(Transaction* tx) {
    if (!tx) {
        LOG(ERROR) << "TransactionExecutorImpl: Cannot execute null transaction";
        return false;
    }
    
    // Try to cast to EVMTransactionInterface
    auto* evm_tx = dynamic_cast<EVMTransactionInterface*>(tx);
    if (evm_tx) {
        // Execute EVM transaction
        LOG(INFO) << "TransactionExecutorImpl: Executing EVM transaction " 
                  << evm_tx->getTransactionID();
        return executeEVMTransaction(evm_tx, storage_);
    } else {
        // Handle non-EVM transactions
        LOG(INFO) << "TransactionExecutorImpl: Executing non-EVM transaction " 
                  << tx->getTransactionID();
        // In a real implementation, we would execute the transaction here
        // For now, we'll just mark it as successful
        tx->setTransactionResult(TransactionResult::COMMIT);
        return true;
    }
}

bool TransactionExecutorImpl::executeEVMTransaction(EVMTransactionInterface* evm_tx, DBStorage* storage) {
    if (!evm_tx || !block_context_ || !storage) {
        LOG(ERROR) << "TransactionExecutorImpl: Invalid parameters for EVM transaction execution";
        return false;
    }
    
    try {
        // Create EVMC message from transaction
        evmc_message message = createEVMCMessage(evm_tx);
        
        // Get contract code
        neuchain::evm::bytes code;
        neuchain::evm::Address contractAddress = evm_tx->getContractAddress();
        
        // For contract creation, use the input as code
        if (evm_tx->isContractCreation()) {
            code = evm_tx->getContractInput();
        } else {
            // For contract call, get code from storage
            if (!getContractCode(storage, contractAddress, code)) {
                LOG(WARNING) << "TransactionExecutorImpl: Failed to get contract code for address";
                // Contract might not exist, return success with revert status
                neuchain::evm::ExecutionResult result;
                result.status_code = EVMC_REVERT;
                result.gas_left = 0;
                result.output.clear();
                applyExecutionResult(evm_tx, result);
                return true;
            }
        }
        
        // Execute the EVM bytecode
        neuchain::evm::ExecutionResult result = executeEVMBytecode(message, code, *block_context_);
        
        // Apply execution result to transaction
        applyExecutionResult(evm_tx, result);
        
        // Set transaction result based on EVM execution
        if (result.isSuccess()) {
            evm_tx->setTransactionResult(TransactionResult::COMMIT);
        } else if (result.isRevert()) {
            evm_tx->setTransactionResult(TransactionResult::ABORT);
        } else {
            evm_tx->setTransactionResult(TransactionResult::ABORT_EXECUTION_ERROR);
        }
        
        LOG(INFO) << "TransactionExecutorImpl: EVM transaction " << evm_tx->getTransactionID() 
                  << " executed with status " << result.status_code;
        
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "TransactionExecutorImpl: Exception during EVM transaction execution: " << e.what();
        evm_tx->setTransactionResult(TransactionResult::ABORT_EXECUTION_ERROR);
        return false;
    }
}

// Create EVMC message from EVM transaction
evmc_message TransactionExecutorImpl::createEVMCMessage(const EVMTransactionInterface* evm_tx) const {
    evmc_message message{};
    
    // Set message kind (CALL or CREATE)
    message.kind = evm_tx->isContractCreation() ? EVMC_CREATE : EVMC_CALL;
    
    // Set gas limit
    message.gas = evm_tx->getGasLimit();
    
    // Set sender address
    evmc_address senderAddr{};
    const neuchain::evm::Address& sender = evm_tx->getSender();
    std::copy(sender.begin(), sender.end(), senderAddr.bytes);
    message.sender = senderAddr;
    
    // Set recipient address (for CALL, empty for CREATE)
    if (!evm_tx->isContractCreation()) {
        evmc_address recipientAddr{};
        const neuchain::evm::Address& recipient = evm_tx->getContractAddress();
        std::copy(recipient.begin(), recipient.end(), recipientAddr.bytes);
        message.recipient = recipientAddr;
    }
    
    // Set input data
    const neuchain::evm::bytes& input = evm_tx->getContractInput();
    message.input_data = input.data();
    message.input_size = input.size();
    
    // Set value (using simple approach instead of intx)
    // For now, we'll just zero-initialize the value
    std::fill(std::begin(message.value.bytes), std::end(message.value.bytes), 0);
    
    // Set other fields
    message.depth = 0;
    message.flags = 0;
    message.create2_salt = {};
    
    return message;
}

// Execute EVM bytecode using EVMC
neuchain::evm::ExecutionResult TransactionExecutorImpl::executeEVMBytecode(
    const evmc_message& message, 
    const neuchain::evm::bytes& code,
    const neuchain::evm::BlockContext& blockContext) {
    
    neuchain::evm::ExecutionResult result{};
    
    try {
        // For simplicity, we'll simulate EVM execution
        // In a real implementation, this would use an actual EVM implementation like evmone
        
        // Check gas limit
        if (message.gas < 0) {
            result.status_code = EVMC_OUT_OF_GAS;
            result.gas_left = 0;
            return result;
        }
        
        // Simulate execution
        result.status_code = EVMC_SUCCESS;
        result.gas_left = message.gas > 21000 ? message.gas - 21000 : 0;  // Simulate gas consumption
        
        // Simulate output (for contract calls)
        if (message.kind == EVMC_CALL && message.input_size > 0) {
            result.output = neuchain::evm::bytes{0x00, 0x01, 0x02, 0x03};  // Dummy output
        }
        
        // Simulate contract creation
        if (message.kind == EVMC_CREATE) {
            // Generate a dummy contract address
            neuchain::evm::Address dummyAddress{};
            for (size_t i = 0; i < dummyAddress.size(); ++i) {
                dummyAddress[i] = static_cast<uint8_t>(i);
            }
            result.create_address = dummyAddress;
        }
        
        // Simulate logs (events)
        if (!code.empty()) {
            neuchain::evm::LogEntry log;
            std::copy(std::begin(message.recipient.bytes), std::end(message.recipient.bytes), log.address.begin());
            log.topics.push_back(neuchain::evm::Hash{});  // Dummy topic
            log.data = neuchain::evm::bytes{0x01, 0x02, 0x03, 0x04};  // Dummy data
            result.logs.push_back(log);
        }
        
    } catch (const std::exception& e) {
        LOG(ERROR) << "TransactionExecutorImpl: Exception during EVM bytecode execution: " << e.what();
        result.status_code = EVMC_INTERNAL_ERROR;
        result.gas_left = 0;
    }
    
    return result;
}

// Get contract code from storage
bool TransactionExecutorImpl::getContractCode(DBStorage* storage, const neuchain::evm::Address& address, 
                                            neuchain::evm::bytes& code) const {
    if (!storage) {
        return false;
    }
    
    try {
        // Convert address to string key
        std::string addressStr(address.begin(), address.end());
        std::string codeStr;
        
        // Try to get code from storage
        if (storage->get("contracts", addressStr, codeStr)) {
            code.assign(codeStr.begin(), codeStr.end());
            return true;
        }
        
        // Contract code not found
        return false;
    } catch (const std::exception& e) {
        LOG(ERROR) << "TransactionExecutorImpl: Exception during contract code retrieval: " << e.what();
        return false;
    }
}

// Apply execution result to transaction
void TransactionExecutorImpl::applyExecutionResult(EVMTransactionInterface* evm_tx, 
                                                const neuchain::evm::ExecutionResult& result) {
    if (!evm_tx) {
        return;
    }
    
    evm_tx->setExecutionResult(result);
}

void TransactionExecutorImpl::setBlockContext(const neuchain::evm::BlockContext& block_context) {
    if (!block_context_) {
        block_context_ = std::make_unique<neuchain::evm::BlockContext>();
    }
    *block_context_ = block_context;
    LOG(INFO) << "TransactionExecutorImpl: Block context set";
}

bool TransactionExecutorImpl::commitList(const std::vector<Transaction*>& transactionList) {
    if (transactionList.empty()) {
        LOG(WARNING) << "TransactionExecutorImpl: Empty transaction list provided for commit";
        return true;
    }
    
    LOG(INFO) << "TransactionExecutorImpl: Committing " << transactionList.size() << " transactions";
    
    size_t committedCount = 0;
    
    for (auto* transaction : transactionList) {
        if (!transaction) {
            LOG(WARNING) << "TransactionExecutorImpl: Null transaction in commit list, skipping";
            continue;
        }
        
        // For transaction executor, committing means marking transactions as fully committed
        uint64_t txId = transaction->getTransactionID();
        LOG(INFO) << "TransactionExecutorImpl: Committing transaction " << txId;
        
        // Mark transaction as committed
        transaction->setTransactionResult(TransactionResult::COMMIT);
        committedCount++;
    }
    
    LOG(INFO) << "TransactionExecutorImpl: Committed " << committedCount << " transactions";
    
    // Return true to indicate successful commit
    return true;
}