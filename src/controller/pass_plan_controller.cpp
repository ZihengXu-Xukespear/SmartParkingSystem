#include "pass_plan_controller.h"
#include "../service/pass_plan_service.h"
#include "../permissions.h"

std::string PassPlanController::getPrefix() const { return "/api/pass-plans"; }

void PassPlanController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/pass-plans").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto p = req.url_params.get("P_name");
        auto plans = PassPlanService::instance().getActivePlans(p ? p : "");
        crow::json::wvalue res;
        res["plans"] = BaseController::toJsonArray(plans);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/pass-plans").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::PASS_PLAN_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        PassPlan plan;
        plan.plan_name = body["plan_name"].s();
        plan.duration_days = body["duration_days"].i();
        plan.price = body["price"].d();
        plan.description = body.has("description") ? std::string(body["description"].s()) : "";
        plan.P_name = body.has("P_name") ? std::string(body["P_name"].s()) : "";
        if (!PassPlanService::instance().addPlan(plan))
            return BaseController::errorResponse(400, "添加失败");
        return BaseController::successResponse("套餐已添加");
    });

    CROW_ROUTE(app, "/api/pass-plans/<int>").methods("PUT"_method)([](const crow::request& req, int id) {
        if (!BaseController::checkPermission(req, Permissions::PASS_PLAN_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        PassPlan plan;
        plan.plan_name = body["plan_name"].s();
        plan.duration_days = body["duration_days"].i();
        plan.price = body["price"].d();
        plan.description = body.has("description") ? std::string(body["description"].s()) : "";
        plan.is_active = body["is_active"].b();
        plan.P_name = body.has("P_name") ? std::string(body["P_name"].s()) : "";
        if (!PassPlanService::instance().updatePlan(id, plan))
            return BaseController::errorResponse(400, "更新失败");
        return BaseController::successResponse("套餐已更新");
    });

    CROW_ROUTE(app, "/api/pass-plans/<int>").methods("DELETE"_method)([](const crow::request& req, int id) {
        if (!BaseController::checkPermission(req, Permissions::PASS_PLAN_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        if (!PassPlanService::instance().deletePlan(id))
            return BaseController::errorResponse(400, "删除失败");
        return BaseController::successResponse("套餐已删除");
    });

    CROW_ROUTE(app, "/api/pass-plans/<int>/purchase").methods("POST"_method)([](const crow::request& req, int planId) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto auth = BaseController::authenticate(req);
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        std::string plate = body["license_plate"].s();
        if (plate.empty()) return BaseController::errorResponse(400, "请输入车牌号");

        std::string error;
        if (!PassPlanService::instance().purchase(auth.first, planId, plate, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("月卡购买成功");
    });

}
