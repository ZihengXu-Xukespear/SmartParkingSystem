#pragma once
#include "base_model.h"
#include <string>

class Bulletin : public BaseModel {
public:
    int id = 0;
    std::string content;
    bool is_pinned = false;
    std::string valid_from;
    std::string valid_until;
    std::string created_at;
    std::string updated_at;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "BULLETIN"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["content"] = content;
        j["is_pinned"] = is_pinned;
        j["valid_from"] = valid_from;
        j["valid_until"] = valid_until;
        j["created_at"] = created_at;
        j["updated_at"] = updated_at;
        return j;
    }
};
