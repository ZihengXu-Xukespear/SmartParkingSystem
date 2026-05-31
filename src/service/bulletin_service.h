#pragma once
#include "crud_service.h"
#include "../model/bulletin.h"

class BulletinService : public CrudService<Bulletin> {
public:
    static BulletinService& instance() {
        static BulletinService inst;
        return inst;
    }

    std::vector<Bulletin> getActive() {
        return list(
            "SELECT id,content,is_pinned,valid_from,valid_until,created_at,updated_at FROM BULLETIN "
            "WHERE (valid_from IS NULL OR valid_from <= NOW()) "
            "AND (valid_until IS NULL OR valid_until >= NOW()) "
            "ORDER BY is_pinned DESC, created_at DESC"
        );
    }

    std::vector<Bulletin> getAll() {
        return list(
            "SELECT id,content,is_pinned,valid_from,valid_until,created_at,updated_at FROM BULLETIN "
            "ORDER BY is_pinned DESC, created_at DESC"
        );
    }

    bool create(const std::string& content, bool isPinned,
                const std::string& validFrom, const std::string& validUntil, std::string& error) {
        auto conn = getConnection();
        if (!conn) { error = "数据库连接失败"; return false; }
        MYSQL* mysql = conn->get();
        std::string sql = "INSERT INTO BULLETIN (content,is_pinned,valid_from,valid_until) VALUES ('" +
            escape(mysql, content) + "'," + std::to_string(isPinned ? 1 : 0) + "," +
            (validFrom.empty() ? "NULL" : "'" + validFrom + "'") + "," +
            (validUntil.empty() ? "NULL" : "'" + validUntil + "'") + ")";
        if (!executeQuery(mysql, sql)) { error = "创建失败"; return false; }
        return true;
    }

    bool update(int id, const std::string& content, bool isPinned,
                const std::string& validFrom, const std::string& validUntil, std::string& error) {
        auto conn = getConnection();
        if (!conn) { error = "数据库连接失败"; return false; }
        MYSQL* mysql = conn->get();
        std::string sql = "UPDATE BULLETIN SET content='" + escape(mysql, content) + "',"
            "is_pinned=" + std::to_string(isPinned ? 1 : 0) + ","
            "valid_from=" + (validFrom.empty() ? "NULL" : "'" + validFrom + "'") + ","
            "valid_until=" + (validUntil.empty() ? "NULL" : "'" + validUntil + "'") +
            " WHERE id=" + std::to_string(id);
        if (!executeQuery(mysql, sql)) { error = "更新失败"; return false; }
        return true;
    }

    bool remove(int id) {
        return deleteById(id);
    }

protected:
    Bulletin mapRow(MYSQL_ROW row) override {
        Bulletin b;
        b.id = std::stoi(row[0]);
        b.content = row[1] ? row[1] : "";
        b.is_pinned = row[2] && std::stoi(row[2]) != 0;
        b.valid_from = row[3] ? row[3] : "";
        b.valid_until = row[4] ? row[4] : "";
        b.created_at = row[5] ? row[5] : "";
        b.updated_at = row[6] ? row[6] : "";
        return b;
    }

private:
    BulletinService() = default;
};
