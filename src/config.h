#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include "crow.h"

struct AppConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string database = "smart_parking";
    std::string user = "root";
    std::string password;
    std::string parking_name = "停车场1";
    double fee = 5.00;
    int capacity = 100;
    int server_port = 8080;
    int notice_expire_minutes = 30;
    std::string notice = "欢迎使用智慧停车场管理系统！\n请遵守停车场管理规定，文明停车。";
    bool initialized = false;
    std::string config_dir = "config";
    std::string config_file = "config/db_config.json";

    static AppConfig& instance() {
        static AppConfig inst;
        return inst;
    }

    bool load(const std::string& path);
    bool save(const std::string& path);

    // Thread-safe read lock for concurrent reads
    std::shared_mutex mutex_;
};
