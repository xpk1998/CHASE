#pragma once
#include "../../hasher/AnyHasher.h"
#include "CommonType.h"
#include "KeyInterface.h"
#include "../../../network/gatew../../utilities/FixedBytes.h"
#include "../../../network/gatew../../utilities/Common.h"
#include <memory>
namespace parachain::crypto
{
enum HashImplType : int
{
    Keccak256Hash,
    Sm3Hash,
    Sha3
};
class Hash
{
public:
    using Ptr = std::shared_ptr<Hash>;
    using UniquePtr = std::unique_ptr<Hash>;
    Hash() = default;
    virtual ~Hash() = default;
    virtual parachain::crypto::HashType hashBytes(const bcos::bytesConstRef& _data) const = 0;
    virtual parachain::crypto::HashType hashBytesRef(const bcos::bytesConstRef& _data) const
    {
        return hashBytes(_data);
    }
    virtual parachain::crypto::HashType hash(const bcos::bytesConstRef& _data) const
    {
        return hashBytesRef(_data);
    }
    parachain::crypto::HashType hash(std::string_view view) const
    {
        return hashBytesRef(bcos::ref((const unsigned char*)view.data(), view.size()));
    }
    virtual parachain::crypto::HashType emptyHash()
    {
        if (parachain::crypto::HashType() == m_emptyHash)
        {
            m_emptyHash = hash(bcos::bytesConstRef());
        }
        return m_emptyHash;
    }
    virtual parachain::crypto::HashType hashBytesVec(const bcos::bytes& _data) const
    {
        return hashBytes(bcos::ref(_data.data(), _data.size()));
    }
    virtual parachain::crypto::HashType hash(std::string const& _data) const { return hashBytesRef(bcos::ref((const unsigned char*)_data.data(), _data.size())); }

    template <unsigned N>
    inline parachain::crypto::HashType hash(bcos::FixedBytes<N> const& _input) const
    {
        return hashBytesRef(bcos::ref(_input.ref(), N));
    }

    inline parachain::crypto::HashType hash(const bcos::crypto::PublicPtr& _public) const { return hashBytesRef(bcos::ref((const unsigned char*)_public->constData(), _public->size())); }

    inline void setHashImplType(HashImplType _type) { m_type = _type; }

    inline HashImplType getHashImplType() const { return m_type; }

    virtual hasher::AnyHasher hasher() const = 0;

private:
    parachain::crypto::HashType m_emptyHash = parachain::crypto::HashType();
    HashImplType m_type = Keccak256Hash;
};
}  // namespace parachain::crypto