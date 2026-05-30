#include "mysql_pool.h"
#include <iostream>

MySQLPool& MySQLPool::instance() {
    static MySQLPool inst;
    return inst;
}

bool MySQLPool::init(const AppConfig& cfg, int pool_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    mysql_library_init(0, nullptr, nullptr);

    for (int i = 0; i < pool_size; i++) {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn) {
            std::cout << "[MySQL] mysql_init failed" << std::endl;
            return false;
        }

        mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
        mysql_options(conn, MYSQL_INIT_COMMAND, "SET NAMES utf8mb4");

        if (!mysql_real_connect(conn,
            cfg.host.c_str(),
            cfg.user.c_str(),
            cfg.password.c_str(),
            cfg.database.c_str(),
            cfg.port,
            nullptr, 0))
        {
            std::cout << "[MySQL] connect failed: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return false;
        }

        pool_.push(conn);
    }

    std::cout << "[MySQL] pool initialized with " << pool_size << " connections" << std::endl;
    return true;
}

std::shared_ptr<MySQLPool::ConnGuard> MySQLPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (pool_.empty()) {
        if (cv_.wait_for(lock, std::chrono::seconds(5)) == std::cv_status::timeout) {
            return nullptr;
        }
    }

    MYSQL* conn = pool_.front();
    pool_.pop();

    if (mysql_ping(conn) != 0) {
        const auto& cfg = AppConfig::instance();
        std::cout << "[MySQL] connection ping failed, reconnecting..." << std::endl;
        mysql_close(conn);

        conn = mysql_init(nullptr);
        mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

        if (!mysql_real_connect(conn,
            cfg.host.c_str(),
            cfg.user.c_str(),
            cfg.password.c_str(),
            cfg.database.c_str(),
            cfg.port, nullptr, 0))
        {
            std::cout << "[MySQL] reconnect failed: " << mysql_error(conn) << std::endl;
            return nullptr;
        }
    }

    return std::make_shared<ConnGuard>(conn, this);
}

void MySQLPool::returnConn(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push(conn);
    cv_.notify_one();
}

MySQLPool::~MySQLPool() {
    while (!pool_.empty()) {
        mysql_close(pool_.front());
        pool_.pop();
    }
    mysql_library_end();
}