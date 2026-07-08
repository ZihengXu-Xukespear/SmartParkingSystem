#include "customer_service_controller.h"
#include "../service/customer_service_service.h"
#include "../permissions.h"

std::string CustomerServiceController::getPrefix() const { return "/api/cs"; }

void CustomerServiceController::registerRoutes(crow::SimpleApp& app) {

    // ---- user: send a message (AI answers while status=ai, else routed to human) ----
    CROW_ROUTE(app, "/api/cs/send").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_SEND))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        if (auth.first == -1) return BaseController::errorResponse(401, "未登录");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        std::string message = body.has("message") ? std::string(body["message"].s()) : "";

        AskResult ar = CustomerServiceService::instance().ask(auth.first, message);
        if (!ar.ok) return BaseController::errorResponse(400, ar.error);

        crow::json::wvalue res;
        res["mode"] = ar.mode;
        res["reply"] = ar.reply;
        res["show_actions"] = ar.show_actions;
        res["session_id"] = ar.session_id;
        res["status"] = ar.status;
        crow::response r(res);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
    });

    // ---- user: transfer to a human agent ----
    CROW_ROUTE(app, "/api/cs/escalate").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_SEND))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        if (auth.first == -1) return BaseController::errorResponse(401, "未登录");
        std::string error;
        if (!CustomerServiceService::instance().escalate(auth.first, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("已转接人工客服");
    });

    // ---- user: load active session + messages + status ----
    CROW_ROUTE(app, "/api/cs/session").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_SEND))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        if (auth.first == -1) return BaseController::errorResponse(401, "未登录");
        auto sess = CustomerServiceService::instance().getSession(auth.first);
        crow::response r(sess);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
    });

    // ---- admin: list all conversations ----
    CROW_ROUTE(app, "/api/cs/admin/sessions").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto statusParam = req.url_params.get("status");
        std::string filter = statusParam ? statusParam : "";
        auto res = CustomerServiceService::instance().adminListSessions(filter);
        crow::response r(res);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
    });

    // ---- admin: open one conversation (marks user messages read) ----
    CROW_ROUTE(app, "/api/cs/admin/session").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto idParam = req.url_params.get("id");
        int sid = idParam ? std::stoi(idParam) : 0;
        auto res = CustomerServiceService::instance().adminGetSession(sid);
        crow::response r(res);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
    });

    // ---- admin: reply (takes over the session) ----
    CROW_ROUTE(app, "/api/cs/admin/reply").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        if (auth.first == -1) return BaseController::errorResponse(401, "未登录");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        int sid = body.has("session_id") ? body["session_id"].i() : 0;
        std::string content = body.has("content") ? std::string(body["content"].s()) : "";
        if (sid <= 0) return BaseController::errorResponse(400, "缺少会话ID");
        if (content.empty()) return BaseController::errorResponse(400, "回复内容不能为空");
        std::string error;
        if (!CustomerServiceService::instance().adminReply(auth.first, sid, content, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("回复成功");
    });

    // ---- admin: close a conversation ----
    CROW_ROUTE(app, "/api/cs/admin/close").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");
        int sid = body.has("session_id") ? body["session_id"].i() : 0;
        if (sid <= 0) return BaseController::errorResponse(400, "缺少会话ID");
        std::string error;
        if (!CustomerServiceService::instance().adminClose(sid, error))
            return BaseController::errorResponse(400, error);
        return BaseController::successResponse("已关闭");
    });

    // ---- admin: pending/unread counts (for the sidebar badge) ----
    CROW_ROUTE(app, "/api/cs/admin/pending").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::MESSAGE_MANAGE))
            return BaseController::errorResponse(403, "权限不足");
        crow::json::wvalue res;
        res["pending"] = CustomerServiceService::instance().adminPendingCount();
        res["unread"] = CustomerServiceService::instance().adminUnreadCount();
        crow::response r(res);
        r.set_header("Content-Type", "application/json; charset=utf-8");
        return r;
    });
}
