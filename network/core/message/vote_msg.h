//
// VoteMsg definition for AptosBFT consensus
//

#ifndef NEUBLOCKCHAIN_APTOSBFT_VOTE_MSG_H
#define NEUBLOCKCHAIN_APTOSBFT_VOTE_MSG_H

#include "../core/vote.h"
#include "../core/sync_info.h"
#include <string>

namespace aptosbft {

// VoteMsg is a message containing a vote
class VoteMsg {
private:
    Vote vote_;
    SyncInfo sync_info_;

public:
    VoteMsg();
    VoteMsg(const Vote& vote, const SyncInfo& sync_info);
    
    // Getters
    const Vote& vote() const { return vote_; }
    const SyncInfo& syncInfo() const { return sync_info_; }
    
    // Utility methods
    bool verify(const std::string& sender, const std::vector<std::string>& validator_ips) const;
    
    // Serialization methods
    std::string serialize() const;
    bool deserialize(const std::string& data);
    
    // Comparison operators
    bool operator==(const VoteMsg& other) const;
    bool operator!=(const VoteMsg& other) const;
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_VOTE_MSG_H