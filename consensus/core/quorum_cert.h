#ifndef NEUBLOCKCHAIN_APTOSBFT_QUORUM_CERT_H
#define NEUBLOCKCHAIN_APTOSBFT_QUORUM_CERT_H

#include "vote_data.h"
#include "ledger_info.h"
#include <string>
#include <vector>

namespace aptosbft {

// QuorumCert is a proof that a quorum of validators have voted for a proposal
class QuorumCert {
private:
    VoteData vote_data_;
    LedgerInfo ledger_info_;

public:
    QuorumCert();
    QuorumCert(const VoteData& vote_data, const LedgerInfo& ledger_info);
    
    // Factory method for dummy QuorumCert
    static QuorumCert dummy();
    
    // Factory method for genesis certificate
    static QuorumCert certificateForGenesisFromLedgerInfo(const LedgerInfo& ledger_info, 
                                                         const std::string& genesis_id);
    
    // Getters
    const VoteData& voteData() const { return vote_data_; }
    const LedgerInfo& ledgerInfo() const { return ledger_info_; }
    const BlockInfo& certifiedBlock() const;
    const BlockInfo& parentBlock() const;
    const BlockInfo& commitInfo() const;
    
    // Utility methods
    bool verify(const std::vector<std::string>& validator_ips) const;
    bool endsEpoch() const;
    
    // Merge with executed state
    QuorumCert createMergedWithExecutedState(const LedgerInfo& executed_ledger_info) const;
    
    // Comparison operators
    bool operator==(const QuorumCert& other) const;
    bool operator!=(const QuorumCert& other) const;
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_QUORUM_CERT_H