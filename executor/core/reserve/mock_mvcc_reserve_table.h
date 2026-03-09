//
// Created for ParaChain Mock MVCC Reserve Table Implementation
//

#ifndef NEUBLOCKCHAIN_MOCK_MVCC_RESERVE_TABLE_H
#define NEUBLOCKCHAIN_MOCK_MVCC_RESERVE_TABLE_H

#include "reserve_table.h"
#include "../../../utilities/mvcc_hash_map.h"
#include <atomic>

class MockMVCCReserveTable: public ReserveTable {
public:
    [[deprecated("MVCC reserve table currently has bug and not recommend to use.")]]
    explicit MockMVCCReserveTable(epoch_size_t _epoch): ReserveTable(_epoch) {}
    bool reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) override;
    TransactionDependency dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) override;

private:
    MVCCHashMap<997, std::string, std::atomic<tid_size_t>> readTable;
    MVCCHashMap<997, std::string, std::atomic<tid_size_t>> writeTable;
};

#endif //NEUBLOCKCHAIN_MOCK_MVCC_RESERVE_TABLE_H