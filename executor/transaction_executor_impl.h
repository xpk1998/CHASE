//
// Created by peng on 2021/1/22.
//

#ifndef NEUBLOCKCHAIN_TRANSACTIONEXECUTORIMPL_H
#define NEUBLOCKCHAIN_TRANSACTIONEXECUTORIMPL_H

#include "transaction_executor.h"
#include "../transaction/evm_transaction_interface.h"
#include "../../runtime/vm/types.h"
#include "../../storage/database/db_storage.h"
#include <evmc/evmc.h>
#include <memory>
#include <vector>

// Forward declarations
class ReserveTable;
class DBStorage;

namespace neuchain {
namespace evm {
    struct BlockContext;
    struct TransactionContext;
}
}

class TransactionExecutorImpl: public TransactionExecutor {
public:
    TransactionExecutorImpl();
    virtual ~TransactionExecutorImpl();
    
    bool executeList(const std::vector<Transaction*>& transactionList) override;
    bool commitList(const std::vector<Transaction*>& transactionList) override;
    
    // Set block context for EVM execution
    void setBlockContext(const neuchain::evm::BlockContext& block_context);
    
private:
    // Execute single transaction
    bool executeTransaction(Transaction* tx);
    
    // Execute EVM transaction
    bool executeEVMTransaction(EVMTransactionInterface* evm_tx, DBStorage* storage);
    
    // Create EVMC message from EVM transaction
    evmc_message createEVMCMessage(const EVMTransactionInterface* evm_tx) const;
    
    // Execute EVM bytecode
    neuchain::evm::ExecutionResult executeEVMBytecode(
        const evmc_message& message, 
        const neuchain::evm::bytes& code,
        const neuchain::evm::BlockContext& blockContext);
    
    // Get contract code from storage
    bool getContractCode(DBStorage* storage, const neuchain::evm::Address& address, 
                        neuchain::evm::bytes& code) const;
    
    // Apply execution result to transaction
    void applyExecutionResult(EVMTransactionInterface* evm_tx, 
                            const neuchain::evm::ExecutionResult& result);
    
    // Block context
    std::unique_ptr<neuchain::evm::BlockContext> block_context_;
    
    // Storage interface
    DBStorage* storage_;
};


#endif //NEUBLOCKCHAIN_TRANSACTIONEXECUTORIMPL_H