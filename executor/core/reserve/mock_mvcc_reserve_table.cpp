//
// Created for ParaChain Mock MVCC Reserve Table Implementation
//

#include "mock_mvcc_reserve_table.h"
#include "transaction_dependency.h"
#include "../../build/proto_gen/kv_rwset.pb.h"

bool MockMVCCReserveTable::reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) { //reentry
    for(const auto& read : kvRWSet->reads()) {
        auto* reg = readTable.get_key_version(read.key(), read.table());
        if (reg == nullptr) {
            reg = &readTable.insert_key_version_holder(read.key(), read.table());
        }
        auto old_rts = reg->load(std::memory_order_relaxed);
        do {
            if (old_rts <= transactionID && old_rts != 0) {
                break;
            }
        } while (!reg->compare_exchange_weak(old_rts, transactionID, std::memory_order_release, std::memory_order_relaxed));
    }

    for(const auto& write : kvRWSet->writes()) {
        auto* reg = writeTable.get_key_version(write.key(), write.table());
        if (reg == nullptr) {
            reg = &writeTable.insert_key_version_holder(write.key(), write.table());
        }
        auto old_wts = reg->load(std::memory_order_relaxed);
        do {
            if (old_wts <= transactionID && old_wts != 0) {
                break;
            }
        } while (!reg->compare_exchange_weak(old_wts, transactionID, std::memory_order_release, std::memory_order_relaxed));
    }
    return true;
}

TransactionDependency MockMVCCReserveTable::dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) {   //reentry
    TransactionDependency trDependency;
    for(const auto& write : kvRWSet->writes()) {
        auto old_wts = writeTable.get_key_version(write.key(), write.table())->load(std::memory_order_relaxed);

        if (old_wts < transactionID && old_wts != 0) {  // waw dependency
            trDependency.waw = true;
            break;
        }
    }

    for(const auto& write : kvRWSet->writes()) {
        auto old_rts = readTable.get_key_version(write.key(), write.table())->load(std::memory_order_relaxed);

        if (old_rts < transactionID && old_rts != 0) {  // war dependency
            trDependency.war = true;
            break;
        }
    }

    for(const auto& read : kvRWSet->reads()) {
        auto old_wts = writeTable.get_key_version(read.key(), read.table())->load(std::memory_order_relaxed);

        if (old_wts < transactionID && old_wts != 0) {  // raw dependency
            trDependency.raw = true;
            break;
        }
    }
    return trDependency;
}