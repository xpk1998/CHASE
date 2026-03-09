#pragma once
#include "CommonType.h"
#include "KeyInterface.h"
#include "KeyPairInterface.h"
#include <memory>
#include <mutex>
namespace bcos::crypto
{
class SignatureCrypto
{
public:
    using Ptr = std::shared_ptr<SignatureCrypto>;
    using UniquePtr = std::unique_ptr<SignatureCrypto>;
    SignatureCrypto() = default;
    virtual ~SignatureCrypto() = default;

    // sign returns a signature of a given hash
    virtual std::shared_ptr<bytes> sign(const KeyPairInterface& _keyPair, const parachain::crypto::HashType& _hash,
        bool _signatureWithPub = false) const = 0;

    // verify checks whether a signature is calculated from a given hash
    virtual bool verify(
        PublicPtr _pubKey, const parachain::crypto::HashType& _hash, bytesConstRef _signatureData) const = 0;
    virtual bool verify(std::shared_ptr<const bytes> _pubKeyBytes, const parachain::crypto::HashType& _hash,
        bytesConstRef _signatureData) const = 0;

    // recover recovers the public key from the given signature
    virtual PublicPtr recover(const parachain::crypto::HashType& _hash, bytesConstRef _signatureData) const = 0;
    virtual std::pair<bool, bytes> recoverAddress(
        parachain::crypto::Hash& _hashImpl, const parachain::crypto::HashType& _hash, bytesConstRef _signatureData) const
    {
        auto pubKey = recover(_hash, _signatureData);
        if (!pubKey)
        {
            return std::make_pair(false, bytes());
        }
        auto address = calculateAddress(_hashImpl, pubKey);
        // Convert FixedBytes<20> to bytes
        const auto& addrData = address.data();
                std::vector<unsigned char> addrBytes(addrData.end() - 20, addrData.end());
        return {true, addrBytes};
    }

    // generateKeyPair generates keyPair
    virtual std::unique_ptr<KeyPairInterface> generateKeyPair() const = 0;

    // recoverAddress recovers address from a signature(for precompiled)
    virtual std::pair<bool, bytes> recoverAddress(parachain::crypto::Hash::Ptr _hashImpl, bytesConstRef _in) const = 0;
    virtual std::unique_ptr<KeyPairInterface> createKeyPair(
        SecretPtr _secretKey) const = 0;  // 修改为SecretPtr
};
}  // namespace bcos::crypto