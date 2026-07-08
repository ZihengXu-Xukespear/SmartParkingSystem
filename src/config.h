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
    // AI customer-service (LLM) settings.
    // These are only fallback defaults; real values are loaded from config/db_config.json
    // (or the SP_LLM_KEY env var for the API key) at startup.
    std::string llm_base_url = "http://your-llm-host:port";
    std::string llm_api_key;
    std::string llm_model = "glm-5.2";
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
