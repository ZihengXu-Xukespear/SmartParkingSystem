#include "base_controller.h"
#include "../service/auth_service.h"

crow::response BaseController::errorResponse(int code, const std::string& message) {
    crow::response res(code, "{\"error\":\"" + message + "\"}");
    res.set_header("Content-Type", "application/json; charset=utf-8");
    return res;
}

crow::response BaseController::successResponse(const std::string& message) {
    crow::response res(200, "{\"message\":\"" + message + "\"}");
    res.set_header("Content-Type", "application/json; charset=utf-8");
    return res;
}

std::string BaseController::extractToken(const crow::request& req) {
    std::string token = req.get_header_value("Authorization");
    if (token.size() >= 7 && token.substr(0, 7) == "Bearer ") token = token.substr(7);
    return token;
}

std::pair<int, std::string> BaseController::authenticate(const crow::request& req) {
    std::string token = extractToken(req);
    if (token.empty()) return {-1, ""};
    int uid = AuthService::instance().getUserId(token);
    if (uid == -1) return {-1, ""};
    std::string role = AuthService::instance().getUserRole(token);
    return {uid, role};
}

bool BaseController::isAuthenticated(const crow::request& req) {
    return authenticate(req).first != -1;
}

bool BaseController::checkPermission(const crow::request& req, const std::string& permission) {
    std::string token = extractToken(req);
    if (token.empty()) return false;
    return AuthService::instance().hasPermission(token, permission);
}

bool BaseController::checkPermissions(const crow::request& req, const std::vector<std::string>& permissions) {
    std::string token = extractToken(req);
    if (token.empty()) return false;
    for (const auto& p : permissions) {
        if (!AuthService::instance().hasPermission(token, p)) return false;
    }
    return true;
}

bool BaseController::isAdmin(const crow::request& req) {
    return checkPermission(req, "user.manage");
}

bool BaseController::isRoot(const crow::request& req) {
    return getRole(req) == "admin";
}

std::string BaseController::getRole(const crow::request& req) {
    return authenticate(req).second;
}

crow::json::rvalue BaseController::parseBody(const crow::request& req) {
    return crow::json::load(req.body);
}
