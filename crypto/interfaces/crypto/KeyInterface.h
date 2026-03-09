#pragma once
#include "CommonType.h"
#include "../../../network/gatew../../utilities/Common.h"
#include <memory>
#include <vector>
#include <string_view>

namespace bcos::crypto
{
class KeyInterface
{
public:
    using Ptr = std::shared_ptr<KeyInterface>;
    using ConstPtr = std::shared_ptr<const KeyInterface>;

    virtual ~KeyInterface() = default;

    virtual const uint8_t* data() const = 0;
    virtual size_t size() const = 0;
    virtual std::string_view hex() const = 0;
    virtual bool operator==(const KeyInterface& rhs) const = 0;
    virtual bool operator!=(const KeyInterface& rhs) const = 0;
    
    // 添加constData方法以兼容Hash.h中的调用
    virtual const char* constData() const { return (const char*)data(); }
};

using PublicPtr = std::shared_ptr<const KeyInterface>;
using PrivatePtr = std::shared_ptr<const KeyInterface>;
using KeyPair = std::pair<PrivatePtr, PublicPtr>;

class KeyFactory
{
public:
    using Ptr = std::shared_ptr<KeyFactory>;

    virtual ~KeyFactory() = default;

    virtual PublicPtr createPublic(const bcos::bytes& keyData) = 0;
    virtual PrivatePtr createPrivate(const bcos::bytes& keyData) = 0;
    virtual KeyPair generateKeyPair() = 0;
};
}  // namespace bcos::crypto