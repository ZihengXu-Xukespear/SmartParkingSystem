#include "user_service.h"
#include "../sha256.h"

#include <iostream>

UserService& UserService::instance() {
    static UserService inst;
    return inst;
}

std::vector<User> UserService::listUsers() {
    return list("SELECT id,username,telephone,truename,role,balance,created_at FROM USER ORDER BY id");
}

bool UserService::addUser(const User& user) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    std::string hashed = sha256::hash(user.password);
    std::string sql = "INSERT INTO USER (username,password,telephone,truename,role) VALUES (" +
        quote(mysql, user.username) + "," +
        quote(mysql, hashed) + "," +
        quote(mysql, user.telephone) + "," +
        quote(mysql, user.truename) + "," +
        quote(mysql, user.role) + ")";
    return executeQuery(mysql, sql);
}

bool UserService::updateUser(int id, const std::string& username, const std::string& telephone,
    const std::string& truename, const std::string& role) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    std::string sql = "UPDATE USER SET username=" + quote(mysql, username) +
        ", telephone=" + quote(mysql, telephone) +
        ", truename=" + quote(mysql, truename);
    if (!role.empty())
        sql += ", role=" + quote(mysql, role);
    sql += " WHERE id=" + std::to_string(id);
    return executeQuery(mysql, sql);
}

bool UserService::updateUserPassword(int id, const std::string& password) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    std::string hashed = sha256::hash(password);
    std::string sql = "UPDATE USER SET password=" + quote(mysql, hashed) +
        " WHERE id=" + std::to_string(id);
    return executeQuery(mysql, sql);
}

bool UserService::deleteUser(int id) {
    return deleteById(id);
}

User UserService::mapRow(MYSQL_ROW row) {
    User u;
    u.id = std::stoi(row[0]);
    u.username = row[1] ? row[1] : "";
    u.telephone = row[2] ? row[2] : "";
    u.truename = row[3] ? row[3] : "";
    u.role = row[4] ? row[4] : "user";
    u.balance = row[5] ? std::stod(row[5]) : 0.0;
    u.created_at = row[6] ? row[6] : "";
    return u;
}

// ==================== 密码校验（正确匹配 SHA256 加密） ====================
// 密码校验（适配 SHA256）
bool UserService::checkPassword(int userId, const std::string& password) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT password FROM USER WHERE id = %d", userId);
    if (mysql_query(mysql, sql)) return false;

    MYSQL_RES* res = mysql_store_result(mysql);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return false;
    }

    std::string inputHash = sha256::hash(password);
    std::string dbHash = row[0];
    bool ok = (inputHash == dbHash);
    mysql_free_result(res);
    return ok;
}

// 注销账号
bool UserService::disableUser(int id) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE USER SET status=0 WHERE id=%d", id);

    int result = mysql_query(mysql, sql);
    return result == 0;
}

// ==================== 根据ID获取用户 ====================
User UserService::getById(int id) {
    User u;
    auto conn = getConnection();
    if (!conn) return u;
    MYSQL* mysql = conn->get();

    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id,username,password FROM USER WHERE id = %d", id);
    if (mysql_query(mysql, sql)) return u;

    MYSQL_RES* res = mysql_store_result(mysql);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        u.id = id;
        u.username = row[1];
        u.password = row[2];
    }
    mysql_free_result(res);
    return u;
}