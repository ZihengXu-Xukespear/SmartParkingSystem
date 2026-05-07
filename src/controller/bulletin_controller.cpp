#include "bulletin_controller.h"
#include "../service/bulletin_service.h"
#include "../config.h"
#include "../permissions.h"

std::string BulletinController::getPrefix() const { return "/api/bulletin"; }

void BulletinController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/bulletin").methods("GET"_method)([](const crow::request& req) {
        auto auth = BaseController::authenticate(req);
        if (auth.first == -1) return BaseController::errorResponse(401, "未登录");
        auto bulletins = BulletinService::instance().getActive();
        crow::json::wvalue res;
        std::vector<crow::json::wvalue> arr;
        for (auto& b : bulletins) arr.push_back(b.serialize());
        res["bulletins"] = std::move(arr);
        res["notice"] = bulletins.empty() ? AppConfig::instance().notice : bulletins[0].content;
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/bulletin/all").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::NOTICE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto bulletins = BulletinService::instance().getAll();
        crow::json::wvalue res;
        std::vector<crow::json::wvalue> arr;
        for (auto& b : bulletins) arr.push_back(b.serialize());
        res["bulletins"] = std::move(arr);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/bulletin").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::NOTICE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        std::string content = body["content"].s();
        bool isPinned = body.has("is_pinned") ? static_cast<bool>(body["is_pinned"].b()) : false;
        std::string validFrom = body.has("valid_from") ? std::string(body["valid_from"].s()) : "";
        std::string validUntil = body.has("valid_until") ? std::string(body["valid_until"].s()) : "";
        std::string error;
        if (!BulletinService::instance().create(content, isPinned, validFrom, validUntil, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("公告已创建");
    });

    CROW_ROUTE(app, "/api/bulletin/<int>").methods("PUT"_method)([](const crow::request& req, int id) {
        if (!BaseController::checkPermission(req, Permissions::NOTICE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        std::string content = body["content"].s();
        bool isPinned = body.has("is_pinned") ? static_cast<bool>(body["is_pinned"].b()) : false;
        std::string validFrom = body.has("valid_from") ? std::string(body["valid_from"].s()) : "";
        std::string validUntil = body.has("valid_until") ? std::string(body["valid_until"].s()) : "";
        std::string error;
        if (!BulletinService::instance().update(id, content, isPinned, validFrom, validUntil, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("公告已更新");
    });

    CROW_ROUTE(app, "/api/bulletin/<int>").methods("DELETE"_method)([](const crow::request& req, int id) {
        if (!BaseController::checkPermission(req, Permissions::NOTICE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        if (!BulletinService::instance().remove(id))
            return BaseController::errorResponse(400, "删除失败");
        return BaseController::successResponse("公告已删除");
    });
}
