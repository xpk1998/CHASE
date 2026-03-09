//
// Created for ParaChain Reserve Table Implementation
//

#include "reserve_table.h"
#include "mock_reserve_table.h"
#include "mock_mvcc_reserve_table.h"
#include "glog/logging.h"
#include <mutex>

// ReserveTableProvider实现
ReserveTableProvider* ReserveTableProvider::getHandle() {
    static ReserveTableProvider* reserveTableProvider = nullptr;
    static std::mutex mutex;
    if(reserveTableProvider == nullptr) {  // performance
        std::unique_lock<std::mutex> lock(mutex);
        if(reserveTableProvider == nullptr) {  // prevent for reentry
            reserveTableProvider = new ReserveTableProvider;
        }
    }
    return reserveTableProvider;
}

ReserveTable *ReserveTableProvider::getReserveTable(epoch_size_t epoch) {
    // 默认使用HashMap版本的ReserveTable
    return getHashMapReserveTable(epoch);
}

ReserveTable *ReserveTableProvider::getHashMapReserveTable(epoch_size_t epoch) {
    if(this->reserveTable == nullptr || epoch != this->reserveTable->getEpoch()) {
        static std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        if(this->reserveTable == nullptr || epoch != this->reserveTable->getEpoch())
            this->reserveTable = std::make_unique<MockReserveTable>(epoch);
    }
    return this->reserveTable.get();
}

ReserveTable *ReserveTableProvider::getMVCCReserveTable(epoch_size_t epoch) {
    if(this->mvccReserveTable == nullptr || epoch != this->mvccReserveTable->getEpoch()) {
        static std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        if(this->mvccReserveTable == nullptr || epoch != this->mvccReserveTable->getEpoch())
            this->mvccReserveTable = std::make_unique<MockMVCCReserveTable>(epoch);
    }
    return this->mvccReserveTable.get();
}