#ifndef NEUBLOCKCHAIN_APTOSBFT_LEDGER_INFO_H
#define NEUBLOCKCHAIN_APTOSBFT_LEDGER_INFO_H

#include "block.h"
#include <string>

namespace aptosbft {

// LedgerInfo represents information about the ledger at a specific point in time
class LedgerInfo {
private:
    BlockInfo commit_info_;
    std::string consensus_data_hash_;

public:
    LedgerInfo();
    LedgerInfo(const BlockInfo& commit_info, const std::string& consensus_data_hash);
    
    // Factory method for dummy LedgerInfo
    static LedgerInfo dummy();
    
    // Getters
    const BlockInfo& commitInfo() const { return commit_info_; }
    const std::string& consensusDataHash() const { return consensus_data_hash_; }
    
    // Setters
    void setConsensusDataHash(const std::string& hash) { consensus_data_hash_ = hash; }
    
    // Utility methods
    epoch_size_t epoch() const;
    bool endsEpoch() const;
    bool validateSignature() const;
    
    // Comparison operators
    bool operator==(const LedgerInfo& other) const;
    bool operator!=(const LedgerInfo& other) const;
};

} // namespace aptosbft

#endif // NEUBLOCKCHAIN_APTOSBFT_LEDGER_INFO_H