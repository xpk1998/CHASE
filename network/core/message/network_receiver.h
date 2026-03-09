#ifndef NEUBLOCKCHAIN_APTOSBFT_NETWORK_RECEIVER_H
#define NEUBLOCKCHAIN_APTOSBFT_NETWORK_RECEIVER_H

#include "proposal_msg.h"
#include "vote_msg.h"
#include "sync_info.h"
// 更新包含路径，使用 ../../../../utilities/concurrent_queue 而不是 ../../common/concurrent_queue
#include "../../../utilities/concurrent_queue/blocking_mpmc_queue.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace aptosbft {

// Forward declarations
class RoundManager;

// NetworkReceiver handles receiving messages from other validators
class NetworkReceiver {
private:
    std::string local_ip_;
    std::vector<std::string> validator_ips_;
    RoundManager* round_manager_;
    
    // Message queues
    BlockingMPMCQueue<std::string> proposal_queue_;
    BlockingMPMCQueue<std::string> vote_queue_;
    BlockingMPMCQueue<std::string> sync_queue_;
    
    // Threading
    std::atomic<bool> running_;
    std::thread proposal_thread_;
    std::thread vote_thread_;
    std::thread sync_thread_;
    
public:
    NetworkReceiver(const std::string& local_ip, const std::vector<std::string>& validator_ips, 
                   RoundManager* round_manager);
    ~NetworkReceiver();
    
    // Delete copy constructor and assignment operator
    NetworkReceiver(const NetworkReceiver&) = delete;
    NetworkReceiver& operator=(const NetworkReceiver&) = delete;
    
    // Initialization
    void start();
    void stop();
    
    // Message receiving methods
    void receiveProposal(const std::string& serialized_proposal);
    void receiveVote(const std::string& serialized_vote);
    void receiveSyncInfo(const std::string& serialized_sync_info);
    void receiveTimeout(uint64_t round);
    void receiveNewRound(uint64_t round, const std::string& high_qc);
    
    // Background threads
    void proposalReceivingThread();
    void voteReceivingThread();
    void syncReceivingThread();
    
    // Utility methods
    ProposalMsg deserializeProposal(const std::string& data) const;
    VoteMsg deserializeVote(const std::string& data) const;
    SyncInfo deserializeSyncInfo(const std::string& data) const;
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_NETWORK_RECEIVER_H