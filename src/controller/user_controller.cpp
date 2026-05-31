#include "user_controller.h"
#include "../service/user_service.h"
#include "../permissions.h"

std::string UserController::getPrefix() const { return "/api/user"; }

void UserController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/user/list").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::USER_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto users = UserService::instance().listUsers();
        crow::json::wvalue res;
        res["users"] = BaseController::toJsonArray(users);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/user/add").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::USER_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        User user;
        user.username = body["username"].s();
        user.password = body["password"].s();
        user.telephone = body["telephone"].s();
        user.truename = body["truename"].s();
        user.role = body.has("role") ? std::string(body["role"].s()) : std::string("user");

        if (!UserService::instance().addUser(user))
            return BaseController::errorResponse(400, "添加失败，用户名可能已存在");

        return BaseController::successResponse("添加成功");
    });

    CROW_ROUTE(app, "/api/user/update").methods("PUT"_method)([](const crow::request& req) {
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        int id = body["id"].i();
        auto auth = BaseController::authenticate(req);
        bool isSelfUpdate = (auth.first == id);
        bool hasManagePerm = BaseController::checkPermission(req, Permissions::USER_MANAGE);

        if (!isSelfUpdate && !hasManagePerm)
            return BaseController::errorResponse(403, "权限不足");

        std::string username = body["username"].s();
        std::string telephone = body["telephone"].s();
        std::string truename = body["truename"].s();
        std::string newRole = body.has("role") ? std::string(body["role"].s()) : std::string("user");

        if (isSelfUpdate && !hasManagePerm)
            newRole = "";

        if (newRole == "admin" && !BaseController::isRoot(req))
            return BaseController::errorResponse(403, "只有管理员可以授予管理员角色");

        if (!UserService::instance().updateUser(id, username, telephone, truename, newRole))
            return BaseController::errorResponse(400, "更新失败");

        if (body.has("password") && std::string(body["password"].s()).size() > 0) {
            UserService::instance().updateUserPassword(id, body["password"].s());
        }

        return BaseController::successResponse("更新成功");
    });

    CROW_ROUTE(app, "/api/user/<int>").methods("DELETE"_method)([](const crow::request& req, int id) {
        if (!BaseController::checkPermission(req, Permissions::USER_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        if (auth.first == id)
            return BaseController::errorResponse(403, "不能删除自己的账号");
        if (!UserService::instance().deleteUser(id))
            return BaseController::errorResponse(400, "删除失败");
        return BaseController::successResponse("删除成功");
    });
}
