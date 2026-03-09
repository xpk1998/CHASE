//
// Created by peng on 5/10/21.
//

#include "block_generator.h"
#include "../../database/impl/block_storage_to_file.h"
#include "../../database/impl/block_storage_to_db.h"
#include "../../../utilities/config/compile_config.h"
#include "../../build/proto_gen/block.pb.h"
#include "glog/logging.h"
#include "../../framework/protocol/Block.h"
#include "../../framework/protocol/BlockHeader.h"
#include "../../framework/protocol/Transaction.h"
#include "../../framework/protocol/TransactionReceipt.h"
#include "../../framework/protocol/impl/BlockImpl.h"
#include <filesystem>
#include <fstream>
#include <sstream>

std::unique_ptr<BlockStorage> getStorage() {
#ifdef USING_MEMORY_BLOCK
    return std::make_unique<BlockStorageToDB>();
#else
    return std::make_unique<BlockStorageToFile>();
#endif
}


BlockGenerator::BlockGenerator()
        : storage(getStorage()){
    // init priority queue
    getPendingTransactionQueue();
}

BlockGenerator::~BlockGenerator() = default;

std::unique_ptr<BlockGenerator::TxPriorityQueue> BlockGenerator::getPendingTransactionQueue() {
    auto pendingTransactionQueueRef = std::move(pendingTransactionQueue);
    // Create a new priority queue with the comparator
    auto comparator = [](Transaction* a, Transaction* b) {
        return a->getTransactionID() < b->getTransactionID();
    };
    pendingTransactionQueue = std::make_unique<TxPriorityQueue>(comparator);
    return pendingTransactionQueueRef;
}

epoch_size_t BlockGenerator::getLatestSavedEpoch() const {
    return storage->getLatestSavedEpoch();
}

bool BlockGenerator::loadBlock(epoch_size_t blockNum, std::string &serializedBlock) const {
    // 创建一个临时的bcos::protocol::Block对象来加载指定编号的区块
    auto tempBlock = std::make_shared<bcos::protocol::BlockImpl>();
    if (loadBlock(blockNum, *tempBlock)) {
        // 序列化为字符串
        bcos::bytes encodedData;
        tempBlock->encode(encodedData);
        serializedBlock.assign((char*)encodedData.data(), encodedData.size());
        return true;
    }
    return false;
}

bool BlockGenerator::loadBlock(epoch_size_t blockNum, bcos::protocol::Block& block) const {
    // 直接从文件系统加载区块文件
    std::string blockFile = "./blocks/block_" + std::to_string(blockNum) + ".dat";
    if (!std::filesystem::exists(blockFile)) {
        LOG(WARNING) << "Block file does not exist: " << blockFile;
        return false;
    }
    
    // 读取文件内容
    std::ifstream file(blockFile, std::ios::binary);
    if (!file.is_open()) {
        LOG(ERROR) << "Failed to open block file for reading: " << blockFile;
        return false;
    }
    
    // 读取整个文件内容
    std::string serializedData((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    file.close();
    
    // 将数据解码到目标block对象
    try {
        block.decode(bcos::ref((const unsigned char*)serializedData.data(), serializedData.size()), true, true);
        return true;
    } catch (...) {
        LOG(ERROR) << "Failed to parse block from file: " << blockFile;
        return false;
    }
}