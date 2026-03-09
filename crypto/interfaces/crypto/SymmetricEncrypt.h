#pragma once
#include <memory>
#include <vector>
#include <string>

namespace bcos::crypto
{
class SymmetricEncrypt
{
public:
    using Ptr = std::shared_ptr<SymmetricEncrypt>;
    using ConstPtr = std::shared_ptr<const SymmetricEncrypt>;

    virtual ~SymmetricEncrypt() = default;

    virtual std::vector<uint8_t> encrypt(
        const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) const = 0;
    virtual std::vector<uint8_t> decrypt(
        const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) const = 0;
};
}  // namespace bcos::crypto