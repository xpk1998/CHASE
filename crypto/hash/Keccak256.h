#pragma once

#include "../hasher/OpenSSLHasher.h"
#include "../interfaces/crypto/Hash.h"
#ifndef ONLY_CPP_SDK
// #include <wedpr-crypto/WedprCrypto.h>
#endif

namespace bcos::crypto
{

inline parachain::crypto::HashType keccak256Hash(const bcos::bytesConstRef& _data)
{
    parachain::crypto::hasher::openssl::OpenSSL_Keccak256_Hasher hasher;
    hasher.update(_data);

    parachain::crypto::HashType out;
    hasher.final(out);
    return out;
}

class Keccak256 : public parachain::crypto::Hash
{
public:
    using Ptr = std::shared_ptr<Keccak256>;
    Keccak256() { setHashImplType(parachain::crypto::HashImplType::Keccak256Hash); }
    ~Keccak256() noexcept override = default;
    parachain::crypto::HashType hashBytes(const bcos::bytesConstRef& _data) const override { return keccak256Hash(_data); }
    parachain::crypto::HashType hashBytesRef(const bcos::bytesConstRef& _data) const override { return keccak256Hash(_data); }
    parachain::crypto::hasher::AnyHasher hasher() const override
    {
        return parachain::crypto::hasher::AnyHasher{
            parachain::crypto::hasher::openssl::OpenSSL_Keccak256_Hasher{}};
    }
};

}  // namespace bcos::crypto