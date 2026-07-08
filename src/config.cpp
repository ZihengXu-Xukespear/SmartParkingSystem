#include "config.h"
#include <cstdlib>

bool AppConfig::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto j = crow::json::load(content);
    if (!j) return false;
    std::lock_guard<std::shared_mutex> lock(mutex_);
    host = j["host"].s();
    port = j["port"].i();
    database = j["database"].s();
    user = j["user"].s();
    password = j["password"].s();
    parking_name = j["parking_name"].s();
    fee = j["fee"].d();
    capacity = j["capacity"].i();
    server_port = j["server_port"].i();
    if (j.has("notice")) notice = j["notice"].s();
    if (j.has("notice_expire_minutes")) notice_expire_minutes = j["notice_expire_minutes"].i();
    if (j.has("llm_base_url")) llm_base_url = j["llm_base_url"].s();
    if (j.has("llm_model")) llm_model = j["llm_model"].s();
    if (j.has("llm_api_key")) llm_api_key = j["llm_api_key"].s();
    // Environment variable takes precedence over the config file for the key
    if (const char* envKey = std::getenv("SP_LLM_KEY")) {
        std::string ek(envKey);
        if (!ek.empty()) llm_api_key = ek;
    }
    initialized = true;
    return true;
}

bool AppConfig::save(const std::string& path) {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    crow::json::wvalue j;
    j["host"] = host;
    j["port"] = port;
    j["database"] = database;
    j["user"] = user;
    j["password"] = password;
    j["parking_name"] = parking_name;
    j["fee"] = fee;
    j["capacity"] = capacity;
    j["server_port"] = server_port;
    j["notice_expire_minutes"] = notice_expire_minutes;
    j["notice"] = notice;
    j["llm_base_url"] = llm_base_url;
    j["llm_model"] = llm_model;
    j["llm_api_key"] = llm_api_key;
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump();
    initialized = true;
    return true;
}
