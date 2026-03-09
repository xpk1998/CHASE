#include "shard.h"

namespace scheduling {

Shard::Shard(uint32_t shard_id) 
    : shard_id_(shard_id), total_load_(0), sequences_scheduled_(false),
      concurrency_width_(0), execution_depth_(0) {
}

} // namespace scheduling