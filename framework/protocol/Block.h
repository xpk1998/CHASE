#pragma once
#include "BlockHeader.h"
#include "Transaction.h"
#include "TransactionMetaData.h"
#include "TransactionReceipt.h"
#include "../crypto/interfaces/crypto/CommonType.h"
#include "../crypto/interfaces/crypto/Hash.h"  // 添加Hash的引用
#include <vector>
#include <memory>

namespace bcos::protocol
{
using HashList = std::vector<parachain::crypto::HashType>;
using HashListPtr = std::shared_ptr<HashList>;
using HashListConstPtr = std::shared_ptr<const HashList>;

enum BlockType : int32_t
{
    CompleteBlock = 1,
    WithTransactionsHash = 2,
};

class Block
{
public:
    using Ptr = std::shared_ptr<Block>;
    using ConstPtr = std::shared_ptr<Block const>;
    Block() = default;
    Block(const Block&) = default;
    Block(Block&&) = default;
    Block& operator=(const Block&) = default;
    Block& operator=(Block&&) = default;
    virtual ~Block() = default;

    virtual void decode(bytesConstRef _data, bool _calculateHash, bool _checkSig) = 0;
    virtual void encode(bytes& _encodeData) const = 0;

    virtual parachain::crypto::HashType calculateTransactionRoot(const parachain::crypto::Hash& hashImpl) const = 0;
    virtual parachain::crypto::HashType calculateReceiptRoot(const parachain::crypto::Hash& hashImpl) const = 0;

    virtual int32_t version() const = 0;
    virtual void setVersion(int32_t _version) = 0;
    virtual BlockType blockType() const = 0;
    // blockHeader gets blockHeader
    virtual BlockHeader::ConstPtr blockHeaderConst() const = 0;
    virtual BlockHeader::Ptr blockHeader() = 0;
    // get transactions
    virtual Transaction::ConstPtr transaction(uint64_t _index) const = 0;
    // get receipts
    virtual TransactionReceipt::ConstPtr receipt(uint64_t _index) const = 0;
    // get transaction metaData
    virtual TransactionMetaData::ConstPtr transactionMetaData(uint64_t _index) const = 0;  // 修复类型
    // get transaction hash
    virtual parachain::crypto::HashType transactionHash(uint64_t _index) const
    {
        auto txMetaData = transactionMetaData(_index);  // 修复调用
        if (txMetaData)
        {
            return txMetaData->hash();
        }
        return {};
    }

    virtual void setBlockType(BlockType _blockType) = 0;
    // setBlockHeader sets blockHeader
    virtual void setBlockHeader(BlockHeader::Ptr _blockHeader) = 0;
    // set transactions
    virtual void setTransaction(uint64_t _index, Transaction::Ptr _transaction) = 0;
    // FIXME: appendTransaction will create Transaction, the parameter should be object not pointer
    virtual void appendTransaction(Transaction::Ptr _transaction) = 0;
    // set receipts
    virtual void setReceipt(uint64_t _index, TransactionReceipt::Ptr _receipt) = 0;
    virtual void appendReceipt(TransactionReceipt::Ptr _receipt) = 0;
    // set transaction metaData
    // FIXME: appendTransactionMetaData will create, parameter should be object instead of pointer
    virtual void appendTransactionMetaData(TransactionMetaData::Ptr _txMetaData) = 0;

    // get transactions size
    virtual uint64_t transactionsSize() const = 0;
    virtual uint64_t transactionsMetaDataSize() const = 0;
    virtual uint64_t transactionsHashSize() const { return transactionsMetaDataSize(); }

    // get receipts size
    virtual uint64_t receiptsSize() const = 0;

    // for nonceList
    // 简化nonceList相关功能，避免复杂的ranges使用
    virtual void setNonceList(std::vector<std::string> nonces) = 0;
    virtual std::vector<std::string> nonceList() const = 0;

    virtual std::shared_ptr<std::vector<std::string>> nonces() const
    {
        // 简化实现，返回空的nonce列表
        return std::make_shared<std::vector<std::string>>();
    }
    bool operator<(const Block& block) const
    {
        return blockHeaderConst()->number() < block.blockHeaderConst()->number();
    }
    virtual size_t size() const = 0;
};
using Blocks = std::vector<Block::Ptr>;
using BlocksPtr = std::shared_ptr<Blocks>;

}  // namespace bcos::protocol