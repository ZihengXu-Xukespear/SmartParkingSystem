#include "user_controller.h"
#include "../service/user_service.h"
#include "../permissions.h"

#include <mysql.h>
#include <iostream>
#include "../config.h"
#include "../sha256.h"

std::string UserController::getPrefix() const { return "/api/user"; }

void UserController::registerRoutes(crow::SimpleApp& app) {

    // ===================== 【前端用户注册接口】 =====================
    CROW_ROUTE(app, "/api/user/register").methods("POST"_method)([](const crow::request& req) {
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "参数错误");

        User user;
        user.username = body["username"].s();
        user.password = body["password"].s();
        user.telephone = body["telephone"].s();
        user.truename = body["truename"].s();
        user.role = "user";

        if (!UserService::instance().addUser(user)) {
            return BaseController::errorResponse(400, "注册失败，用户名已存在");
        }

        return BaseController::successResponse("注册成功");
        });

    // ===================== 【用户余额 + 套餐查询接口】 =====================
    CROW_ROUTE(app, "/api/user/balance").methods("GET"_method)([](const crow::request& req) {
        auto auth_pair = BaseController::authenticate(req);
        int user_id = auth_pair.first;

        if (user_id <= 0) {
            return crow::response(401, R"({"code":401,"msg":"未登录或登录已过期"})");
        }

        MYSQL* conn = mysql_init(nullptr);
        if (!conn) {
            return crow::response(500, R"({"code":500,"msg":"数据库初始化失败"})");
        }

        const AppConfig& cfg = AppConfig::instance();
        if (!mysql_real_connect(conn, cfg.host.c_str(), cfg.user.c_str(),
            cfg.password.c_str(), cfg.database.c_str(), cfg.port, nullptr, 0)) {
            mysql_close(conn);
            return crow::response(500, R"({"code":500,"msg":"数据库连接失败"})");
        }

        double balance = 0.0;
        char sql_balance[256];
        sprintf(sql_balance, "SELECT balance FROM `user` WHERE id = %d", user_id);
        if (mysql_query(conn, sql_balance) == 0) {
            MYSQL_RES* res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) {
                    balance = atof(row[0]);
                }
                mysql_free_result(res);
            }
        }

        std::string plans_json = "[";
        char sql_plan[256];
        sprintf(sql_plan, "SELECT pass_type, start_date, end_date, license_plate, fee FROM monthly_pass WHERE user_id = %d AND is_active = 1", user_id);

        if (mysql_query(conn, sql_plan) == 0) {
            MYSQL_RES* res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row;
                bool first = true;
                while ((row = mysql_fetch_row(res))) {
                    if (!first) plans_json += ",";
                    first = false;

                    std::string pass_type = row[0] ? row[0] : "";
                    std::string start_date = row[1] ? row[1] : "";
                    std::string end_date = row[2] ? row[2] : "";
                    std::string license_plate = row[3] ? row[3] : "";
                    double fee = row[4] ? atof(row[4]) : 0.0;

                    char buf[512];
                    sprintf(buf, R"({"pass_type":"%s","start_date":"%s","end_date":"%s","license_plate":"%s","fee":%.2f})",
                        pass_type.c_str(), start_date.c_str(), end_date.c_str(), license_plate.c_str(), fee);
                    plans_json += buf;
                }
                mysql_free_result(res);
            }
        }
        plans_json += "]";

        mysql_close(conn);

        char final_response[1024];
        sprintf(final_response,
            R"({"code":200,"msg":"success","data":{"balance":%.2f,"plans":%s}})",
            balance, plans_json.c_str());

        return crow::response(200, final_response);
        });

    // ===================== 【最终安全版注销接口】 =====================
    CROW_ROUTE(app, "/api/user/delete").methods("POST"_method)([](const crow::request& req) {
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "参数错误");

        int user_id = body["id"].i();
        std::string password = body["password"].s();

        MYSQL* conn = mysql_init(nullptr);
        if (!conn) return crow::response(500, R"({"code":500,"msg":"数据库失败"})");

        const AppConfig& cfg = AppConfig::instance();
        if (!mysql_real_connect(conn, cfg.host.c_str(), cfg.user.c_str(), cfg.password.c_str(), cfg.database.c_str(), cfg.port, nullptr, 0)) {
            mysql_close(conn);
            return crow::response(500, R"({"code":500,"msg":"数据库连接失败"})");
        }

        std::string hashed_pwd = sha256::hash(password);
        char sql_check[256];
        sprintf(sql_check, "SELECT password FROM `user` WHERE id=%d", user_id);

        if (mysql_query(conn, sql_check) != 0) {
            mysql_close(conn);
            return crow::response(500, R"({"code":500,"msg":"查询失败"})");
        }

        MYSQL_RES* res = mysql_store_result(conn);
        if (!res || mysql_num_rows(res) == 0) {
            mysql_free_result(res);
            mysql_close(conn);
            return crow::response(401, R"({"code":401,"msg":"密码错误"})");
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        std::string db_pwd = row[0];
        mysql_free_result(res);

        if (db_pwd != hashed_pwd) {
            mysql_close(conn);
            return crow::response(401, R"({"code":401,"msg":"密码错误"})");
        }

        char sql_update[256];
        sprintf(sql_update, "UPDATE `user` SET status=0 WHERE id=%d", user_id);
        bool success = (mysql_query(conn, sql_update) == 0);
        mysql_close(conn);

        if (success) {
            return crow::response(200, R"({"code":200,"msg":"注销成功"})");
        }
        else {
            return crow::response(500, R"({"code":500,"msg":"注销失败"})");
        }
        });

    CROW_ROUTE(app, "/api/user/rename").methods("PUT"_method)([](const crow::request& req) {
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "参数错误");

        int id = body["id"].i();
        std::string new_username = body["new_username"].s();

        MYSQL* conn = mysql_init(nullptr);
        if (!conn) return crow::response(500, R"({"code":500,"msg":"数据库失败"})");

        const AppConfig& cfg = AppConfig::instance();
        if (!mysql_real_connect(conn, cfg.host.c_str(), cfg.user.c_str(), cfg.password.c_str(), cfg.database.c_str(), cfg.port, nullptr, 0)) {
            mysql_close(conn);
            return crow::response(500, R"({"code":500,"msg":"数据库连接失败"})");
        }

        char checkSql[256];
        sprintf(checkSql, "SELECT id FROM `user` WHERE username = '%s'", new_username.c_str());
        if (mysql_query(conn, checkSql) != 0) {
            mysql_close(conn);
            return crow::response(500, R"({"code":500,"msg":"查询失败"})");
        }
        MYSQL_RES* res = mysql_store_result(conn);
        if (res && mysql_num_rows(res) > 0) {
            mysql_free_result(res);
            mysql_close(conn);
            return crow::response(400, R"({"code":400,"msg":"用户名已存在"})");
        }
        mysql_free_result(res);

        char updateSql[256];
        sprintf(updateSql, "UPDATE `user` SET username = '%s' WHERE id = %d", new_username.c_str(), id);
        bool success = (mysql_query(conn, updateSql) == 0);
        mysql_close(conn);

        if (success) {
            return crow::response(200, R"({"code":200,"msg":"用户名修改成功"})");
        }
        else {
            return crow::response(500, R"({"code":500,"msg":"修改失败"})");
        }
        });

    // ===================== 以下是原来的接口，保持不变 =====================
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