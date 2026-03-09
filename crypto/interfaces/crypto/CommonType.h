#pragma once
#include "../../../network/gatew../../utilities/BoostLog.h"
#include "../../../network/gatew../../utilities/FixedBytes.h"
#include <vector>
#include <memory>

#define CRYPTO_LOG(LEVEL) BCOS_LOG(LEVEL) << LOG_BADGE("CRYPTO")
namespace parachain::crypto
{
using HashType = bcos::h256;
using HashList = std::vector<HashType>;
using HashListPtr = std::shared_ptr<HashList>;
using Address = bcos::h160;

}  // namespace parachain::crypto