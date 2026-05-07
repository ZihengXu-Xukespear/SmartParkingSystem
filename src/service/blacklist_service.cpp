#include "blacklist_service.h"

BlacklistService& BlacklistService::instance() {
    static BlacklistService inst;
    return inst;
}

std::vector<BlacklistEntry> BlacklistService::getAll() {
    return list("SELECT id,license_plate,reason,created_at FROM VEHICLE_BLACKLIST ORDER BY created_at DESC");
}

bool BlacklistService::isBlacklisted(const std::string& plate, std::string* reason_out) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "SELECT reason FROM VEHICLE_BLACKLIST WHERE license_plate=" + quote(mysql, plate);
    if (mysql_query(mysql, sql.c_str()) != 0) return false;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return false;
    MYSQL_ROW row = mysql_fetch_row(res);
    bool blocked = row != nullptr;
    if (blocked && reason_out) *reason_out = row[0] ? row[0] : "";
    mysql_free_result(res);
    return blocked;
}

bool BlacklistService::add(const std::string& plate, const std::string& reason, std::string& error) {
    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();
    std::string sql = "INSERT IGNORE INTO VEHICLE_BLACKLIST (license_plate,reason) VALUES (" +
        quote(mysql, plate) + "," + quote(mysql, reason) + ")";
    if (!executeQuery(mysql, sql)) { error = "添加失败"; return false; }
    return true;
}

bool BlacklistService::remove(int id) {
    return deleteById(id);
}

bool BlacklistService::logInterception(const std::string& plate, const std::string& reason) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "INSERT INTO INTERCEPTION_LOG (license_plate,reason) VALUES (" +
        quote(mysql, plate) + "," + quote(mysql, reason) + ")";
    return mysql_query(mysql, sql.c_str()) == 0;
}

std::vector<InterceptionLog> BlacklistService::getRecentInterceptions(int limit) {
    std::vector<InterceptionLog> list;
    auto conn = getConnection();
    if (!conn) return list;
    MYSQL* mysql = conn->get();
    std::string sql = "SELECT id,license_plate,reason,intercepted_at FROM INTERCEPTION_LOG ORDER BY intercepted_at DESC LIMIT " + std::to_string(limit);
    if (mysql_query(mysql, sql.c_str()) != 0) return list;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return list;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        InterceptionLog l;
        l.id = std::stoi(row[0]);
        l.license_plate = row[1] ? row[1] : "";
        l.reason = row[2] ? row[2] : "";
        l.intercepted_at = row[3] ? row[3] : "";
        list.push_back(l);
    }
    mysql_free_result(res);
    return list;
}

int BlacklistService::getRecentInterceptionCount(int hours) {
    auto conn = getConnection();
    if (!conn) return 0;
    MYSQL* mysql = conn->get();
    std::string sql = "SELECT COUNT(*) FROM INTERCEPTION_LOG WHERE intercepted_at >= DATE_SUB(NOW(), INTERVAL " + std::to_string(hours) + " HOUR)";
    if (mysql_query(mysql, sql.c_str()) != 0) return 0;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(res);
    return count;
}

BlacklistEntry BlacklistService::mapRow(MYSQL_ROW row) {
    BlacklistEntry e;
    e.id = std::stoi(row[0]);
    e.license_plate = row[1] ? row[1] : "";
    e.reason = row[2] ? row[2] : "";
    e.created_at = row[3] ? row[3] : "";
    return e;
}
