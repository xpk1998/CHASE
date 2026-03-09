#include "Sha256.h"

namespace parachain::crypto {

// Implementation of SHA256 hash function
parachain::crypto::HashType Sha256::hashBytesRef(const bcos::bytesConstRef& _data) const
{
    return sha256Hash(_data);
}

// Get the hasher for SHA256
parachain::crypto::hasher::AnyHasher Sha256::hasher() const
{
    return parachain::crypto::hasher::AnyHasher{
        parachain::crypto::hasher::openssl::OpenSSL_SHA2_256_Hasher{}};
}

}  // namespace parachain::crypto