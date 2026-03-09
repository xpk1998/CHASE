//
// Created by peng on 2021/1/19.
//

#include "database.h"
#include "../../database/impl/database_impl.h"
#include "glog/logging.h"

Database* Database::getDBInstance() {
    static Database* instance = dynamic_cast<Database*>(DatabaseImpl::createInstance());
    return instance;
}

// 添加一些辅助函数来增强数据库功能
namespace {
    // 数据库初始化标记
    static bool database_initialized = false;
    
    // 数据库配置参数
    static struct DBConfig {
        size_t cache_size = 1024 * 1024;  // 1MB缓存
        bool enable_compression = true;
        int max_open_files = 1000;
    } db_config;
}

// 数据库初始化函数
bool Database::initializeDatabase(const DatabaseConfig& config) {
    if (database_initialized) {
        LOG(WARNING) << "Database already initialized";
        return true;
    }
    
    db_config.cache_size = config.cache_size;
    db_config.enable_compression = config.enable_compression;
    db_config.max_open_files = config.max_open_files;
    database_initialized = true;
    
    LOG(INFO) << "Database initialized with cache_size=" << db_config.cache_size
              << ", compression=" << (db_config.enable_compression ? "enabled" : "disabled")
              << ", max_open_files=" << db_config.max_open_files;
    return true;
}

// 获取数据库配置
DatabaseConfig Database::getDatabaseConfig() {
    DatabaseConfig config;
    config.cache_size = db_config.cache_size;
    config.enable_compression = db_config.enable_compression;
    config.max_open_files = db_config.max_open_files;
    return config;
}

// 检查数据库是否已初始化
bool Database::isDatabaseInitialized() {
    return database_initialized;
}