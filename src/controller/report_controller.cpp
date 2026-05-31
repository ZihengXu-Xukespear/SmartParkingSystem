#include "report_controller.h"
#include "../service/report_service.h"
#include "../permissions.h"

std::string ReportController::getPrefix() const { return "/api/report"; }

void ReportController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/report/summary").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::REPORT_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto s = ReportService::instance().getSummary();
        crow::json::wvalue res;
        res["today_income"] = s.today_income;
        res["month_income"] = s.month_income;
        res["total_income"] = s.total_income;
        res["parking_fees"] = s.parking_fees;
        res["pass_sales"] = s.pass_sales;
        res["reservation_prepaid"] = s.reservation_prepaid;
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/report/daily").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::REPORT_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto data = ReportService::instance().getDailyRevenue(30);
        crow::json::wvalue res;
        std::vector<crow::json::wvalue> dates, values;
        for (auto& [d, v] : data) {
            dates.push_back(crow::json::wvalue(d));
            values.push_back(crow::json::wvalue(v));
        }
        res["dates"] = std::move(dates);
        res["values"] = std::move(values);
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/report/export").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::REPORT_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto start = req.url_params.get("start");
        auto end   = req.url_params.get("end");
        auto data = ReportService::instance().getDailyReport(
            start ? start : "", end ? end : "");

        std::string csv = "\xEF\xBB\xBF日期,收入,停车次数,套餐销售\n";
        for (auto& r : data) {
            csv += r.date + ",";
            csv += std::to_string(r.revenue) + ",";
            csv += std::to_string(r.parking_count) + ",";
            csv += std::to_string(r.pass_sales) + "\n";
        }

        crow::response resp(csv);
        resp.set_header("Content-Type", "text/csv; charset=utf-8");
        resp.set_header("Content-Disposition",
            "attachment; filename=report.csv");
        return resp;
    });

    CROW_ROUTE(app, "/api/report/prediction").methods("GET"_method)([](const crow::request& req) {
        if (!BaseController::checkPermission(req, Permissions::REPORT_VIEW))
            return BaseController::errorResponse(403, "权限不足");
        auto p = ReportService::instance().getRevenuePrediction();
        crow::json::wvalue res;
        res["daily_average"] = p.daily_average;
        res["predicted_monthly"] = p.predicted_monthly;
        res["days_remaining"] = p.days_remaining;
        res["days_in_month"] = p.days_in_month;
        return crow::response(res);
    });
}
