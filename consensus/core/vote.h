#ifndef NEUBLOCKCHAIN_APTOSBFT_VOTE_H
#define NEUBLOCKCHAIN_APTOSBFT_VOTE_H

#include "vote_data.h"
#include "ledger_info.h"
#include <string>

namespace aptosbft {

// Vote represents a validator's vote on a proposal
class Vote {
private:
    VoteData vote_data_;
    std::string author_;
    LedgerInfo ledger_info_;
    std::string signature_;

public:
    Vote();
    Vote(const VoteData& vote_data, const std::string& author, 
         const LedgerInfo& ledger_info, const std::string& signature);
    
    // Factory methods
    static Vote newVote(const VoteData& vote_data, const std::string& author,
                       LedgerInfo ledger_info_placeholder);
    
    // Getters
    const VoteData& voteData() const { return vote_data_; }
    const std::string& author() const { return author_; }
    const LedgerInfo& ledgerInfo() const { return ledger_info_; }
    const std::string& signature() const { return signature_; }
    
    // Setters
    void setSignature(const std::string& signature) { signature_ = signature; }
    
    // Utility methods
    epoch_size_t epoch() const;
    bool isTimeout() const;
    bool verify(const std::vector<std::string>& validator_ips) const;
    
    // Comparison operators
    bool operator==(const Vote& other) const;
    bool operator!=(const Vote& other) const;
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_VOTE_H