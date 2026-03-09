//
// Created by peng on 2/17/21.
//

#ifndef NEUBLOCKCHAIN_BLOCK_GENERATOR_H
#define NEUBLOCKCHAIN_BLOCK_GENERATOR_H

#include <queue>
#include <memory>
#include <functional>

// Include BlockInfoHelper class definition
#include "../../database/block_info_helper.h"

// Include Block protobuf definition (可能不再需要，但保留以防其他地方使用)
#include "../../build/proto_gen/block.pb.h"

// Forward declaration
class BlockStorage;

// Include Transaction class definition
#include "../../../executor/transaction/transaction.h"

// Include the actual Block class definition
#include "../../framework/protocol/Block.h"

class BlockGenerator: public BlockInfoHelper {
public:
    virtual ~BlockGenerator();
    // add an executed tx to block chain, not thread safe.
    inline void addCommittedTransaction(Transaction* transaction) { pendingTransactionQueue->push(transaction); }
    // async generate a block and broadcast to other server later, not thread safe.
    virtual bool generateBlock() = 0;

public:
    // override function impl here
    bool loadBlock(epoch_size_t blockNum, std::string& serializedBlock) const override;
    bool loadBlock(epoch_size_t blockNum, bcos::protocol::Block& block) const override;  // 修复：使用正确的Block类型

    [[nodiscard]] epoch_size_t getLatestSavedEpoch() const override;

    // accept a storage, automatic deleted it when destroyed
    BlockGenerator();
    std::unique_ptr<BlockStorage> storage;

    using TxPriorityQueue = std::priority_queue<Transaction*, std::vector<Transaction*>, std::function<bool(Transaction*, Transaction*)>>;
    std::unique_ptr<TxPriorityQueue> getPendingTransactionQueue();

private:
    std::unique_ptr<TxPriorityQueue> pendingTransactionQueue;
};

#endif //NEUBLOCKCHAIN_BLOCK_GENERATOR_H