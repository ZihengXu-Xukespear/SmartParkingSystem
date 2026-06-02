#include "parking_controller.h"
#include "../service/parking_service.h"
#include "../service/billing_service.h"
#include "../config.h"
#include "../permissions.h"

std::string ParkingController::getPrefix() const { return "/api/parking"; }

void ParkingController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/parking/list").methods("GET"_method)([](const crow::request& req) {
        // 已注释登录验证
        // if (!BaseController::isAuthenticated(req))
        //     return BaseController::errorResponse(401, "请先登录");

        auto conn = MySQLPool::instance().getConnection();
        crow::json::wvalue res;
        std::vector<crow::json::wvalue> arr;
        if (conn) {
            if (mysql_query(conn->get(), "SELECT P_id,P_name,P_total_count,P_current_count,P_reserve_count,P_fee FROM PARKING_LOT") == 0) {
                MYSQL_RES* r = mysql_store_result(conn->get());
                if (r) {
                    MYSQL_ROW row;
                    while ((row = mysql_fetch_row(r))) {
                        crow::json::wvalue lot;
                        lot["P_id"] = std::stoi(row[0]);
                        lot["P_name"] = row[1] ? row[1] : "";
                        lot["P_total_count"] = std::stoi(row[2]);
                        lot["P_current_count"] = std::stoi(row[3]);
                        lot["P_reserve_count"] = std::stoi(row[4]);
                        lot["P_fee"] = row[5] ? std::stod(row[5]) : 5.0;
                        arr.push_back(std::move(lot));
                    }
                    mysql_free_result(r);
                }
            }
        }
        res["lots"] = std::move(arr);
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/parking/status").methods("GET"_method)([](const crow::request& req) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::PARKING_VIEW))
        //     return BaseController::errorResponse(403, "权限不足");

        auto p = req.url_params.get("P_name");
        auto lot = ParkingService::instance().getStatus(p ? p : "");
        crow::response r(lot.serialize());
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
        });

    CROW_ROUTE(app, "/api/parking/stats").methods("GET"_method)([](const crow::request& req) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::PARKING_VIEW))
        //     return BaseController::errorResponse(403, "权限不足");

        auto p = req.url_params.get("P_name");
        auto lot = ParkingService::instance().getStatus(p ? p : "");
        crow::json::wvalue res;
        res["reserved"] = lot.P_reserve_count;
        res["occupied"] = lot.P_current_count;
        res["available"] = lot.P_total_count - lot.P_current_count - lot.P_reserve_count;
        res["total"] = lot.P_total_count;
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/parking/settings").methods("PUT"_method)([](const crow::request& req) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::PARKING_SETTINGS))
        //     return BaseController::errorResponse(403, "权限不足");

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string P_name = body.has("P_name") ? std::string(body["P_name"].s()) : "";
        double fee = body["fee"].d();
        int total = body["capacity"].i();
        std::string new_name = body.has("new_name") ? std::string(body["new_name"].s()) : "";
        if (!ParkingService::instance().updateSettings(P_name, fee, total, new_name))
            return BaseController::errorResponse(400, "更新失败");

        if (P_name.empty() || P_name == AppConfig::instance().parking_name) {
            AppConfig::instance().fee = fee;
            AppConfig::instance().capacity = total;
        }
        return BaseController::successResponse("更新成功");
        });

    CROW_ROUTE(app, "/api/parking/lot").methods("POST"_method)([](const crow::request& req) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::PARKING_SETTINGS))
        //     return BaseController::errorResponse(403, "权限不足");

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        std::string name = body["P_name"].s();
        int total = body["capacity"].i();
        double fee = body.has("fee") ? body["fee"].d() : 5.00;
        if (name.empty()) return BaseController::errorResponse(400, "请输入停车场名称");
        if (!ParkingService::instance().addLot(name, total, fee))
            return BaseController::errorResponse(400, "添加失败，名称可能重复");
        return BaseController::successResponse("停车场已添加");
        });

    CROW_ROUTE(app, "/api/parking/lot/<int>").methods("DELETE"_method)([](const crow::request& req, int id) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::PARKING_SETTINGS))
        //     return BaseController::errorResponse(403, "权限不足");

        std::string error;
        if (!ParkingService::instance().deleteLot(id, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("停车场已删除");
        });

    CROW_ROUTE(app, "/api/parking/billing-types").methods("GET"_method)([](const crow::request& req) {
        // 已注释登录验证
        // if (!BaseController::isAuthenticated(req))
        //     return BaseController::errorResponse(401, "请先登录");

        auto p = req.url_params.get("P_name");
        auto conn = MySQLPool::instance().getConnection();
        crow::json::wvalue res;
        std::vector<crow::json::wvalue> types;
        if (conn) {
            std::string sql = "SELECT id,rule_name,rule_type FROM BILLING_RULE WHERE is_active=1";
            if (p && p[0]) {
                char buf[512];
                mysql_real_escape_string(conn->get(), buf, p, (unsigned long)strlen(p));
                sql += std::string(" AND (P_name='") + buf + "' OR P_name='' OR P_name IS NULL)";
            }
            sql += " ORDER BY id";
            if (mysql_query(conn->get(), sql.c_str()) == 0) {
                MYSQL_RES* r = mysql_store_result(conn->get());
                if (r) {
                    MYSQL_ROW row;
                    while ((row = mysql_fetch_row(r))) {
                        crow::json::wvalue t;
                        t["id"] = std::stoi(row[0]);
                        t["rule_name"] = row[1] ? row[1] : "";
                        t["rule_type"] = row[2] ? row[2] : "";
                        types.push_back(std::move(t));
                    }
                    mysql_free_result(r);
                }
            }
        }
        res["types"] = std::move(types);
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/parking/billing-rules").methods("GET"_method)([](const crow::request& req) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::BILLING_VIEW))
        //     return BaseController::errorResponse(403, "权限不足");

        auto rules = BillingService::instance().getRules();
        crow::json::wvalue res;
        res["rules"] = BaseController::toJsonArray(rules);
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/parking/billing-rules").methods("POST"_method)([](const crow::request& req) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::BILLING_MANAGE))
        //     return BaseController::errorResponse(403, "权限不足");

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        BillingRule rule;
        rule.rule_name = body["rule_name"].s();
        rule.rule_type = body["rule_type"].s();
        rule.free_minutes = body["free_minutes"].i();
        rule.hourly_rate = body["hourly_rate"].d();
        rule.max_daily_fee = body.has("max_daily_fee") ? body["max_daily_fee"].d() : 0;
        rule.tier_config = body.has("tier_config") ? std::string(body["tier_config"].s()) : "";
        rule.description = body.has("description") ? std::string(body["description"].s()) : "";
        rule.is_active = body.has("is_active") ? body["is_active"].b() : true;
        rule.P_name = body.has("P_name") ? std::string(body["P_name"].s()) : "";
        if (!BillingService::instance().addRule(rule))
            return BaseController::errorResponse(400, "添加失败");
        return BaseController::successResponse("规则已添加");
        });

    CROW_ROUTE(app, "/api/parking/billing-rules/<int>").methods("DELETE"_method)([](const crow::request& req, int id) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::BILLING_MANAGE))
        //     return BaseController::errorResponse(403, "权限不足");

        if (!BillingService::instance().deleteRule(id))
            return BaseController::errorResponse(400, "删除失败");
        return BaseController::successResponse("删除成功");
        });

    CROW_ROUTE(app, "/api/parking/billing-rules/<int>").methods("PUT"_method)([](const crow::request& req, int id) {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::BILLING_MANAGE))
        //     return BaseController::errorResponse(403, "权限不足");

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        BillingRule rule;
        rule.rule_name = body["rule_name"].s();
        rule.rule_type = body["rule_type"].s();
        rule.free_minutes = body["free_minutes"].i();
        rule.hourly_rate = body["hourly_rate"].d();
        rule.max_daily_fee = body["max_daily_fee"].d();
        rule.tier_config = body.has("tier_config") ? std::string(body["tier_config"].s()) : std::string("");
        rule.description = body.has("description") ? std::string(body["description"].s()) : std::string("");
        rule.is_active = body["is_active"].b();
        rule.P_name = body.has("P_name") ? std::string(body["P_name"].s()) : "";
        if (!BillingService::instance().updateRule(id, rule))
            return BaseController::errorResponse(400, "更新失败");
        return BaseController::successResponse("更新成功");
        });

    CROW_ROUTE(app, "/api/parking/monthly-passes")
        ([](const crow::request& req) -> crow::response {
        if (req.method == crow::HTTPMethod::GET) {
            // 已注释登录验证
            // if (!BaseController::isAuthenticated(req))
            //     return BaseController::errorResponse(401, "请先登录");

            std::vector<MonthlyPass> passes;
            auto userIdParam = req.url_params.get("user_id");
            if (userIdParam) {
                int userId = std::stoi(userIdParam);
                passes = BillingService::instance().getMonthlyPasses(userId);
            } else {
                passes = BillingService::instance().getMonthlyPasses();
            }

            // Optional plate filter
            auto plateParam = req.url_params.get("plate");
            if (plateParam) {
                std::string filterPlate(plateParam);
                passes.erase(std::remove_if(passes.begin(), passes.end(),
                    [&filterPlate](const MonthlyPass& p) { return p.license_plate != filterPlate; }),
                    passes.end());
            }

            crow::json::wvalue res;
            res["passes"] = BaseController::toJsonArray(passes);
            return crow::response(res);
        }
        else if (req.method == crow::HTTPMethod::POST) {
            // 已注释登录验证
            // if (!BaseController::isAuthenticated(req))
            //     return BaseController::errorResponse(401, "请先登录");

            auto body = BaseController::parseBody(req);
            if (!body) return BaseController::errorResponse(400, "Invalid JSON");

            MonthlyPass pass;
            pass.license_plate = body["license_plate"].s();
            pass.pass_type = body["pass_type"].s();
            pass.start_date = body["start_date"].s();
            pass.end_date = body["end_date"].s();
            pass.fee = body["fee"].d();
            pass.user_id = 0;
            if (body.has("P_name")) pass.P_name = body["P_name"].s();

            if (!BillingService::instance().addMonthlyPass(pass))
                return BaseController::errorResponse(400, "添加失败");
            return BaseController::successResponse("添加成功");
        }
        return BaseController::errorResponse(405, "Method Not Allowed");
            });

    CROW_ROUTE(app, "/api/parking/monthly-passes/<int>").methods("PUT"_method)
        ([](const crow::request& req, int id) -> crow::response {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::BILLING_MANAGE))
        //     return BaseController::errorResponse(403, "权限不足");

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        MonthlyPass pass;
        pass.license_plate = body["license_plate"].s();
        pass.pass_type = body["pass_type"].s();
        pass.start_date = body["start_date"].s();
        pass.end_date = body["end_date"].s();
        pass.fee = body["fee"].d();
        if (body.has("P_name")) pass.P_name = body["P_name"].s();
        if (!BillingService::instance().updateMonthlyPass(id, pass))
            return BaseController::errorResponse(400, "更新失败");
        return BaseController::successResponse("更新成功");
            });

    CROW_ROUTE(app, "/api/parking/monthly-passes/<int>").methods("DELETE"_method)
        ([](const crow::request& req, int id) -> crow::response {
        // 已注释权限验证
        // if (!BaseController::checkPermission(req, Permissions::BILLING_MANAGE))
        //     return BaseController::errorResponse(403, "权限不足");

        if (!BillingService::instance().deleteMonthlyPass(id))
            return BaseController::errorResponse(400, "删除失败");
        return BaseController::successResponse("删除成功");
            });
}