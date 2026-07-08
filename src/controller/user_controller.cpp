#include "user_controller.h"
#include "../service/user_service.h"
#include "../service/plate_service.h"
#include "../database/mysql_pool.h"
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

    // ========== User Plate Binding ==========
    // Get plates bound to current user
    CROW_ROUTE(app, "/api/user/plates").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto auth = BaseController::authenticate(req);
        int userId = auth.first;

        auto conn = MySQLPool::instance().getConnection();
        if (!conn) return BaseController::errorResponse(500, "数据库连接失败");

        std::string sql = "SELECT id, license_plate, created_at FROM USER_PLATE WHERE user_id=" + std::to_string(userId) + " ORDER BY created_at DESC";
        if (mysql_query(conn->get(), sql.c_str()) != 0)
            return BaseController::errorResponse(500, "查询失败");

        MYSQL_RES* res = mysql_store_result(conn->get());
        std::vector<crow::json::wvalue> plates;
        if (res) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                crow::json::wvalue p;
                p["id"] = std::stoi(row[0]);
                p["license_plate"] = row[1] ? row[1] : "";
                p["created_at"] = row[2] ? row[2] : "";
                plates.push_back(std::move(p));
            }
            mysql_free_result(res);
        }
        crow::json::wvalue result;
        result["plates"] = std::move(plates);
        return crow::response(result);
    });

    // Bind a plate to current user
    CROW_ROUTE(app, "/api/user/plates").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto auth = BaseController::authenticate(req);
        int userId = auth.first;

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        std::string plate = body["license_plate"].s();
        if (plate.empty()) return BaseController::errorResponse(400, "请输入车牌号");

        // Validate plate format
        std::string validateError;
        if (!PlateService::validatePlate(plate)) {
            return BaseController::errorResponse(400, "车牌号格式不正确");
        }

        auto conn = MySQLPool::instance().getConnection();
        if (!conn) return BaseController::errorResponse(500, "数据库连接失败");
        MYSQL* mysql = conn->get();

        // Quote plate value safely
        char* buf = new char[plate.size() * 2 + 3];
        buf[0] = '\'';
        unsigned long len = mysql_real_escape_string(mysql, buf + 1, plate.c_str(), (unsigned long)plate.size());
        buf[1 + len] = '\'';
        buf[2 + len] = '\0';
        std::string quotedPlate(buf);
        delete[] buf;

        std::string sql = "INSERT IGNORE INTO USER_PLATE (user_id, license_plate) VALUES (" +
            std::to_string(userId) + "," + quotedPlate + ")";
        if (mysql_query(mysql, sql.c_str()) != 0)
            return BaseController::errorResponse(400, "绑定失败，该车牌可能已绑定");

        if (mysql_affected_rows(mysql) == 0)
            return BaseController::errorResponse(400, "该车牌已绑定");

        return BaseController::successResponse("车牌绑定成功");
    });

    // Unbind a plate
    CROW_ROUTE(app, "/api/user/plates/<int>").methods("DELETE"_method)([](const crow::request& req, int plateId) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto auth = BaseController::authenticate(req);
        int userId = auth.first;

        auto conn = MySQLPool::instance().getConnection();
        if (!conn) return BaseController::errorResponse(500, "数据库连接失败");

        std::string sql = "DELETE FROM USER_PLATE WHERE id=" + std::to_string(plateId) +
            " AND user_id=" + std::to_string(userId);
        if (mysql_query(conn->get(), sql.c_str()) != 0 || mysql_affected_rows(conn->get()) == 0)
            return BaseController::errorResponse(400, "解绑失败");

        return BaseController::successResponse("解绑成功");
    });
}
