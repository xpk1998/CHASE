#pragma once
#include "../hasher/OpenSSLHasher.h"
#include "../interfaces/crypto/Hash.h"

namespace parachain::crypto
{
inline HashType sha256Hash(const bcos::bytesConstRef& _data)
{
    hasher::openssl::OpenSSL_SHA2_256_Hasher hasher;
    hasher.update(_data);

    HashType out;
    hasher.final(out);
    return out;
}
class Sha256 : public Hash
{
public:
    using Ptr = std::shared_ptr<Sha256>;
    Sha256() { setHashImplType(HashImplType::Sha3); }
    ~Sha256() override = default;
    
    // Implement the pure virtual method from base class
    HashType hashBytes(const bcos::bytesConstRef& _data) const override { return sha256Hash(_data); }
    
    // Declare the methods that are implemented in Sha256Impl.cpp
    HashType hashBytesRef(const bcos::bytesConstRef& _data) const override;
    hasher::AnyHasher hasher() const override;
};
}  // namespace parachain::crypto