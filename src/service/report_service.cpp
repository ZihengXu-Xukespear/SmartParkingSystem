#include "report_service.h"

ReportService& ReportService::instance() {
    static ReportService inst;
    return inst;
}

ReportService::RevenueSummary ReportService::getSummary() {
    RevenueSummary s;
    auto conn = getConnection();
    if (!conn) return s;
    MYSQL* mysql = conn->get();

    auto queryPeriod = [&](const std::string& where_clause, double& total) {
        std::string sql = "SELECT type, COALESCE(SUM(ABS(amount)),0) FROM BALANCE_RECORD "
            "WHERE amount<0 " + where_clause + " GROUP BY type";
        if (mysql_query(mysql, sql.c_str()) != 0) return;
        MYSQL_RES* res = mysql_store_result(mysql);
        if (!res) return;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            std::string type = row[0] ? row[0] : "";
            double val = row[1] ? std::stod(row[1]) : 0;
            total += val;
            if (type == "checkout") s.parking_fees += val;
            else if (type == "purchase") s.pass_sales += val;
            else if (type == "reservation") s.reservation_prepaid += val;
        }
        mysql_free_result(res);
    };

    queryPeriod("AND DATE(created_at)=CURDATE()", s.today_income);
    queryPeriod("AND YEAR(created_at)=YEAR(CURDATE()) AND MONTH(created_at)=MONTH(CURDATE())", s.month_income);
    queryPeriod("", s.total_income);

    return s;
}

std::vector<std::pair<std::string, double>> ReportService::getDailyRevenue(int days) {
    std::vector<std::pair<std::string, double>> data;
    auto conn = getConnection();
    if (!conn) return data;
    MYSQL* mysql = conn->get();

    std::string sql = "SELECT DATE(created_at) as d, COALESCE(SUM(ABS(amount)),0) FROM BALANCE_RECORD "
        "WHERE amount<0 AND created_at >= DATE_SUB(CURDATE(), INTERVAL " + std::to_string(days) + " DAY) "
        "GROUP BY DATE(created_at) ORDER BY d";
    if (mysql_query(mysql, sql.c_str()) != 0) return data;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return data;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        data.push_back({row[0] ? row[0] : "", row[1] ? std::stod(row[1]) : 0});
    }
    mysql_free_result(res);
    return data;
}

std::vector<ReportService::DailyReportRow> ReportService::getDailyReport(const std::string& startDate, const std::string& endDate) {
    std::vector<DailyReportRow> data;
    auto conn = getConnection();
    if (!conn) return data;
    MYSQL* mysql = conn->get();

    std::string sql =
        "SELECT d, COALESCE(revenue,0), COALESCE(parking_count,0), COALESCE(pass_sales,0) FROM ("
        "SELECT DATE(b.created_at) as d, SUM(ABS(b.amount)) as revenue "
        "FROM BALANCE_RECORD b WHERE b.amount<0 ";
    if (!startDate.empty()) sql += "AND DATE(b.created_at) >= '" + startDate + "' ";
    if (!endDate.empty()) sql += "AND DATE(b.created_at) <= '" + endDate + "' ";
    sql += "GROUP BY DATE(b.created_at)"
        ") rev LEFT JOIN ("
        "SELECT DATE(check_in_time) as d2, COUNT(*) as parking_count "
        "FROM CAR_RECORD ";
    if (!startDate.empty()) sql += "WHERE DATE(check_in_time) >= '" + startDate + "' ";
    if (!endDate.empty()) {
        sql += (startDate.empty() ? "WHERE " : "AND ");
        sql += "DATE(check_in_time) <= '" + endDate + "' ";
    }
    sql += "GROUP BY DATE(check_in_time)"
        ") car ON rev.d = car.d2 LEFT JOIN ("
        "SELECT DATE(created_at) as d3, SUM(ABS(amount)) as pass_sales "
        "FROM BALANCE_RECORD WHERE amount<0 AND type='purchase' ";
    if (!startDate.empty()) sql += "AND DATE(created_at) >= '" + startDate + "' ";
    if (!endDate.empty()) sql += "AND DATE(created_at) <= '" + endDate + "' ";
    sql += "GROUP BY DATE(created_at)"
        ") ps ON rev.d = ps.d3 ORDER BY d";

    if (mysql_query(mysql, sql.c_str()) != 0) return data;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return data;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        DailyReportRow r;
        r.date = row[0] ? row[0] : "";
        r.revenue = row[1] ? std::stod(row[1]) : 0;
        r.parking_count = row[2] ? std::stoi(row[2]) : 0;
        r.pass_sales = row[3] ? std::stod(row[3]) : 0;
        data.push_back(r);
    }
    mysql_free_result(res);
    return data;
}

ReportService::RevenuePrediction ReportService::getRevenuePrediction() {
    RevenuePrediction p;
    auto conn = getConnection();
    if (!conn) return p;
    MYSQL* mysql = conn->get();

    std::string sql = "SELECT COALESCE(AVG(daily_total),0) FROM ("
        "SELECT DATE(created_at) as d, SUM(ABS(amount)) as daily_total "
        "FROM BALANCE_RECORD WHERE amount<0 "
        "AND created_at >= DATE_SUB(CURDATE(), INTERVAL 30 DAY) "
        "GROUP BY DATE(created_at)) t";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) p.daily_average = std::stod(row[0]);
            mysql_free_result(res);
        }
    }

    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        daysInMonth[1] = 29;
    p.days_in_month = daysInMonth[month - 1];
    p.days_remaining = p.days_in_month - day;
    p.predicted_monthly = p.daily_average * p.days_in_month;
    return p;
}
