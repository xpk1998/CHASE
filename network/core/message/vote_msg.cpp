//
// VoteMsg implementation for AptosBFT consensus
//

#include "vote_msg.h"
#include <sstream>

namespace aptosbft {

VoteMsg::VoteMsg() {}

VoteMsg::VoteMsg(const Vote& vote, const SyncInfo& sync_info) 
    : vote_(vote), sync_info_(sync_info) {}

bool VoteMsg::verify(const std::string& sender, const std::vector<std::string>& validator_ips) const {
    // Check that the vote author matches the sender
    if (vote_.author() != sender) {
        return false;
    }
    
    // Check that the vote epoch matches the sync info epoch
    if (vote_.epoch() != sync_info_.epoch()) {
        return false;
    }
    
    // Check that the vote round is higher than the sync info highest round
    // Note: We need to implement methods to get the vote round and sync info highest round
    // Placeholder implementation
    // if (vote_.voteData().proposed().round() <= sync_info_.highestRound()) {
    //     return false;
    // }
    
    // If there is a timeout, check that the timeout hqc is less than or equal to the sync info hqc
    // Placeholder implementation
    // if (vote_.hasTimeout() && vote_.timeoutHqcRound() > sync_info_.highestCertifiedRound()) {
    //     return false;
    // }
    
    // Verify the vote itself
    SyncInfo syncInfo; // Create a temporary SyncInfo object
    return vote_.verify(syncInfo);
}

std::string VoteMsg::serialize() const {
    // Placeholder implementation
    std::ostringstream oss;
    oss << "VoteMsg{author:" << vote_.author() << ",epoch:" << vote_.epoch() 
        << ",round:" << "0" << "}"; // Placeholder for round
    return oss.str();
}

bool VoteMsg::operator==(const VoteMsg& other) const {
    return vote_ == other.vote_ &&
           sync_info_ == other.sync_info_;
}

bool VoteMsg::operator!=(const VoteMsg& other) const {
    return !(*this == other);
}

} // namespace aptosbft