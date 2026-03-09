//
// Created for ParaChain Mock Reserve Table Implementation
//

#ifndef NEUBLOCKCHAIN_MOCK_RESERVE_TABLE_H
#define NEUBLOCKCHAIN_MOCK_RESERVE_TABLE_H

#include "reserve_table.h"
#include "../../../../utilities/hash_map.h"
#include <atomic>
#include <string>

class MockReserveTable: public ReserveTable {
public:
    explicit MockReserveTable(epoch_size_t _epoch):ReserveTable(_epoch) {}
    bool reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) override;
    TransactionDependency dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) override;

private:
    HashMap<std::string, HashMap<std::string, std::atomic<tid_size_t>>> readTableList;
    HashMap<std::string, HashMap<std::string, std::atomic<tid_size_t>>> writeTableList;
};

#endif //NEUBLOCKCHAIN_MOCK_RESERVE_TABLE_H