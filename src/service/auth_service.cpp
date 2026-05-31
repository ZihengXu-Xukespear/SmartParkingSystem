#include "auth_service.h"
#include "../sha256.h"

AuthService& AuthService::instance() {
    static AuthService inst;
    return inst;
}

std::string AuthService::login(const std::string& username, const std::string& password, User& out_user) {
    auto conn = getConnection();
    if (!conn) return "";

    std::string hashed = sha256::hash(password);
    std::string sql = "SELECT id,username,password,telephone,truename,role,created_at FROM USER WHERE username=" +
        quote(conn->get(), username) + " AND password=" + quote(conn->get(), hashed);

    if (mysql_query(conn->get(), sql.c_str()) != 0) return "";
    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res || mysql_num_rows(res) == 0) {
        if (res) mysql_free_result(res);
        return "";
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    out_user.id = std::stoi(row[0]);
    out_user.username = row[1] ? row[1] : "";
    out_user.password = row[2] ? row[2] : "";
    out_user.telephone = row[3] ? row[3] : "";
    out_user.truename = row[4] ? row[4] : "";
    out_user.role = row[5] ? row[5] : "user";
    out_user.created_at = row[6] ? row[6] : "";
    mysql_free_result(res);

    std::string token = generateToken();
    time_t expiry = std::time(nullptr) + token_ttl_seconds_;
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    tokens_[token] = TokenInfo{out_user.id, out_user.role, expiry};
    return token;
}

bool AuthService::registerUser(const User& user) {
    auto conn = getConnection();
    if (!conn) return false;

    std::string hashed = sha256::hash(user.password);
    std::string sql = "INSERT INTO USER (username,password,telephone,truename,role) VALUES (" +
        quote(conn->get(), user.username) + "," +
        quote(conn->get(), hashed) + "," +
        quote(conn->get(), user.telephone) + "," +
        quote(conn->get(), user.truename) + "," +
        quote(conn->get(), user.role) + ")";

    return mysql_query(conn->get(), sql.c_str()) == 0;
}

bool AuthService::validateToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    auto it = tokens_.find(token);
    if (it == tokens_.end()) return false;
    if (std::time(nullptr) >= it->second.expiry) {
        tokens_.erase(it);
        return false;
    }
    return true;
}

int AuthService::getUserId(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    auto it = tokens_.find(token);
    if (it == tokens_.end()) return -1;
    if (std::time(nullptr) >= it->second.expiry) {
        tokens_.erase(it);
        return -1;
    }
    return it->second.userId;
}

std::string AuthService::getUserRole(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    auto it = tokens_.find(token);
    if (it == tokens_.end()) return "";
    if (std::time(nullptr) >= it->second.expiry) {
        tokens_.erase(it);
        return "";
    }
    return it->second.role;
}

void AuthService::logout(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    tokens_.erase(token);
}

void AuthService::cleanupExpiredTokens() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    time_t now = std::time(nullptr);
    for (auto it = tokens_.begin(); it != tokens_.end(); ) {
        if (now >= it->second.expiry)
            it = tokens_.erase(it);
        else
            ++it;
    }
}

bool AuthService::hasPermission(const std::string& token, const std::string& permission) {
    std::string role = getUserRole(token);
    if (role.empty()) return false;
    const auto& perms = Permissions::getPermissionsForRole(role);
    return perms.find(permission) != perms.end();
}

std::vector<std::string> AuthService::getUserPermissions(const std::string& token) {
    std::string role = getUserRole(token);
    const auto& perms = Permissions::getPermissionsForRole(role);
    return std::vector<std::string>(perms.begin(), perms.end());
}

bool AuthService::createDefaultAdmin() {
    auto conn = MySQLPool::instance().getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    if (mysql_query(mysql, "SELECT COUNT(*) FROM USER WHERE role='admin'") == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && std::stoi(row[0]) > 0) {
                mysql_free_result(res);
                return true;
            }
            mysql_free_result(res);
        }
    }

    std::string hashed = sha256::hash("admin123");
    std::string sql = "INSERT INTO USER (username,password,telephone,truename,role) VALUES "
        "('admin','" + hashed + "','00000000000','系统管理员','admin')";
    return mysql_query(mysql, sql.c_str()) == 0;
}

AuthService::AuthService() {
    std::random_device rd;
    rng_.seed(rd());
}

std::string AuthService::generateToken() {
    static const char chars[] = "0123456789abcdef";
    std::uniform_int_distribution<int> dist(0, 15);
    std::string token;
    for (int i = 0; i < 64; i++) token += chars[dist(rng_)];
    return token;
}
