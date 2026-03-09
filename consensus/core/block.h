#ifndef NEUBLOCKCHAIN_APTOSBFT_BLOCK_H
#define NEUBLOCKCHAIN_APTOSBFT_BLOCK_H

#include "utilities/types/aria_types.h"
#include "../../executor/transaction/transaction.h"  // Add this line to include Transaction class
#include <string>
#include <vector>
#include <cstdint>

namespace aptosbft {

// BlockInfo represents metadata about a block
struct BlockInfo {
    epoch_size_t epoch;
    uint64_t round;
    std::string id;
    std::string parent_id;
    uint64_t timestamp_usecs;
    uint64_t version;

    BlockInfo();
    BlockInfo(epoch_size_t epoch, uint64_t round, const std::string& id, 
              const std::string& parent_id, uint64_t timestamp_usecs, uint64_t version);
    
    bool operator==(const BlockInfo& other) const {
        return epoch == other.epoch &&
               round == other.round &&
               id == other.id &&
               parent_id == other.parent_id &&
               timestamp_usecs == other.timestamp_usecs &&
               version == other.version;
    }
    
    bool operator!=(const BlockInfo& other) const {
        return !(*this == other);
    }
};

// BlockData represents the data contained in a block
struct BlockData {
    std::vector<Transaction*> transactions;  // Now Transaction is properly defined
    std::string payload;
    std::string payload_hash;

    BlockData();
    BlockData(const std::vector<Transaction*>& transactions, const std::string& payload);
    
    bool operator==(const BlockData& other) const {
        // For now, we'll compare by payload hash since comparing vectors of pointers is complex
        return payload == other.payload &&
               payload_hash == other.payload_hash;
    }
    
    bool operator!=(const BlockData& other) const {
        return !(*this == other);
    }
};

// Block represents a consensus block
class Block {
private:
    BlockInfo info_;
    BlockData data_;
    std::string author_;
    std::string signature_;

public:
    Block();
    Block(const BlockInfo& info, const BlockData& data, const std::string& author);
    
    // Getters
    const BlockInfo& info() const { return info_; }
    const BlockData& data() const { return data_; }
    const std::string& author() const { return author_; }
    const std::string& signature() const { return signature_; }
    
    // Setters
    void setInfo(const BlockInfo& info) { info_ = info; }
    void setData(const BlockData& data) { data_ = data; }
    void setAuthor(const std::string& author) { author_ = author; }
    void setSignature(const std::string& signature) { signature_ = signature; }
    
    // Utility methods
    std::string id() const;
    epoch_size_t epoch() const { return info_.epoch; }
    uint64_t round() const { return info_.round; }
    const std::string& parentId() const { return info_.parent_id; }
    uint64_t timestampUsecs() const { return info_.timestamp_usecs; }
    
    bool isNilBlock() const;
    bool verifyWellFormed() const;
    
    // Comparison operators
    bool operator==(const Block& other) const {
        return info_ == other.info_ &&
               data_ == other.data_ &&
               author_ == other.author_ &&
               signature_ == other.signature_;
    }
    
    bool operator!=(const Block& other) const {
        return !(*this == other);
    }
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_BLOCK_H