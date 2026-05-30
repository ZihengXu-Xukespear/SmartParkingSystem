#pragma once
#include "crud_service.h"
#include "../model/user.h"

class UserService : public CrudService<User> {
public:
    static UserService& instance();
    std::vector<User> listUsers();
    bool addUser(const User& user);
    bool updateUser(int id, const std::string& username, const std::string& telephone,
        const std::string& truename, const std::string& role);
    bool updateUserPassword(int id, const std::string& password);
    bool deleteUser(int id);

    // 注销账号需要的方法
    bool checkPassword(int userId, const std::string& password);
    bool disableUser(int id);
    User getById(int id);

protected:
    User mapRow(MYSQL_ROW row) override;
private:
    UserService() = default;
};