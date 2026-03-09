//
// Created by peng on 2021/1/19.
//

#ifndef NEUBLOCKCHAIN_DATABASE_H
#define NEUBLOCKCHAIN_DATABASE_H

#include <cstdint>
#include <string>
#include "../../../scheduler/serial/aria_types.h"
#include "../../database/versioned_db.h"
#include "glog/logging.h"

#define ARIA_SYS_TABLE "aria_sys_table"

class KVRWSet;
class DBStorage;

// 数据库配置结构体
struct DatabaseConfig {
    size_t cache_size = 1024 * 1024;  // 1MB缓存
    bool enable_compression = true;
    int max_open_files = 1000;
};

// Database类继承自blp::VersionedDB
class Database : public blp::VersionedDB {
public:
    static Database* getDBInstance();
    
    // 数据库初始化函数
    static bool initializeDatabase(const DatabaseConfig& config);
    
    // 获取数据库配置
    static DatabaseConfig getDatabaseConfig();
    
    // 检查数据库是否已初始化
    static bool isDatabaseInitialized();
    
    virtual ~Database() = default;

    virtual DBStorage* getStorage() = 0;
    virtual bool commitUpdate(tid_size_t tid) = 0;
    virtual void abortUpdate(tid_size_t tid) = 0;
    
    // 实现blp::VersionedDB接口
    virtual bool get(const std::string& table, const std::string& key, std::string& value) = 0;
    virtual void put(const std::string& table, const std::string& key, const std::string& value) = 0;
    virtual void updateWriteSet(const std::string& key, const std::string& value, const std::string& table) = 0;
    virtual void* selectDB(const std::string& key, const std::string& table) = 0;
};

#endif //NEUBLOCKCHAIN_DATABASE_H