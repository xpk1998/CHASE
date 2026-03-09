#ifndef NEUBLOCKCHAIN_COORDINATOR_H
#define NEUBLOCKCHAIN_COORDINATOR_H

#include "utilities/types/aria_types.h"
#include "../../../executor/transaction/transaction_manager.h"
#include <memory>

namespace consensus {

class Coordinator {
public:
    Coordinator();
    virtual ~Coordinator();

    // Core coordination methods
    virtual bool coordinateRound() = 0;
    virtual void handleTransaction(const Transaction& tx) = 0;
    virtual void finalizeBlock() = 0;

protected:
    std::unique_ptr<TransactionManager> transaction_manager_;
    epoch_size_t current_epoch_;
    round_size_t current_round_;
};

} // namespace consensus

#endif //NEUBLOCKCHAIN_COORDINATOR_H