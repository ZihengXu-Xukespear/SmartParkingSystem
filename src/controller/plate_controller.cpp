#include "plate_controller.h"
#include "../service/plate_service.h"
#include "../permissions.h"

std::string PlateController::getPrefix() const { return "/api/plate"; }

void PlateController::registerRoutes(crow::SimpleApp& app) {
    // Original stub endpoint (kept for backward compatibility)
    CROW_ROUTE(app, "/api/plate/recognize").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::PLATE_RECOGNIZE))
            return BaseController::errorResponse(403, "权限不足");
        auto result = PlateService::instance().recognize(req.body);

        crow::json::wvalue res;
        res["plate_number"] = result.plate_number;
        res["confidence"] = result.confidence;
        res["color"] = result.color;
        res["message"] = result.message;
        crow::response cres(res);
        cres.set_header("Content-Type", "application/json; charset=utf-8");
        return cres;
    });

    // New: Recognize plate from camera image (base64) and check registration
    CROW_ROUTE(app, "/api/plate/recognize-image").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::PLATE_RECOGNIZE))
            return BaseController::errorResponse(403, "权限不足");

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string image_data = body["image"].s();
        if (image_data.empty())
            return BaseController::errorResponse(400, "图片数据为空");

        // Strip data URL prefix if present (e.g., "data:image/jpeg;base64,...")
        size_t comma_pos = image_data.find(',');
        if (comma_pos != std::string::npos) {
            // Validate prefix
            std::string prefix = image_data.substr(0, comma_pos);
            if (prefix.find("base64") != std::string::npos) {
                image_data = image_data.substr(comma_pos + 1);
            }
        }

        // Step 1: Recognize plate
        auto& ps = PlateService::instance();
        auto plate_result = ps.recognize(image_data);

        crow::json::wvalue res;
        res["plate_number"] = plate_result.plate_number;
        res["confidence"] = plate_result.confidence;
        res["color"] = plate_result.color;
        res["recognize_message"] = plate_result.message;
        res["_debug_raw_hex"] = ""; // will be filled if available

        // Step 2: If plate was recognized, check registration
        if (!plate_result.plate_number.empty()) {
            auto reg_info = ps.checkRegistration(plate_result.plate_number);

            res["registration"] = crow::json::wvalue();
            res["registration"]["is_registered"] = reg_info.is_registered;
            res["registration"]["in_parking"] = reg_info.in_parking;
            res["registration"]["has_monthly_pass"] = reg_info.has_monthly_pass;
            res["registration"]["is_blacklisted"] = reg_info.is_blacklisted;
            res["registration"]["last_check_in"] = reg_info.last_check_in;
            res["registration"]["monthly_pass_end"] = reg_info.monthly_pass_end;
            res["registration"]["blacklist_reason"] = reg_info.blacklist_reason;
            res["registration"]["message"] = reg_info.message;
        }

        crow::response cres(res);
        cres.set_header("Content-Type", "application/json; charset=utf-8");
        return cres;
    });

    // New: Check if a plate is registered (manual entry)
    CROW_ROUTE(app, "/api/plate/check-registered").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");

        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string plate = body["license_plate"].s();
        if (plate.empty())
            return BaseController::errorResponse(400, "请输入车牌号");

        auto reg_info = PlateService::instance().checkRegistration(plate);

        crow::json::wvalue res;
        res["plate_number"] = reg_info.plate_number;
        res["is_registered"] = reg_info.is_registered;
        res["in_parking"] = reg_info.in_parking;
        res["has_monthly_pass"] = reg_info.has_monthly_pass;
        res["is_blacklisted"] = reg_info.is_blacklisted;
        res["last_check_in"] = reg_info.last_check_in;
        res["monthly_pass_end"] = reg_info.monthly_pass_end;
        res["blacklist_reason"] = reg_info.blacklist_reason;
        res["message"] = reg_info.message;

        return crow::response(res);
    });

    // Keep original validate endpoint
    CROW_ROUTE(app, "/api/plate/validate").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string plate = body["license_plate"].s();
        bool valid = PlateService::validatePlate(plate);

        crow::json::wvalue res;
        res["valid"] = valid;
        res["plate"] = plate;
        if (!valid) res["message"] = "车牌号格式不正确，应为：省份汉字+字母+5-6位数字/字母";
        return crow::response(res);
    });
}
