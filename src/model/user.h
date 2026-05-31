#pragma once
#include <string>
#include "base_model.h"

class User : public BaseModel {
public:
    int id = 0;
    std::string username;
    std::string password;
    std::string telephone;
    std::string truename;
    std::string role = "user";
    double balance = 0.0;
    std::string created_at;

    int getId() const override { return id; }
    void setId(int id_) override { id = id_; }
    std::string getTableName() const override { return "USER"; }

    crow::json::wvalue serialize() const override {
        crow::json::wvalue j;
        j["id"] = id;
        j["username"] = username;
        j["telephone"] = telephone;
        j["truename"] = truename;
        j["role"] = role;
        j["balance"] = balance;
        j["created_at"] = created_at;
        return j;
    }
};