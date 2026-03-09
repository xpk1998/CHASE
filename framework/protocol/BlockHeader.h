#pragma once
#include <memory>
#include "Transaction.h"
#include "../crypto/interfaces/crypto/CommonType.h"

namespace bcos::protocol
{
class BlockHeader
{
public:
    using Ptr = std::shared_ptr<BlockHeader>;
    using ConstPtr = std::shared_ptr<const BlockHeader>;

    virtual ~BlockHeader() = default;

    virtual int64_t number() const = 0;
    virtual void setNumber(int64_t number) = 0;

    virtual parachain::crypto::HashType hash() const = 0;
    virtual void calculateHash() = 0;
};
}  // namespace bcos::protocol