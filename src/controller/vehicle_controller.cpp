#include "vehicle_controller.h"
#include "../service/vehicle_service.h"
#include "../permissions.h"

std::string VehicleController::getPrefix() const { return "/api/vehicle"; }

void VehicleController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/vehicle/checkin").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::VEHICLE_CHECKIN))
            return BaseController::errorResponse(403, "权限不足");
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string plate = body["license_plate"].s();
        std::string billing_type = "standard";
        if (body.has("billing_type")) billing_type = body["billing_type"].s();
        std::string P_name = body.has("P_name") ? std::string(body["P_name"].s()) : "";
        int spotNum = body.has("spot_number") ? body["spot_number"].i() : 0;

        std::string error;
        if (!VehicleService::instance().checkIn(plate, billing_type, P_name, spotNum, error))
            return BaseController::errorResponse(400, error);

        crow::json::wvalue res;
        res["message"] = "车辆入库成功";
        res["license_plate"] = plate;
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/vehicle/checkout").methods("POST"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::VEHICLE_CHECKOUT))
            return BaseController::errorResponse(403, "权限不足");
        auto auth = BaseController::authenticate(req);
        auto body = BaseController::parseBody(req);
        if (!body) return BaseController::errorResponse(400, "Invalid JSON");

        std::string plate = body["license_plate"].s();
        double fee = 0;
        CarRecord record;
        std::string error;

        if (!VehicleService::instance().checkOut(plate, auth.first, fee, record, error))
            return BaseController::errorResponse(400, error);

        crow::json::wvalue res;
        res["message"] = "车辆出库成功";
        res["fee"] = fee;
        res["record"] = record.serialize();
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/vehicle/query").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::VEHICLE_QUERY))
            return BaseController::errorResponse(403, "权限不足");
        std::string plate = req.url_params.get("plate") ? req.url_params.get("plate") : "";
        std::string start = req.url_params.get("start") ? req.url_params.get("start") : "";
        std::string end = req.url_params.get("end") ? req.url_params.get("end") : "";

        auto records = VehicleService::instance().queryRecords(plate, start, end);
        crow::json::wvalue res;
        res["records"] = BaseController::toJsonArray(records);
        res["total"] = (int)records.size();
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/vehicle/parked").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::VEHICLE_QUERY))
            return BaseController::errorResponse(403, "权限不足");
        std::string plate = req.url_params.get("plate") ? req.url_params.get("plate") : "";
        auto records = VehicleService::instance().getParkedVehicles(plate);
        crow::json::wvalue res;
        res["records"] = BaseController::toJsonArray(records);
        res["total"] = (int)records.size();
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/vehicle/status").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::isAuthenticated(req))
            return BaseController::errorResponse(401, "请先登录");
        auto conn = MySQLPool::instance().getConnection();
        crow::json::wvalue res;
        std::vector<crow::json::wvalue> arr;
        if (conn) {
            const char* sql =
                "SELECT license_plate, MAX(check_in_time) as last_in, "
                "SUM(CASE WHEN check_out_time IS NULL THEN 1 ELSE 0 END) as parked_count, "
                "COUNT(*) as total_records "
                "FROM CAR_RECORD GROUP BY license_plate ORDER BY parked_count DESC, last_in DESC";
            if (mysql_query(conn->get(), sql) == 0) {
                MYSQL_RES* r = mysql_store_result(conn->get());
                if (r) {
                    MYSQL_ROW row;
                    while ((row = mysql_fetch_row(r))) {
                        crow::json::wvalue v;
                        v["license_plate"] = row[0] ? row[0] : "";
                        v["last_check_in"] = row[1] ? row[1] : "";
                        v["is_parked"] = row[2] ? std::stoi(row[2]) > 0 : false;
                        v["total_records"] = row[3] ? std::stoi(row[3]) : 0;
                        arr.push_back(std::move(v));
                    }
                    mysql_free_result(r);
                }
            }
        }
        res["vehicles"] = std::move(arr);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/vehicle/<int>").methods("DELETE"_method)([](const crow::request& req, int id) {
        if (!BaseController::checkPermission(req, Permissions::VEHICLE_DELETE))
            return BaseController::errorResponse(403, "权限不足");
        if (!VehicleService::instance().deleteRecord(id))
            return BaseController::errorResponse(400, "删除失败");
        return BaseController::successResponse("删除成功");
    });

    CROW_ROUTE(app, "/api/vehicle/export").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::EXPORT_DATA))
            return BaseController::errorResponse(403, "权限不足");
        std::string plate = req.url_params.get("plate") ? req.url_params.get("plate") : "";
        std::string start = req.url_params.get("start") ? req.url_params.get("start") : "";
        std::string end = req.url_params.get("end") ? req.url_params.get("end") : "";

        auto records = VehicleService::instance().queryRecords(plate, start, end);

        auto csvEsc = [](const std::string& s) {
            std::string out = s;
            size_t p = 0;
            while ((p = out.find('"', p)) != std::string::npos) { out.replace(p, 1, "\"\""); p += 2; }
            return "\"" + out + "\"";
        };
        std::string csv = "ID,车牌号,入库时间,出库时间,费用,停车场,计费方式\n";
        for (auto& r : records) {
            csv += std::to_string(r.id) + "," + csvEsc(r.license_plate) + "," +
                   csvEsc(r.check_in_time) + "," + csvEsc(r.check_out_time.empty() ? "-" : r.check_out_time) + "," +
                   std::to_string(r.fee) + "," + csvEsc(r.location) + "," + csvEsc(r.billing_type) + "\n";
        }
        crow::response res(csv);
        res.set_header("Content-Type", "text/csv; charset=utf-8");
        res.set_header("Content-Disposition", "attachment; filename=parking_records.csv");
        return res;
    });
}
