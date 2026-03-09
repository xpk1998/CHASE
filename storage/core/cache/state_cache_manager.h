#pragma once

#include <string>
#include <memory>
#include "../common/protocol/block_header.h"

namespace parachain {
namespace blp {

// 状态缓存管理器
class StateCacheManager {
public:
    using Ptr = std::shared_ptr<StateCacheManager>;
    using UPtr = std::unique_ptr<StateCacheManager>;

    StateCacheManager() = default;
    virtual ~StateCacheManager() = default;

    // 为区块准备缓存
    virtual void prepareForBlock(protocol::BlockNumber block_number) {
        // 实现准备逻辑
    }

    // 提交区块
    virtual void commitBlock(protocol::BlockNumber block_number) {
        // 实现提交逻辑
    }

    // 获取根哈希
    virtual std::string getRootHash() const {
        return "mock_root_hash";  // 临时实现
    }

    // 其他缓存管理方法
    virtual void clear() {
        // 清空缓存
    }
};

} // namespace blp
} // namespace parachain