#pragma once

#include <string>
#include <unordered_map>
#include "../../scheduler/serial/rw_set.h"

namespace scheduling {

class Snapshot {
public:
    Snapshot();
    ~Snapshot();

    // Read operation
    bool read(const std::string& table, const std::string& key, std::string& value);
    
    // Write operation
    void write(const std::string& table, const std::string& key, const std::string& value);
    
    // Get all read-write sets
    const std::unordered_map<std::string, RwSet>& getRwSets() const;

private:
    std::unordered_map<std::string, RwSet> rw_sets_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data_;
};

} // namespace scheduling