#pragma once
#include <string>
#include "base_model.h"

class CarRecord : public BaseModel {
public:
    int id = 0;
    std::string license_plate;
    std::string check_in_time;
    std::string check_out_time;
    double fee = 0.0;
    std::string location;
    std::string billing_type = "standard";
    std::string duration;
    std::string exit_deadline;
    std::string P_name;
    int spot_number = 0;
    int operator_id = 0;
    double charging_fee = 0.0;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "CAR_RECORD"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["license_plate"] = license_plate;
        j["check_in_time"] = check_in_time;
        j["check_out_time"] = check_out_time;
        j["fee"] = fee;
        j["location"] = location;
        j["billing_type"] = billing_type;
        j["duration"] = duration;
        j["exit_deadline"] = exit_deadline;
        j["P_name"] = P_name.empty() ? location : P_name;
        j["spot_number"] = spot_number;
        j["operator_id"] = operator_id;
        j["charging_fee"] = charging_fee;
        return j;
    }
};
