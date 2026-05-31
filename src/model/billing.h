#pragma once
#include <string>
#include "base_model.h"

class BillingRule : public BaseModel {
public:
    int id = 0;
    std::string rule_name;
    std::string rule_type;
    int free_minutes = 30;
    double hourly_rate = 5.00;
    double max_daily_fee = 50.00;
    std::string tier_config;
    std::string description;
    bool is_active = true;
    std::string P_name;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "BILLING_RULE"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["rule_name"] = rule_name;
        j["rule_type"] = rule_type;
        j["free_minutes"] = free_minutes;
        j["hourly_rate"] = hourly_rate;
        j["max_daily_fee"] = max_daily_fee;
        j["tier_config"] = tier_config;
        j["description"] = description;
        j["is_active"] = is_active;
        j["P_name"] = P_name;
        return j;
    }
};

class PassPlan : public BaseModel {
public:
    int id = 0;
    std::string plan_name;
    int duration_days = 30;
    double price = 300.00;
    std::string description;
    bool is_active = true;
    std::string P_name;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "PASS_PLAN"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["plan_name"] = plan_name;
        j["duration_days"] = duration_days;
        j["price"] = price;
        j["description"] = description;
        j["is_active"] = is_active;
        j["P_name"] = P_name;
        return j;
    }
};

class MonthlyPass : public BaseModel {
public:
    int id = 0;
    std::string license_plate;
    std::string pass_type;
    std::string start_date;
    std::string end_date;
    double fee = 0.0;
    bool is_active = true;
    int user_id = 0;
    int plan_id = 0;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "MONTHLY_PASS"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["license_plate"] = license_plate;
        j["pass_type"] = pass_type;
        j["start_date"] = start_date;
        j["end_date"] = end_date;
        j["fee"] = fee;
        j["is_active"] = is_active;
        j["user_id"] = user_id;
        j["plan_id"] = plan_id;
        return j;
    }
};
