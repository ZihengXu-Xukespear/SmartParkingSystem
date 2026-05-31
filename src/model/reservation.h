#pragma once
#include <string>
#include "base_model.h"

class Reservation : public BaseModel {
public:
    int id = 0;
    std::string license_plate;
    std::string P_name;
    double prepaid = 0.0;
    std::string status = "active";  // active/completed/cancelled/expired
    int spot_number = 0;
    std::string created_at;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "RESERVATION"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["license_plate"] = license_plate;
        j["P_name"] = P_name;
        j["prepaid"] = prepaid;
        j["status"] = status;
        j["spot_number"] = spot_number;
        j["created_at"] = created_at;
        return j;
    }
};
