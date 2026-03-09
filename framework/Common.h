#pragma once

// Common includes for ParaChain Framework
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <set>
#include <optional>
#include <variant>
#include <string_view>
#include <cstdint>
#include <atomic>
#include <thread>
#include <future>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// Common types
namespace parachain::common
{
    using bytes = std::vector<uint8_t>;
    using bytesConstRef = std::basic_string_view<uint8_t>;
    using bytesPointer = std::shared_ptr<bytes>;

    // Error type
    struct Error
    {
        int32_t errorCode = 0;
        std::string errorMessage;

        Error() = default;
        Error(int32_t code, const std::string& message) : errorCode(code), errorMessage(message) {}
    };
    using ErrorPtr = std::shared_ptr<Error>;
    using ErrorUniquePtr = std::unique_ptr<Error>;
}

// Common macros
#ifndef SAFE_LOG
#define SAFE_LOG(LEVEL) std::cout
#endif

// Common utilities
namespace parachain::common
{
    template<typename T>
    using Ptr = std::shared_ptr<T>;
    
    template<typename T>
    using ConstPtr = std::shared_ptr<const T>;
    
    template<typename T>
    using UniquePtr = std::unique_ptr<T>;
    
    template<typename T>
    using WeakPtr = std::weak_ptr<T>;
}