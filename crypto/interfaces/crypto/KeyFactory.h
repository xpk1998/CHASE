#pragma once
#include "KeyInterface.h"
namespace bcos::crypto
{
class KeyFactory
{
public:
    using Ptr = std::shared_ptr<KeyFactory>;
    KeyFactory() = default;
    virtual ~KeyFactory() = default;

    virtual std::shared_ptr<KeyInterface> createKey(bcos::bytesConstRef _keyData) = 0;
    virtual std::shared_ptr<KeyInterface> createKey(bcos::bytes const& _keyData) = 0;
};
}  // namespace bcos::crypto