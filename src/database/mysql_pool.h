#pragma once
#include <mysql.h>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>
#include <chrono>
#include "../config.h"

// RAII transaction guard — disables autocommit, rolls back if not committed
class Transaction {
public:
    explicit Transaction(MYSQL* mysql) : mysql_(mysql), committed_(false) {
        if (mysql_) mysql_autocommit(mysql_, 0);
    }
    ~Transaction() {
        if (mysql_ && !committed_) {
            mysql_rollback(mysql_);
            mysql_autocommit(mysql_, 1);
        }
    }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&& other) noexcept : mysql_(other.mysql_), committed_(other.committed_) {
        other.mysql_ = nullptr;
    }
    bool commit() {
        if (!mysql_) return false;
        committed_ = true;
        bool ok = mysql_commit(mysql_) == 0;
        mysql_autocommit(mysql_, 1);
        return ok;
    }

private:
    MYSQL* mysql_;
    bool committed_;
};

class MySQLPool {
public:
    static MySQLPool& instance();

    // 这里加默认参数 = 完全兼容 main.cpp，不用改任何代码！
    bool init(const AppConfig& cfg, int pool_size = 5);

    class ConnGuard {
    public:
        ConnGuard(MYSQL* conn, MySQLPool* pool) : conn_(conn), pool_(pool) {}
        ~ConnGuard() { if (conn_) pool_->returnConn(conn_); }
        MYSQL* get() { return conn_; }
        MYSQL* operator->() { return conn_; }
        ConnGuard(const ConnGuard&) = delete;
        ConnGuard& operator=(const ConnGuard&) = delete;
    private:
        MYSQL* conn_;
        MySQLPool* pool_;
    };

    std::shared_ptr<ConnGuard> getConnection();

    const std::string& lastError() const { return last_error_; }

    ~MySQLPool();

private:
    MySQLPool() = default;
    void returnConn(MYSQL* conn);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<MYSQL*> pool_;
    std::string last_error_;
};