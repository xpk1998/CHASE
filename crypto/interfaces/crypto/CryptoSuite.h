#pragma once
#include "Hash.h"
#include "Signature.h"
#include "SymmetricEncrypt.h"

namespace bcos::crypto
{
class CryptoSuite
{
public:
    using Ptr = std::shared_ptr<CryptoSuite>;

    CryptoSuite() = default;
    virtual ~CryptoSuite() = default;

    virtual parachain::crypto::Hash::Ptr hash() const = 0;
    virtual SignatureCrypto::Ptr signature() const = 0;
    virtual SymmetricEncrypt::Ptr symmetricEncrypt() const = 0;

    virtual void setHash(parachain::crypto::Hash::Ptr hash) = 0;
    virtual void setSignature(SignatureCrypto::Ptr signature) = 0;
    virtual void setSymmetricEncrypt(SymmetricEncrypt::Ptr symmetricEncrypt) = 0;
};
}  // namespace bcos::crypto