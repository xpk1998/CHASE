#pragma once
#include "KeyPairInterface.h"
#include "../../../network/gatew../../utilities/Common.h"
namespace bcos::crypto
{
class KeyPairFactory
{
public:
    using Ptr = std::shared_ptr<KeyPairFactory>;
    KeyPairFactory() = default;
    virtual ~KeyPairFactory() = default;
    virtual KeyPairInterface::UniquePtr createKeyPair(SecretPtr _secretKey) = 0;
    virtual KeyPairInterface::UniquePtr generateKeyPair() = 0;
};
}  // namespace bcos::crypto
