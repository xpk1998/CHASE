//
// Created for New Scheduling Scheme Integration
// KDG (Key Dependency Graph) Node structures
//

#ifndef NEUBLOCKCHAIN_KDG_NODE_H
#define NEUBLOCKCHAIN_KDG_NODE_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <set>
#include "../../scheduler/serial/rw_set.h"

namespace scheduling {

// Forward declarations
class TransactionNode;
class Unit;
class Address;

// Unit type: Read or Write
enum class UnitType {
    READ,
    WRITE
};

// Unit: A read or write operation on a specific key
// Transaction Unit structure
class Unit {
public:
    Unit(std::shared_ptr<TransactionNode> tx, 
         UnitType type, 
         const std::string& address,
         bool co_located = false)
        : tx_(tx),
          unit_type_(type),
          address_(address),
          wr_dependencies_(0),
          co_located_(co_located) {}
    
    // Getters
    UnitType unitType() const { return unit_type_; }
    const std::string& address() const { return address_; }
    uint32_t degree() const { return wr_dependencies_.load(); }
    bool coLocated() const { return co_located_; }
    std::shared_ptr<TransactionNode> transaction() const { return tx_; }
    
    // Add a WR dependency
    void addDependency() {
        wr_dependencies_.fetch_add(1);
    }
    
    // Check if transaction is sorted
    bool isSorted() const;
    
    // Get transaction sequence
    uint32_t sequence() const;
    
    // Set transaction sequence
    void setSequence(uint32_t seq);
    
    // Abort the transaction
    void abortTransaction();
    
private:
    std::shared_ptr<TransactionNode> tx_;
    UnitType unit_type_;
    std::string address_;                    // Key address (table:key)
    std::atomic<uint32_t> wr_dependencies_;  // Number of WR dependencies
    bool co_located_;                        // True if read and write on same address
};

// ReadUnits: Collection of read units for an address
class ReadUnits {
public:
    ReadUnits() : max_seq_(0) {}
    
    void push(std::shared_ptr<Unit> unit) {
        units_.push_back(unit);
    }
    
    // Sort read units
    void sort();
    
    uint32_t maxSeq() const { return max_seq_; }
    
    uint32_t incrementAndGetMaxSeq() {
        return ++max_seq_;
    }
    
    const std::vector<std::shared_ptr<Unit>>& units() const {
        return units_;
    }
    
private:
    std::vector<std::shared_ptr<Unit>> units_;
    uint32_t max_seq_;
};

// WriteUnits: Collection of write units for an address
class WriteUnits {
public:
    WriteUnits() : max_seq_(0), first_updater_flag_(false) {}
    
    void push(std::shared_ptr<Unit> unit) {
        units_.push_back(unit);
    }
    
    // Sort write units
    void sort(ReadUnits& read_units);
    
    uint32_t maxSeq() const { return max_seq_; }
    
    bool firstUpdaterFlag() const { return first_updater_flag_; }
    void setFirstUpdaterFlag(bool flag) { first_updater_flag_ = flag; }
    
    const std::vector<std::shared_ptr<Unit>>& units() const {
        return units_;
    }
    
private:
    std::vector<std::shared_ptr<Unit>> units_;
    uint32_t max_seq_;
    bool first_updater_flag_;  // First-updater-wins flag
};

// Address: Represents a key in the KDG
// Address structure with read/write units
class Address {
public:
    explicit Address(const std::string& addr) 
        : address_(addr),
          in_degree_(0),
          out_degree_(0),
          first_updater_flag_(false) {}
    
    const std::string& address() const { return address_; }
    uint32_t inDegree() const { return in_degree_; }
    uint32_t outDegree() const { return out_degree_; }
    bool firstUpdaterFlag() const { return first_updater_flag_; }
    
    // Add a unit to this address
    void addUnit(std::shared_ptr<Unit> unit);
    
    // Sort read units
    void sortReadUnits() {
        read_units_.sort();
    }
    
    // Sort write units
    void sortWriteUnits() {
        write_units_.sort(read_units_);
    }
    
    // Get read/write units
    ReadUnits& readUnits() { return read_units_; }
    WriteUnits& writeUnits() { return write_units_; }
    const ReadUnits& readUnits() const { return read_units_; }
    const WriteUnits& writeUnits() const { return write_units_; }
    
    // Merge another address into this one (for parallel construction)
    void merge(Address&& other);
    
private:
    std::string address_;
    uint32_t in_degree_;
    uint32_t out_degree_;
    ReadUnits read_units_;
    WriteUnits write_units_;
    bool first_updater_flag_;
};

// TransactionNode: KDG transaction node (renamed to avoid conflict with global Transaction class)
// Represents a transaction in the KDG
// Transaction structure
class TransactionNode {
public:
    TransactionNode(uint64_t id, const RwSet& rw_set, void* raw_tx, uint64_t gas = 0)
        : tx_id_(id),
          sequence_(0),
          aborted_(false),
          rw_set_(rw_set),
          raw_transaction_(raw_tx),
          gas_used_(gas) {}
    
    uint64_t id() const { return tx_id_; }
    uint32_t sequence() const { return sequence_.load(); }
    bool aborted() const { return aborted_.load(); }
    void* rawTransaction() const { return raw_transaction_; }
    uint64_t gasUsed() const { return gas_used_; }  // Get actual Gas consumed
    
    const RwSet& rwSet() const { return rw_set_; }
    
    std::set<std::string> readKeys() const {
        return rw_set_.getReadKeys();
    }
    
    std::set<std::string> writeKeys() const {
        return rw_set_.getWriteKeys();
    }
    
    // Set sequence number
    void setSequence(uint32_t seq) {
        sequence_.store(seq);
    }
    
    // Mark as aborted
    void abort() {
        aborted_.store(true);
    }
    
    // Reset for reordering
    void reset() {
        sequence_.store(0);
        aborted_.store(false);
    }
    
    // Check if sorted
    bool isSorted() const {
        return sequence_.load() != 0 || aborted_.load();
    }
    
    // Check if reorderable (write-only transactions)
    bool isReorderable() const {
        return isWriteOnly() && writeKeys().size() > 1;
    }
    
    // Add write units
    void addWriteUnit(std::shared_ptr<Unit> unit) {
        write_units_.push_back(unit);
    }
    
    // Reserve capacity for write units (optimization)
    void reserveWriteUnits(size_t capacity) {
        write_units_.reserve(capacity);
    }
    
    void clearWriteUnits() {
        write_units_.clear();
    }
    
    const std::vector<std::shared_ptr<Unit>>& writeUnits() const {
        return write_units_;
    }
    
    // Check if transaction has no write dependencies
    bool hasNoWriteDependencies() const {
        // A transaction has no write dependencies if it has no write units
        // or all its write units have zero dependencies
        for (const auto& unit : write_units_) {
            if (unit && unit->degree() > 0) {
                return false;
            }
        }
        return true;
    }
    
private:
    bool isWriteOnly() const {
        return rw_set_.reads().empty() && !rw_set_.writes().empty();
    }
    
    uint64_t tx_id_;
    std::atomic<uint32_t> sequence_;
    std::atomic<bool> aborted_;
    RwSet rw_set_;
    void* raw_transaction_;  // Pointer to original Transaction object
    std::vector<std::shared_ptr<Unit>> write_units_;
    uint64_t gas_used_;  // Actual Gas consumed (from simulation)
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_KDG_NODE_H
