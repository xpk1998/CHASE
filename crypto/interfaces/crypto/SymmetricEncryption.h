#pragma once
#include "../../../network/gatew../../utilities/Common.h"
#include <memory>
#include <mutex>

namespace bcos
{
namespace crypto
{
class SymmetricEncryption
{
public:
    using Ptr = std::shared_ptr<SymmetricEncryption>;
    using UniquePtr = std::unique_ptr<SymmetricEncryption>;
    SymmetricEncryption() = default;
    virtual ~SymmetricEncryption() {}

    // symmetricEncrypt encrypts plain data with default ivData
    virtual bytesPointer symmetricEncrypt(const unsigned char* _plainData, size_t _plainDataSize,
        const unsigned char* _key, size_t _keySize) = 0;
    // symmetricDecrypt encrypts plain data with default ivData
    virtual bytesPointer symmetricDecrypt(const unsigned char* _cipherData, size_t _cipherDataSize,
        const unsigned char* _key, size_t _keySize) = 0;

    // symmetricEncrypt encrypts plain data with given ivData
    virtual bytesPointer symmetricEncrypt(const unsigned char* _plainData, size_t _plainDataSize,
        const unsigned char* _key, size_t _keySize, const unsigned char* _ivData,
        size_t _ivDataSize) = 0;
    // symmetricDecrypt encrypts plain data with given ivData
    virtual bytesPointer symmetricDecrypt(const unsigned char* _cipherData, size_t _cipherDataSize,
        const unsigned char* _key, size_t _keySize, const unsigned char* _ivData,
        size_t _ivDataSize) = 0;
};
}  // namespace crypto
}  // namespace bcos
