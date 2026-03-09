#ifndef NEUBLOCKCHAIN_APTOSBFT_PROPOSAL_MSG_H
#define NEUBLOCKCHAIN_APTOSBFT_PROPOSAL_MSG_H

#include "../core/block.h"
#include "../core/sync_info.h"
#include <string>

namespace aptosbft {

// ProposalMsg contains the required information for the proposer election protocol to make its choice
class ProposalMsg {
private:
    Block proposal_;
    SyncInfo sync_info_;

public:
    ProposalMsg();
    ProposalMsg(const Block& proposal, const SyncInfo& sync_info);
    
    // Getters
    const Block& proposal() const { return proposal_; }
    Block takeProposal() { return proposal_; }
    const SyncInfo& syncInfo() const { return sync_info_; }
    std::string proposer() const;
    epoch_size_t epoch() const;
    
    // Utility methods
    bool verifyWellFormed() const;
    bool verify(const std::string& sender, const std::vector<std::string>& validator_ips) const;
    
    // Serialization methods
    std::string serialize() const;
    bool deserialize(const std::string& data);
    
    // Comparison operators
    bool operator==(const ProposalMsg& other) const;
    bool operator!=(const ProposalMsg& other) const;
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_PROPOSAL_MSG_H