#pragma once
#include <string>
#include "base_model.h"

// A single message inside a customer-service session.
// sender_type: "user" | "assistant" | "admin" | "system"
class CSMessage : public BaseModel {
public:
    int id = 0;
    int session_id = 0;
    int user_id = 0;          // the customer this message belongs to (denormalized)
    std::string sender_type;  // user | assistant | admin | system
    int sender_id = 0;        // actual user id for admin messages
    std::string content;
    std::string created_at;
    bool is_read_by_admin = false;
    bool is_read_by_user = false;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "CS_MESSAGE"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["session_id"] = session_id;
        j["user_id"] = user_id;
        j["sender_type"] = sender_type;
        j["sender_id"] = sender_id;
        j["content"] = content;
        j["created_at"] = created_at;
        j["is_read_by_admin"] = is_read_by_admin;
        j["is_read_by_user"] = is_read_by_user;
        return j;
    }
};
