#include "balance_controller.h"
#include "../service/balance_service.h"
#include "../permissions.h"
#include "../service/billing_service.h"
#include <crow.h>


std::string BalanceController::getPrefix() const { return "/api/balance"; }

void BalanceController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/balance").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::BALANCE_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        int userId = auth.first;

        double balance = BalanceService::instance().getBalance(userId);
        auto transactions = BalanceService::instance().getTransactions(userId, 20);

        crow::json::wvalue res;
        res["balance"] = balance;
        res["transactions"] = BaseController::toJsonArray(transactions);
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/balance/<int>").methods("GET"_method)([](const crow::request& req, int userId) {
        if (!BaseController::checkPermission(req, Permissions::BALANCE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");

        double balance = BalanceService::instance().getBalance(userId);
        auto transactions = BalanceService::instance().getTransactions(userId, 50);

        crow::json::wvalue res;
        res["balance"] = balance;
        res["transactions"] = BaseController::toJsonArray(transactions);
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/balance/recharge").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::BALANCE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        int userId = body["user_id"].i();
        double amount = body["amount"].d();
        std::string desc = body.has("description") ? std::string(body["description"].s()) : "管理员充值";

        std::string error;
        if (!BalanceService::instance().recharge(userId, amount, desc, error))
            return BaseController::errorResponse(400, error);

        double newBal = BalanceService::instance().getBalance(userId);
        crow::json::wvalue res;
        res["message"] = "充值成功";
        res["balance"] = newBal;
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/balance/deposit").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto auth = BaseController::authenticate(req);
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        double amount = body["amount"].d();
        if (amount < 1.0) return BaseController::errorResponse(400, "充值金额至少1元");
        if (amount > 10000) return BaseController::errorResponse(400, "单次充值最多10000元");

        std::string error;
        if (!BalanceService::instance().recharge(auth.first, amount, "自助充值", error))
            return BaseController::errorResponse(400, error);

        crow::json::wvalue res;
        res["message"] = "充值成功";
        res["balance"] = BalanceService::instance().getBalance(auth.first);
        return crow::response(res);
        });

    CROW_ROUTE(app, "/api/balance/transactions").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::BALANCE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto transactions = BalanceService::instance().getTransactions(0, 200);
        crow::json::wvalue res;
        res["transactions"] = BaseController::toJsonArray(transactions);
        return crow::response(res);
        });

    // 接口：计算停车费用（用于前端预计算）
    CROW_ROUTE(app, "/api/balance/calculate-fee")
        .methods(crow::HTTPMethod::POST)
        ([](const crow::request& req) {
        // 权限校验：管理员/系统权限
        if (!BaseController::checkPermission(req, Permissions::BALANCE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");

        // 解析请求体
        auto body = BaseController::parseBody(req);
        if (!body)
            return BaseController::errorResponse(400, "Invalid JSON");

        // 获取参数：入场时间、离场时间（Unix时间戳）
        if (!body.has("in_time") || !body.has("out_time"))
            return BaseController::errorResponse(400, "缺少必要参数：in_time/out_time");

        time_t inTime = static_cast<time_t>(body["in_time"].i());
        time_t outTime = static_cast<time_t>(body["out_time"].i());

        // 调用计费服务计算费用
        std::string ruleInfo;
        double fee = BillingService::instance().calculateParkingFee(inTime, outTime, ruleInfo);

        // 返回结果
        crow::json::wvalue res;
        res["code"] = 0;
        res["fee"] = fee;
        res["rule"] = ruleInfo;
        res["in_time"] = static_cast<int64_t>(inTime);
        res["out_time"] = static_cast<int64_t>(outTime);
        return crow::response(res);
            });

    // 接口：用户离场停车扣费（任务D核心接口）
    CROW_ROUTE(app, "/api/balance/parking-deduct")
        .methods(crow::HTTPMethod::POST)
        ([](const crow::request& req) {
        // 权限校验：必须登录
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto auth = BaseController::authenticate(req);
        int userId = auth.first;

        // 解析请求体
        auto body = BaseController::parseBody(req);
        if (!body)
            return BaseController::errorResponse(400, "Invalid JSON");

        // 获取参数：入场时间、离场时间、车牌
        if (!body.has("in_time") || !body.has("out_time") || !body.has("license_plate"))
            return BaseController::errorResponse(400, "缺少必要参数：in_time/out_time/license_plate");

        time_t inTime = static_cast<time_t>(body["in_time"].i());
        time_t outTime = static_cast<time_t>(body["out_time"].i());
        std::string plate = body["license_plate"].s();

        // 1. 先检查月卡是否有效
        std::string passInfo;
        bool hasValidPass = BillingService::instance().checkMonthlyPassValid(userId, plate, passInfo);
        if (hasValidPass) {
            // 有有效月卡，免扣费，直接返回成功
            crow::json::wvalue res;
            res["code"] = 0;
            res["message"] = "月卡用户，免停车费";
            res["pass_info"] = passInfo;
            res["fee"] = 0.0;
            res["balance"] = BalanceService::instance().getBalance(userId);
            return crow::response(res);
        }

        // 2. 无月卡，计算停车费用
        std::string ruleInfo;
        double fee = BillingService::instance().calculateParkingFee(inTime, outTime, ruleInfo);
        if (fee <= 0) {
            return BaseController::errorResponse(400, "费用计算异常");
        }

        // 3. 检查余额是否足够
        if (!BalanceService::instance().isBalanceEnough(userId, fee)) {
            double currentBal = BalanceService::instance().getBalance(userId);
            crow::json::wvalue res;
            res["code"] = -1;
            res["message"] = "余额不足";
            res["required_fee"] = fee;
            res["current_balance"] = currentBal;
            return crow::response(400, res);
        }

        // 4. 执行扣费
        std::string error;
        bool deductSuccess = BalanceService::instance().parkingDeduct(
            userId,
            inTime,
            outTime,
            BillingService::instance().getActiveRule().hourly_rate,
            error
        );
        if (!deductSuccess) {
            return BaseController::errorResponse(400, error);
        }

        // 5. 扣费成功，返回新余额和费用信息
        double newBalance = BalanceService::instance().getBalance(userId);
        crow::json::wvalue res;
        res["code"] = 0;
        res["message"] = "扣费成功";
        res["fee"] = fee;
        res["rule"] = ruleInfo;
        res["balance"] = newBalance;
        return crow::response(res);
            });

    // 接口：用户余额查询（对外简化接口）
    CROW_ROUTE(app, "/api/balance/my")
        .methods(crow::HTTPMethod::GET)
        ([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto auth = BaseController::authenticate(req);
        int userId = auth.first;

        double balance = BalanceService::instance().getUserBalance(userId);
        crow::json::wvalue res;
        res["code"] = 0;
        res["balance"] = balance;
        return crow::response(res);
            });
}