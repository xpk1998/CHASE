#pragma once

#include <memory>
#include <string>

namespace parachain {
namespace common {

class Error {
public:
    using Ptr = std::shared_ptr<Error>;
    using UniquePtr = std::unique_ptr<Error>;

    Error(int32_t _errorCode, const std::string& _errorMessage) 
        : m_errorCode(_errorCode), m_errorMessage(_errorMessage) {}
    
    Error(int32_t _errorCode, std::string&& _errorMessage) 
        : m_errorCode(_errorCode), m_errorMessage(std::move(_errorMessage)) {}

    int32_t errorCode() const { return m_errorCode; }
    const std::string& errorMessage() const { return m_errorMessage; }
    const std::string& message() const { return m_errorMessage; }

private:
    int32_t m_errorCode;
    std::string m_errorMessage;
};

} // namespace common
} // namespace parachain