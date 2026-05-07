#include "reservation_controller.h"
#include "../service/reservation_service.h"
#include "../config.h"
#include "../permissions.h"

std::string ReservationController::getPrefix() const { return "/api/reservation"; }

void ReservationController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/reservation/create").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::RESERVATION_CREATE))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string plate = body["license_plate"].s();
        std::string P_name = body.has("P_name") ? body["P_name"].s() : AppConfig::instance().parking_name;

        std::string error;
        if (!ReservationService::instance().create(plate, P_name, auth.first, error))
            return BaseController::errorResponse(400, error);

        int expire_min = AppConfig::instance().notice_expire_minutes;
        return BaseController::successResponse("预约成功，请在" + std::to_string(expire_min) + "分钟内到达");
    });

    CROW_ROUTE(app, "/api/reservation/list").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::RESERVATION_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto reservations = ReservationService::instance().list();
        crow::json::wvalue res;
        res["reservations"] = BaseController::toJsonArray(reservations);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/reservation/<int>").methods("DELETE"_method)([](const crow::request& req, int id) {
        if (!BaseController::checkPermission(req, Permissions::RESERVATION_CANCEL))
            return BaseController::errorResponse(403, "权限不足");
        if (!ReservationService::instance().cancel(id))
            return BaseController::errorResponse(400, "取消失败");
        return BaseController::successResponse("取消成功（预付不退还）");
    });

    CROW_ROUTE(app, "/api/reservation/history").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::RESERVATION_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto start = req.url_params.get("start");
        auto end   = req.url_params.get("end");
        auto history = ReservationService::instance().getHistory(
            start ? start : "", end ? end : "", 200, 0);
        crow::json::wvalue res;
        res["reservations"] = BaseController::toJsonArray(history);
        return crow::response(res);
    });
}
