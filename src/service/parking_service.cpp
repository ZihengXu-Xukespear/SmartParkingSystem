#include "parking_service.h"
#include "../config.h"

ParkingService& ParkingService::instance() {
    static ParkingService inst;
    return inst;
}

ParkingLot ParkingService::getStatus(const std::string& P_name) {
    ParkingLot lot;
    const auto& cfg = AppConfig::instance();
    std::string name = P_name.empty() ? cfg.parking_name : P_name;
    lot.P_name = name;
    lot.P_total_count = cfg.capacity;
    lot.P_fee = cfg.fee;
    auto conn = getConnection();
    if (!conn) return lot;
    MYSQL* mysql = conn->get();

    std::string sql = "SELECT P_id,P_name,P_total_count,P_current_count,P_reserve_count,P_fee FROM PARKING_LOT WHERE P_name=" +
        quote(mysql, name);
    if (mysql_query(mysql, sql.c_str()) != 0) return lot;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return lot;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        lot.P_id = std::stoi(row[0]);
        lot.P_name = row[1] ? row[1] : "";
        lot.P_total_count = std::stoi(row[2]);
        lot.P_current_count = std::stoi(row[3]);
        lot.P_reserve_count = std::stoi(row[4]);
        lot.P_fee = std::stod(row[5]);
    }
    mysql_free_result(res);
    return lot;
}

std::vector<ParkingLot> ParkingService::getAllLots() {
    std::vector<ParkingLot> lots;
    auto conn = getConnection();
    if (!conn) return lots;
    MYSQL* mysql = conn->get();
    std::string sql = "SELECT P_id,P_name,P_total_count,P_current_count,P_reserve_count,P_fee FROM PARKING_LOT ORDER BY P_id";
    if (mysql_query(mysql, sql.c_str()) != 0) return lots;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return lots;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        ParkingLot lot;
        lot.P_id = std::stoi(row[0]);
        lot.P_name = row[1] ? row[1] : "";
        lot.P_total_count = std::stoi(row[2]);
        lot.P_current_count = std::stoi(row[3]);
        lot.P_reserve_count = std::stoi(row[4]);
        lot.P_fee = std::stod(row[5]);
        lots.push_back(lot);
    }
    mysql_free_result(res);
    return lots;
}

bool ParkingService::addLot(const std::string& P_name, int total_count, double fee) {
    if (P_name.empty()) return false;
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    // Check for duplicate name
    std::string sql = "SELECT COUNT(*) FROM PARKING_LOT WHERE P_name=" + quote(mysql, P_name);
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && std::stoi(row[0]) > 0) { mysql_free_result(res); return false; }
            mysql_free_result(res);
        }
    }

    sql = "INSERT INTO PARKING_LOT (P_name,P_total_count,P_current_count,P_reserve_count,P_fee) VALUES (" +
        quote(mysql, P_name) + "," + std::to_string(total_count) + ",0,0," + std::to_string(fee) + ")";
    return executeQuery(mysql, sql);
}

bool ParkingService::deleteLot(int id, std::string& error) {
    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();

    // Get lot info
    std::string sql = "SELECT P_name,P_current_count,P_reserve_count FROM PARKING_LOT WHERE P_id=" + std::to_string(id);
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "查询失败"; return false; }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res || mysql_num_rows(res) == 0) { error = "停车场不存在"; if(res)mysql_free_result(res); return false; }
    MYSQL_ROW row = mysql_fetch_row(res);
    std::string name = row[0] ? row[0] : "";
    int current = row[1] ? std::stoi(row[1]) : 0;
    int reserve = row[2] ? std::stoi(row[2]) : 0;
    mysql_free_result(res);

    if (current > 0) { error = "停车场「" + name + "」还有 " + std::to_string(current) + " 辆车在场，无法删除"; return false; }
    if (reserve > 0) { error = "停车场「" + name + "」还有 " + std::to_string(reserve) + " 个预约，无法删除"; return false; }

    // Check it's not the last lot
    sql = "SELECT COUNT(*) FROM PARKING_LOT";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* cres = mysql_store_result(mysql);
        if (cres) {
            MYSQL_ROW crow = mysql_fetch_row(cres);
            if (crow && std::stoi(crow[0]) <= 1) { error = "至少保留一个停车场"; mysql_free_result(cres); return false; }
            mysql_free_result(cres);
        }
    }

    // Delete related billing rules and pass plans for this lot
    mysql_query(mysql, ("DELETE FROM BILLING_RULE WHERE P_name=" + quote(mysql, name)).c_str());
    mysql_query(mysql, ("DELETE FROM PASS_PLAN WHERE P_name=" + quote(mysql, name)).c_str());
    // Cancel active reservations for this lot
    mysql_query(mysql, ("UPDATE RESERVATION SET status='cancelled' WHERE P_name=" + quote(mysql, name) + " AND status='active'").c_str());

    sql = "DELETE FROM PARKING_LOT WHERE P_id=" + std::to_string(id);
    return executeQuery(mysql, sql);
}

bool ParkingService::updateSettings(const std::string& P_name, double fee, int total_count, const std::string& new_name) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    std::string name = P_name.empty() ? AppConfig::instance().parking_name : P_name;
    std::string sql = "UPDATE PARKING_LOT SET P_fee=" + std::to_string(fee) +
        ", P_total_count=" + std::to_string(total_count);
    if (!new_name.empty() && new_name != name) {
        sql += ", P_name=" + quote(mysql, new_name);
    }
    sql += " WHERE P_name=" + quote(mysql, name);
    if (!executeQuery(mysql, sql)) return false;

    // Also update P_name references in all related tables
    if (!new_name.empty() && new_name != name) {
        mysql_query(mysql, ("UPDATE RESERVATION SET P_name=" + quote(mysql, new_name) + " WHERE P_name=" + quote(mysql, name)).c_str());
        mysql_query(mysql, ("UPDATE CAR_RECORD SET P_name=" + quote(mysql, new_name) + " WHERE P_name=" + quote(mysql, name)).c_str());
        mysql_query(mysql, ("UPDATE CAR_RECORD SET location=" + quote(mysql, new_name) + " WHERE location=" + quote(mysql, name)).c_str());
        mysql_query(mysql, ("UPDATE BILLING_RULE SET P_name=" + quote(mysql, new_name) + " WHERE P_name=" + quote(mysql, name)).c_str());
        mysql_query(mysql, ("UPDATE PASS_PLAN SET P_name=" + quote(mysql, new_name) + " WHERE P_name=" + quote(mysql, name)).c_str());
    }

    if (name == AppConfig::instance().parking_name) {
        AppConfig::instance().fee = fee;
        AppConfig::instance().capacity = total_count;
    }
    return true;
}
