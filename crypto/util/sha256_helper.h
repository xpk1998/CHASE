#pragma once
#include "../hash/Sha256.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "../../utilities/FixedBytes.h"

namespace parachain::crypto {

class SHA256Helper {
public:
    void hash(const std::string& input, std::string* output) {
        // Convert input string to bytes
        bcos::bytes inputBytes(input.begin(), input.end());
        bcos::bytesConstRef inputRef(inputBytes);
        
        // Compute hash
        HashType hash = sha256Hash(inputRef);
        
        // Convert hash to string using hex() method from FixedBytes
        *output = hash.hex();
    }
};

} // namespace parachain::crypto
