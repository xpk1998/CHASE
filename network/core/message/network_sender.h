//
// NetworkSender definition for AptosBFT consensus
//

#ifndef NEUBLOCKCHAIN_APTOSBFT_NETWORK_SENDER_H
#define NEUBLOCKCHAIN_APTOSBFT_NETWORK_SENDER_H

#include "proposal_msg.h"
#include "vote_msg.h"
#include "sync_info.h"
#include <string>
#include <vector>
#include <memory>

namespace aptosbft {

// NetworkSender handles sending messages to other validators
class NetworkSender {
private:
    std::string local_ip_;
    std::vector<std::string> validator_ips_;
    
public:
    NetworkSender(const std::string& local_ip, const std::vector<std::string>& validator_ips);
    
    // Message sending methods
    void sendProposal(const ProposalMsg& proposal);
    void sendVote(const VoteMsg& vote);
    void sendSyncInfo(const SyncInfo& sync_info);
    void sendTimeout(uint64_t round);
    void sendNewRound(uint64_t round, const std::string& high_qc);
    
    // Broadcast methods
    void broadcastProposal(const ProposalMsg& proposal);
    void broadcastVote(const VoteMsg& vote);
    void broadcastSyncInfo(const SyncInfo& sync_info);
    
    // Utility methods
    void sendMessage(const std::string& target_ip, const std::string& message);
    std::vector<std::string> serializeProposal(const ProposalMsg& proposal) const;
    std::vector<std::string> serializeVote(const VoteMsg& vote) const;
    std::vector<std::string> serializeSyncInfo(const SyncInfo& sync_info) const;
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_NETWORK_SENDER_H