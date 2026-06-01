#pragma once
#include <string>
#include "base_model.h"
#include "crow.h"

class UserPass : public BaseModel {
public:
    int id = 0;
    int user_id = 0;
    std::string license_plate;
    std::string pass_type;
    std::string start_date;
    std::string end_date;
    double fee = 0.0;
    bool is_active = false;
    std::string P_name;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "MONTHLY_PASS"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["user_id"] = user_id;
        j["license_plate"] = license_plate;
        j["pass_type"] = pass_type;
        j["start_date"] = start_date;
        j["end_date"] = end_date;
        j["fee"] = fee;
        j["is_active"] = is_active;
        j["P_name"] = P_name;
        return j;
    }
};