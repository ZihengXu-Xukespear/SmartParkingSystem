#include "user_service.h"
#include "../sha256.h"

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
