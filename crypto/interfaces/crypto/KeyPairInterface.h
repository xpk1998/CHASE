#pragma once
#include "Hash.h"
#include "KeyInterface.h"
#include <cstddef>
#include <memory>
#include <vector>
#include <string_view>

namespace bcos::crypto
{
enum class KeyPairType : int
{
    Secp256K1 = 0,
    SM2 = 1,
    Ed25519 = 2,
    HsmSM2 = 3
};

using SecretPtr = std::shared_ptr<const KeyInterface>;

class KeyPairInterface
{
public:
    using Ptr = std::shared_ptr<KeyPairInterface>;
    using UniquePtr = std::unique_ptr<KeyPairInterface>;

    KeyPairInterface() = default;
    KeyPairInterface(const KeyPairInterface&) = default;
    KeyPairInterface(KeyPairInterface&&) = delete;
    KeyPairInterface& operator=(const KeyPairInterface&) = default;
    KeyPairInterface& operator=(KeyPairInterface&&) = delete;
    virtual ~KeyPairInterface() = default;

    virtual SecretPtr secretKey() const = 0;
    virtual PublicPtr publicKey() const = 0;
    virtual parachain::crypto::Address address(parachain::crypto::Hash::Ptr _hashImpl) = 0;
    virtual KeyPairType keyPairType() const = 0;
};

parachain::crypto::Address inline calculateAddress(parachain::crypto::Hash::Ptr _hashImpl, PublicPtr _publicKey)
{
    return right160(_hashImpl->hash(_publicKey));
}

parachain::crypto::Address inline calculateAddress(parachain::crypto::Hash& _hashImpl, PublicPtr _publicKey)
{
    return right160(_hashImpl.hash(_publicKey));
}

parachain::crypto::Address inline calculateAddress(parachain::crypto::Hash& _hashImpl, uint8_t* _publicKey, size_t _len)
{
    auto address = _hashImpl.hash(bcos::bytes(_publicKey, _publicKey + _len));
    // Extract the last 20 bytes (160 bits) for the address
    parachain::crypto::Address result;
        const auto& addrData = address.data();
        // Copy the last 20 bytes (160 bits) for the address
        std::copy(addrData.end() - 20, addrData.end(), result.data().begin());
        return result;
}

}  // namespace bcos::crypto