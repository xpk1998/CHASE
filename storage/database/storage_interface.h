#pragma once

#include "entry.h"
#include "table.h"
#include "ranges.h"
#include "../core/database/error.h"
#include "../../utilities/types/two_pc_params.h"
#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include <vector>
#include <memory>

namespace storage {

// Forward declarations
class StorageInterface;
class TraverseStorageInterface;
class MergeableStorageInterface;
class TransactionalStorageInterface;

// StorageInterface class
class StorageInterface
{
public:
    static constexpr const char SYS_TABLES[] = "s_tables";
    static constexpr const char SYS_TABLE_VALUE_FIELDS[] = "key_field,value_fields";

    using Ptr = std::shared_ptr<StorageInterface>;
    using Error = parachain::common::Error;
    using ErrorPtr = std::shared_ptr<Error>;
    using ErrorUniquePtr = std::unique_ptr<Error>;

    virtual ~StorageInterface() = default;

    virtual void asyncGetPrimaryKeys(std::string_view table,
        const std::optional<Condition const>& _condition,
        std::function<void(ErrorUniquePtr, std::vector<std::string>)> _callback) = 0;

    virtual void asyncGetRow(std::string_view table, std::string_view _key,
        std::function<void(ErrorUniquePtr, std::optional<Entry>)> _callback) = 0;

    virtual void asyncGetRows(std::string_view table,
        RANGES::any_view<std::string_view, RANGES::category::random_access>
            keys,
        std::function<void(ErrorUniquePtr, std::vector<std::optional<Entry>>)> _callback) = 0;

    virtual void asyncSetRow(std::string_view table, std::string_view key, Entry entry,
        std::function<void(ErrorUniquePtr)> callback) = 0;

    virtual void asyncCreateTable(std::string _tableName, std::string _valueFields,
        std::function<void(ErrorUniquePtr, std::optional<Table>)> callback) = 0;

    virtual void asyncOpenTable(std::string_view tableName,
        std::function<void(ErrorUniquePtr, std::optional<Table>)> callback) = 0;

    virtual void asyncGetTableInfo(std::string_view tableName,
        std::function<void(ErrorUniquePtr, TableInfo::ConstPtr)> callback) = 0;
        
    virtual ErrorPtr setRows(std::string_view tableName,
        RANGES::any_view<std::string_view, RANGES::category::random_access>
            keys,
        RANGES::any_view<std::string_view, RANGES::category::random_access>
            values) = 0;
    
    virtual ErrorPtr deleteRows(
        std::string_view, const std::variant<const std::vector<std::string_view>, 
                              const std::vector<std::string>>&) = 0;

    virtual std::pair<ErrorUniquePtr, std::optional<Entry>> getRow(
        const std::string_view& table, const std::string_view& _key)
    {
        std::pair<ErrorUniquePtr, std::optional<Entry>> result;
        asyncGetRow(table, _key, [&result](ErrorUniquePtr _error, std::optional<Entry> _entry) {
            result.first = std::move(_error);
            result.second = std::move(_entry);
        });
        return result;
    };
    
    virtual void stop() = 0;
};

// TraverseStorageInterface class
class TraverseStorageInterface : public virtual StorageInterface
{
public:
    using Ptr = std::shared_ptr<TraverseStorageInterface>;
    using ConstPtr = std::shared_ptr<TraverseStorageInterface const>;

    virtual void parallelTraverse(bool onlyDirty,
        std::function<bool(
            const std::string_view& table, const std::string_view& key, Entry const& entry)>
            callback) const = 0;
};

// MergeableStorageInterface class
class MergeableStorageInterface : public virtual StorageInterface
{
public:
    using Ptr = std::shared_ptr<MergeableStorageInterface>;

    virtual void merge(bool onlyDirty, const TraverseStorageInterface& source) = 0;
};

// TransactionalStorageInterface class
class TransactionalStorageInterface : public virtual StorageInterface
{
public:
    using Ptr = std::shared_ptr<TransactionalStorageInterface>;
    using ConstPtr = std::shared_ptr<const TransactionalStorageInterface>;
    using Error = parachain::common::Error;
    using ErrorPtr = std::shared_ptr<Error>;
    using ErrorUniquePtr = std::unique_ptr<Error>;

    ~TransactionalStorageInterface() override = default;

    virtual void asyncPrepare(const bcos::protocol::TwoPCParams& params,
        const TraverseStorageInterface& storage,
        std::function<void(ErrorPtr, uint64_t, const std::string&)> callback) = 0;

    virtual void asyncCommit(const bcos::protocol::TwoPCParams& params,
        std::function<void(ErrorPtr, uint64_t)> callback) = 0;

    virtual void asyncRollback(
        const bcos::protocol::TwoPCParams& params, std::function<void(ErrorPtr)> callback) = 0;
};

}  // namespace storage