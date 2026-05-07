#include "reservation_service.h"
#include "../config.h"
#include "balance_service.h"

ReservationService& ReservationService::instance() {
    static ReservationService inst;
    return inst;
}

bool ReservationService::create(const std::string& plate, const std::string& P_name, int userId, std::string& error) {
    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();
    Transaction tx(mysql);

    std::string sql = "SELECT id FROM RESERVATION WHERE license_plate=" + quote(mysql, plate) + " AND status='active'";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res && mysql_num_rows(res) > 0) {
            mysql_free_result(res);
            error = "该车牌已有预约";
            return false;
        }
        if (res) mysql_free_result(res);
    }

    sql = "SELECT P_total_count, P_current_count, P_reserve_count, P_fee FROM PARKING_LOT WHERE P_name=" +
        quote(mysql, P_name);
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "查询停车场失败"; return false; }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res || mysql_num_rows(res) == 0) { error = "停车场不存在"; return false; }
    MYSQL_ROW row = mysql_fetch_row(res);
    int total = std::stoi(row[0]), current = std::stoi(row[1]), reserve = std::stoi(row[2]);
    double prepaidFee = row[3] ? std::stod(row[3]) : 5.00;
    mysql_free_result(res);

    if (current + reserve >= total) { error = "停车场已满，无法预约"; return false; }

    bool hasPass = false;
    sql = "SELECT COUNT(*) FROM MONTHLY_PASS WHERE license_plate=" + quote(mysql, plate) +
        " AND is_active=1 AND start_date <= CURDATE() AND end_date >= CURDATE()";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* pr = mysql_store_result(mysql);
        if (pr) { MYSQL_ROW prow = mysql_fetch_row(pr); hasPass = (prow && std::stoi(prow[0]) > 0); mysql_free_result(pr); }
    }

    if (hasPass) {
        prepaidFee = 0;
    } else {
        std::string deductErr;
        if (!BalanceService::instance().deduct(userId, prepaidFee, "reservation",
            "预约预付 " + plate + " (首小时费用)", deductErr)) {
            error = deductErr;
            return false;
        }
    }

    sql = "INSERT INTO RESERVATION (license_plate, P_name, prepaid, status) VALUES (" +
        quote(mysql, plate) + "," + quote(mysql, P_name) + "," +
        std::to_string(prepaidFee) + ",'active')";
    if (mysql_query(mysql, sql.c_str()) != 0) {
        if (!hasPass) BalanceService::instance().refund(userId, prepaidFee, "refund", "预约创建失败退款 " + plate);
        error = "预约失败";
        return false;
    }

    if (!tx.commit()) { error = "事务提交失败"; return false; }
    return true;
}

std::vector<Reservation> ReservationService::list() {
    return CrudService<Reservation>::list("SELECT id,license_plate,P_name,prepaid,status,created_at FROM RESERVATION WHERE status='active' ORDER BY created_at DESC");
}

bool ReservationService::cancel(int id) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "UPDATE RESERVATION SET status='cancelled' WHERE id=" + std::to_string(id) + " AND status='active'";
    return executeQueryAffected(mysql, sql) > 0;
}

std::vector<Reservation> ReservationService::getHistory(const std::string& startDate, const std::string& endDate,
                                                         int limit, int offset) {
    std::string sql = "SELECT id,license_plate,P_name,prepaid,status,created_at FROM RESERVATION WHERE status != 'active'";
    if (!startDate.empty()) sql += " AND DATE(created_at) >= '" + startDate + "'";
    if (!endDate.empty()) sql += " AND DATE(created_at) <= '" + endDate + "'";
    sql += " ORDER BY created_at DESC LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);
    return CrudService<Reservation>::list(sql);
}

Reservation ReservationService::mapRow(MYSQL_ROW row) {
    Reservation r;
    r.id = std::stoi(row[0]);
    r.license_plate = row[1] ? row[1] : "";
    r.P_name = row[2] ? row[2] : "";
    r.prepaid = row[3] ? std::stod(row[3]) : 0;
    r.status = row[4] ? row[4] : "";
    r.created_at = row[5] ? row[5] : "";
    return r;
}
