#pragma once
#include <string>
#include "base_model.h"

class Message : public BaseModel {
public:
    int id = 0;
    int sender_id = 0;
    int receiver_id = 0;
    std::string content;
    std::string created_at;
    bool is_read = false;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "MESSAGE"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["sender_id"] = sender_id;
        j["receiver_id"] = receiver_id;
        j["content"] = content;
        j["created_at"] = created_at;
        j["is_read"] = is_read;
        return j;
    }
};
