#include "snapshot.h"

namespace scheduling {

Snapshot::Snapshot() = default;

Snapshot::~Snapshot() = default;

bool Snapshot::read(const std::string& table, const std::string& key, std::string& value) {
    auto table_it = data_.find(table);
    if (table_it != data_.end()) {
        auto key_it = table_it->second.find(key);
        if (key_it != table_it->second.end()) {
            value = key_it->second;
            return true;
        }
    }
    return false;
}

void Snapshot::write(const std::string& table, const std::string& key, const std::string& value) {
    data_[table][key] = value;
}

const std::unordered_map<std::string, RwSet>& Snapshot::getRwSets() const {
    return rw_sets_;
}

} // namespace scheduling