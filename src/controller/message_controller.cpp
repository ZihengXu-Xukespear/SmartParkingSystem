#include "message_controller.h"
#include "../service/message_service.h"
#include "../permissions.h"

std::string MessageController::getPrefix() const { return "/api/message"; }

void MessageController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/message/send").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_SEND))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string content = body.has("content") ? std::string(body["content"].s()) : "";
        int receiverId = body.has("receiver_id") ? body["receiver_id"].i() : 0;
        if (content.empty()) return BaseController::errorResponse(400, "消息内容不能为空");

        std::string error;
        if (!MessageService::instance().sendMessage(auth.first, receiverId, content, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("发送成功");
    });

    CROW_ROUTE(app, "/api/message/conversations").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto result = MessageService::instance().getConversations();
        crow::response r(result);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
    });

    CROW_ROUTE(app, "/api/message/history").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_SEND))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        auto userIdParam = req.url_params.get("user_id");
        int otherUserId = userIdParam ? std::stoi(userIdParam) : 0;

        auto messages = MessageService::instance().getHistory(auth.first, otherUserId);
        MessageService::instance().markAsRead(auth.first, otherUserId);

        crow::json::wvalue res;
        res["messages"] = BaseController::toJsonArray(messages);
        crow::response r(res);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
    });

    CROW_ROUTE(app, "/api/message/mark-read").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_SEND))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        int userId = body.has("user_id") ? body["user_id"].i() : 0;
        MessageService::instance().markAsRead(auth.first, userId);
        return BaseController::successResponse("已标记为已读");
    });

    CROW_ROUTE(app, "/api/message/unread-count").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_SEND))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        int count = MessageService::instance().getUnreadCount(auth.first);
        crow::json::wvalue res;
        res["count"] = count;
        return crow::response(res);
    });
}
