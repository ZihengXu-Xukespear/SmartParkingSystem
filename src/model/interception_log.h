#pragma once
#include "base_model.h"
#include <string>

class InterceptionLog : public BaseModel {
public:
    int id = 0;
    std::string license_plate;
    std::string reason;
    std::string intercepted_at;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "INTERCEPTION_LOG"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["license_plate"] = license_plate;
        j["reason"] = reason;
        j["intercepted_at"] = intercepted_at;
        return j;
    }
};
