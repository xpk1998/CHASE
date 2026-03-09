#pragma once
#include "../hasher/OpenSSLHasher.h"
#include "../interfaces/crypto/Hash.h"

namespace bcos::crypto
{
HashType inline sha3Hash(bytesConstRef _data)
{
    hasher::openssl::OpenSSL_SHA3_256_Hasher hasher;
    hasher.update(_data);

    HashType out;
    hasher.final(out);
    return out;
}

class Sha3 : public Hash
{
public:
    using Ptr = std::shared_ptr<Sha3>;
    Sha3() { setHashImplType(HashImplType::Sha3); }
    ~Sha3() noexcept override = default;
    HashType hash(bytesConstRef _data) const override { return sha3Hash(_data); }
    bcos::crypto::hasher::AnyHasher hasher() const override
    {
        return bcos::crypto::hasher::AnyHasher{
            bcos::crypto::hasher::openssl::OpenSSL_SHA3_256_Hasher{}};
    };
};
}  // namespace bcos::crypto