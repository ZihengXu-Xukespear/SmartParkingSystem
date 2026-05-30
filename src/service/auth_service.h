#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <random>
#include <ctime>
#include "base_service.h"
#include "../model/user.h"
#include "../sha256.h"
#include "../permissions.h"

#include "model/user.h"

class AuthService : public BaseService {
public:
    static AuthService& instance();

    std::string login(const std::string& username, const std::string& password, User& out_user);
    bool registerUser(const User& user);
    bool validateToken(const std::string& token);
    int getUserId(const std::string& token);
    std::string getUserRole(const std::string& token);
    void logout(const std::string& token);
    void cleanupExpiredTokens();

    bool hasPermission(const std::string& token, const std::string& permission);
    std::vector<std::string> getUserPermissions(const std::string& token);

    static bool createDefaultAdmin();

private:
    AuthService();
    std::string generateToken();

    struct TokenInfo {
        int userId;
        std::string role;
        time_t expiry;
    };
    std::unordered_map<std::string, TokenInfo> tokens_;
    std::mutex tokens_mutex_;
    std::mt19937 rng_;
    int token_ttl_seconds_ = 86400;
};
