#ifndef NEUBLOCKCHAIN_DB_CONNECTION_H
#define NEUBLOCKCHAIN_DB_CONNECTION_H

// 更新包含路径，使用 ../../common/concurrent_queue 而不是 ../../common/common/concurrent_queue
#include "../../utilities/concurrent_queue/blocking_concurrent_queue.hpp"
#include <functional>
#include <string>

#define CONN_POOL_INIT_SIZE 50

// Do not forward declare pqxx types - causes conflict with pqxx library
// Users of this header should include pqxx headers before this header if needed

// Do not forward declare pqxx types - causes conflict with pqxx library
// Users of this header should include pqxx headers before this header if needed

// Opaque pointer to avoid exposing pqxx types
using PqxxConnectionPtr = void*;
using PqxxResultPtr = void*;

class DBConnection {
protected:
    DBConnection();
    virtual ~DBConnection();

    bool executeTransaction(const std::string& sql, PqxxResultPtr result = nullptr);
    bool executeTransaction(const std::function<std::string(PqxxConnectionPtr)>& callback, PqxxResultPtr result = nullptr);

private:
    static PqxxConnectionPtr createConnection();
    PqxxConnectionPtr getConnection();
    void restoreConnection(PqxxConnectionPtr);

private:
    moodycamel::BlockingConcurrentQueue<PqxxConnectionPtr> connPool;
};


#endif //NEUBLOCKCHAIN_DB_CONNECTION_H