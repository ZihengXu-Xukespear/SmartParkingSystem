#include "message_service.h"
#include "../config.h"

Message MessageService::mapRow(MYSQL_ROW row) {
    Message m;
    m.id = std::stoi(row[0]);
    m.sender_id = std::stoi(row[1]);
    m.receiver_id = std::stoi(row[2]);
    m.content = row[3] ? row[3] : "";
    m.created_at = row[4] ? row[4] : "";
    m.is_read = row[5] && std::stoi(row[5]) != 0;
    return m;
}

bool MessageService::sendMessage(int senderId, int receiverId, const std::string& content, std::string& error) {
    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();
    std::string sql = "INSERT INTO MESSAGE (sender_id, receiver_id, content) VALUES (" +
        std::to_string(senderId) + "," + std::to_string(receiverId) + ",'" + escape(mysql, content) + "')";
    if (!executeQuery(mysql, sql)) { error = "发送失败"; return false; }
    return true;
}

crow::json::wvalue MessageService::getConversations() {
    auto conn = getConnection();
    crow::json::wvalue result;
    std::vector<crow::json::wvalue> convos;
    if (!conn) { result["conversations"] = std::move(convos); return result; }
    MYSQL* mysql = conn->get();

    std::string sql =
        "SELECT u.id, u.username, u.truename, u.telephone, u.role, u.balance, "
        "  m.content AS last_message, m.created_at AS last_time, "
        "  (SELECT COUNT(*) FROM MESSAGE m2 "
        "   WHERE m2.sender_id = u.id AND m2.receiver_id = 0 AND m2.is_read = 0) AS unread_count, "
        "  u.created_at AS user_created_at "
        "FROM USER u "
        "INNER JOIN MESSAGE m ON m.id = ("
        "  SELECT MAX(m3.id) FROM MESSAGE m3 "
        "  WHERE (m3.sender_id = u.id AND m3.receiver_id = 0) "
        "     OR (m3.receiver_id = u.id AND m3.sender_id IN (SELECT id FROM USER WHERE role='admin'))"
        ") "
        "WHERE u.role NOT IN ('admin','root') "
        "ORDER BY m.created_at DESC";

    if (mysql_query(mysql, sql.c_str()) != 0) {
        result["conversations"] = std::move(convos);
        return result;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) { result["conversations"] = std::move(convos); return result; }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        crow::json::wvalue c;
        c["user_id"] = std::stoi(row[0]);
        c["username"] = row[1] ? row[1] : "";
        c["truename"] = row[2] ? row[2] : "";
        c["telephone"] = row[3] ? row[3] : "";
        c["role"] = row[4] ? row[4] : "";
        c["balance"] = row[5] ? std::stod(row[5]) : 0.0;
        c["last_message"] = row[6] ? row[6] : "";
        c["last_time"] = row[7] ? row[7] : "";
        c["unread_count"] = row[8] ? std::stoi(row[8]) : 0;
        c["created_at"] = row[9] ? row[9] : "";
        convos.push_back(std::move(c));
    }
    mysql_free_result(res);

    result["conversations"] = std::move(convos);
    return result;
}

std::vector<Message> MessageService::getHistory(int userId, int otherUserId, int limit) {
    std::string sql;
    if (otherUserId == 0) {
        // Chat with admin: user→0 OR any admin→user
        sql = "SELECT id, sender_id, receiver_id, content, created_at, is_read FROM MESSAGE "
            "WHERE (sender_id=" + std::to_string(userId) + " AND receiver_id=0) "
            "OR (receiver_id=" + std::to_string(userId) + " AND sender_id IN (SELECT id FROM USER WHERE role='admin')) "
            "ORDER BY created_at ASC LIMIT " + std::to_string(limit);
    } else {
        // Admin viewing a user's conversation: include user→0 messages plus direct exchanges
        sql = "SELECT id, sender_id, receiver_id, content, created_at, is_read FROM MESSAGE "
            "WHERE (sender_id=" + std::to_string(userId) + " AND receiver_id=" + std::to_string(otherUserId) + ") "
            "OR (sender_id=" + std::to_string(otherUserId) + " AND receiver_id=" + std::to_string(userId) + ") "
            "OR (sender_id=" + std::to_string(otherUserId) + " AND receiver_id=0) "
            "ORDER BY created_at ASC LIMIT " + std::to_string(limit);
    }
    return list(sql);
}

bool MessageService::markAsRead(int receiverId, int senderId) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql;
    if (senderId == 0) {
        // Mark all admin messages as read
        sql = "UPDATE MESSAGE SET is_read=1 WHERE receiver_id=" + std::to_string(receiverId) +
            " AND sender_id IN (SELECT id FROM USER WHERE role='admin') AND is_read=0";
    } else {
        sql = "UPDATE MESSAGE SET is_read=1 WHERE receiver_id=" +
            std::to_string(receiverId) + " AND sender_id=" + std::to_string(senderId) + " AND is_read=0";
    }
    return executeQuery(mysql, sql);
}

int MessageService::getUnreadCount(int userId) {
    auto conn = getConnection();
    if (!conn) return 0;
    MYSQL* mysql = conn->get();
    std::string sql = "SELECT COUNT(*) FROM MESSAGE WHERE receiver_id=" + std::to_string(userId) + " AND is_read=0";
    if (mysql_query(mysql, sql.c_str()) != 0) return 0;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = (row && row[0]) ? std::stoi(row[0]) : 0;
    mysql_free_result(res);
    return count;
}
