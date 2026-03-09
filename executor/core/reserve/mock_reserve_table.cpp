//
// Created for ParaChain Mock Reserve Table Implementation
//

#include "mock_reserve_table.h"
#include "../../build/proto_gen/kv_rwset.pb.h"

bool MockReserveTable::reserveRWSet(const KVRWSet* kvRWSet, tid_size_t transactionID) { //reentry
    for(const auto& read : kvRWSet->reads()) {
        auto& table = readTableList[read.table()];
        // Check if key exists using contains method
        if (!table.contains(read.key())) {
            // Use emplace to construct atomic value in place
            table[read.key()].store(0); // Initialize with 0
        }
        auto& reg = table[read.key()];
        auto old_rts = reg.load(std::memory_order_relaxed);
        do {
            if (old_rts <= transactionID && old_rts != 0) {
                break;
            }
        } while (!reg.compare_exchange_weak(old_rts, transactionID, std::memory_order_release, std::memory_order_relaxed));
    }

    for(const auto& write : kvRWSet->writes()) {
        auto& table = writeTableList[write.table()];
        // Check if key exists using contains method
        if (!table.contains(write.key())) {
            // Use emplace to construct atomic value in place
            table[write.key()].store(0); // Initialize with 0
        }
        auto& reg = table[write.key()];
        auto old_wts = reg.load(std::memory_order_relaxed);
        do {
            if (old_wts <= transactionID && old_wts != 0) {
                break;
            }
        } while (!reg.compare_exchange_weak(old_wts, transactionID, std::memory_order_release, std::memory_order_relaxed));
    }
    return true;
}

TransactionDependency MockReserveTable::dependencyAnalysis(const KVRWSet* kvRWSet, tid_size_t transactionID) {   //reentry
    TransactionDependency trDependency;
    for(const auto& write : kvRWSet->writes()) {
        auto& table = writeTableList[write.table()];
        if (table.contains(write.key())) {
            auto& reg = table[write.key()];
            auto old_wts = reg.load(std::memory_order_relaxed);

            if (old_wts < transactionID && old_wts != 0) {  // waw dependency
                trDependency.waw = true;
                break;
            }
        }
    }

    for(const auto& write : kvRWSet->writes()) {
        auto& table = readTableList[write.table()];
        if (table.contains(write.key())) {
            auto& reg = table[write.key()];
            auto old_rts = reg.load(std::memory_order_relaxed);

            if (old_rts < transactionID && old_rts != 0) {  // war dependency
                trDependency.war = true;
                break;
            }
        }
    }

    for(const auto& read : kvRWSet->reads()) {
        auto& table = writeTableList[read.table()];
        if (table.contains(read.key())) {
            auto& reg = table[read.key()];
            auto old_wts = reg.load(std::memory_order_relaxed);

            if (old_wts < transactionID && old_wts != 0) {  // raw dependency
                trDependency.raw = true;
                break;
            }
        }
    }
    return trDependency;
}