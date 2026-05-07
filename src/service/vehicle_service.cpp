#include "vehicle_service.h"
#include "blacklist_service.h"
#include "balance_service.h"
#include "plate_service.h"
#include "../config.h"

VehicleService& VehicleService::instance() {
    static VehicleService inst;
    return inst;
}

bool VehicleService::checkIn(const std::string& plate, const std::string& billing_type, std::string& error) {
    if (!validatePlate(plate)) { error = "车牌号格式不正确"; return false; }

    std::string blReason;
    if (BlacklistService::instance().isBlacklisted(plate, &blReason)) {
        error = "该车辆已被列入黑名单，禁止入库";
        BlacklistService::instance().logInterception(plate, blReason);
        return false;
    }

    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();
    Transaction tx(mysql);

    std::string sql = "SELECT id FROM CAR_RECORD WHERE license_plate=" +
        quote(mysql, plate) + " AND check_out_time IS NULL";
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "查询失败"; return false; }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (res && mysql_num_rows(res) > 0) {
        mysql_free_result(res);
        error = "该车辆已在停车场内";
        return false;
    }
    if (res) mysql_free_result(res);

    const auto& cfg = AppConfig::instance();
    sql = "UPDATE PARKING_LOT SET P_current_count = P_current_count + 1 WHERE P_name=" +
        quote(mysql, cfg.parking_name) + " AND P_current_count + P_reserve_count < P_total_count";
    if (executeQueryAffected(mysql, sql) <= 0) { error = "停车场已满"; return false; }

    int reservationId = 0;
    sql = "SELECT id FROM RESERVATION WHERE license_plate=" + quote(mysql, plate) +
        " AND status='active' LIMIT 1";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* rres = mysql_store_result(mysql);
        if (rres && mysql_num_rows(rres) > 0) {
            MYSQL_ROW rrow = mysql_fetch_row(rres);
            reservationId = std::stoi(rrow[0]);
            std::string updSql = "UPDATE RESERVATION SET status='completed' WHERE id=" + std::to_string(reservationId);
            mysql_query(mysql, updSql.c_str());
        }
        if (rres) mysql_free_result(rres);
    }

    sql = "INSERT INTO CAR_RECORD (license_plate,check_in_time,location,billing_type,reservation_id) VALUES (" +
        quote(mysql, plate) + ",NOW()," + quote(mysql, cfg.parking_name) + "," +
        quote(mysql, billing_type) + "," + std::to_string(reservationId) + ")";
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "插入记录失败"; return false; }

    if (!tx.commit()) { error = "事务提交失败"; return false; }
    return true;
}

bool VehicleService::checkOut(const std::string& plate, int userId, double& fee, CarRecord& record, std::string& error) {
    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();
    Transaction tx(mysql);

    std::string sql = "SELECT id,license_plate,check_in_time,billing_type,reservation_id FROM CAR_RECORD WHERE license_plate=" +
        quote(mysql, plate) + " AND check_out_time IS NULL ORDER BY check_in_time DESC LIMIT 1";
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "查询失败"; return false; }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res || mysql_num_rows(res) == 0) {
        if (res) mysql_free_result(res);
        error = "该车辆不在停车场内";
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int rec_id = std::stoi(row[0]);
    std::string billing_type = row[3] ? row[3] : "standard";
    std::string check_in = row[2] ? row[2] : "";
    int reservationId = row[4] ? std::stoi(row[4]) : 0;
    mysql_free_result(res);

    fee = calculateFee(mysql, plate, check_in, billing_type, reservationId);

    if (fee > 0.01) {
        std::string deductErr;
        if (!BalanceService::instance().deduct(userId, fee, "checkout",
            "停车费 " + plate, deductErr)) {
            error = deductErr;
            return false;
        }
    }

    sql = "UPDATE CAR_RECORD SET check_out_time=NOW(), fee=" + std::to_string(fee) +
        ", exit_deadline=DATE_ADD(NOW(), INTERVAL 10 MINUTE)" +
        " WHERE id=" + std::to_string(rec_id);
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "更新记录失败"; return false; }

    const auto& cfg = AppConfig::instance();
    sql = "UPDATE PARKING_LOT SET P_current_count = GREATEST(P_current_count - 1, 0) WHERE P_name=" +
        quote(mysql, cfg.parking_name);
    mysql_query(mysql, sql.c_str());

    sql = "SELECT id,license_plate,check_in_time,check_out_time,fee,location,billing_type,exit_deadline FROM CAR_RECORD WHERE id=" +
        std::to_string(rec_id);
    if (mysql_query(mysql, sql.c_str()) == 0) {
        res = mysql_store_result(mysql);
        if (res && (row = mysql_fetch_row(res))) {
            record.id = std::stoi(row[0]);
            record.license_plate = row[1] ? row[1] : "";
            record.check_in_time = row[2] ? row[2] : "";
            record.check_out_time = row[3] ? row[3] : "";
            record.fee = row[4] ? std::stod(row[4]) : 0.0;
            record.location = row[5] ? row[5] : "";
            record.billing_type = row[6] ? row[6] : "standard";
            mysql_free_result(res);
        }
    }

    if (!tx.commit()) { error = "事务提交失败"; return false; }
    return true;
}

std::vector<CarRecord> VehicleService::queryRecords(const std::string& plate, const std::string& start_date, const std::string& end_date) {
    auto conn = getConnection();
    if (!conn) return {};
    MYSQL* mysql = conn->get();

    std::string sql = "SELECT id,license_plate,check_in_time,check_out_time,fee,location,billing_type FROM CAR_RECORD WHERE 1=1";
    if (!plate.empty()) sql += " AND license_plate=" + quote(mysql, plate);
    if (!start_date.empty()) sql += " AND check_in_time >= " + quote(mysql, start_date + " 00:00:00");
    if (!end_date.empty()) sql += " AND check_in_time <= " + quote(mysql, end_date + " 23:59:59");
    sql += " ORDER BY check_in_time DESC LIMIT 500";

    return list(sql);
}

bool VehicleService::deleteRecord(int id) {
    return deleteById(id);
}

CarRecord VehicleService::mapRow(MYSQL_ROW row) {
    CarRecord r;
    r.id = std::stoi(row[0]);
    r.license_plate = row[1] ? row[1] : "";
    r.check_in_time = row[2] ? row[2] : "";
    r.check_out_time = row[3] ? row[3] : "";
    r.fee = row[4] ? std::stod(row[4]) : 0.0;
    r.location = row[5] ? row[5] : "";
    r.billing_type = row[6] ? row[6] : "standard";
    return r;
}

double VehicleService::calculateFee(MYSQL* mysql, const std::string& plate, const std::string& check_in_time,
                    const std::string& billing_type, int reservationId) {
    int free_minutes_extra = 0;
    if (reservationId > 0) {
        std::string resSql = "SELECT prepaid FROM RESERVATION WHERE id=" + std::to_string(reservationId);
        if (mysql_query(mysql, resSql.c_str()) == 0) {
            MYSQL_RES* rres = mysql_store_result(mysql);
            if (rres) {
                MYSQL_ROW rrow = mysql_fetch_row(rres);
                if (rrow && rrow[0] && std::stod(rrow[0]) > 0) {
                    free_minutes_extra = 60;
                }
                mysql_free_result(rres);
            }
        }
    }

    std::string sql = "SELECT COUNT(*) FROM MONTHLY_PASS WHERE license_plate=" +
        quote(mysql, plate) + " AND is_active=1 AND start_date <= CURDATE() AND end_date >= CURDATE()";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && std::stoi(row[0]) > 0) {
                mysql_free_result(res);
                return 0.0;
            }
            mysql_free_result(res);
        }
    }

    double hourly_rate = AppConfig::instance().fee;
    int free_minutes = 30;
    double max_daily_fee = 50.0;
    std::string tier_config;

    sql = "SELECT free_minutes,hourly_rate,max_daily_fee,tier_config FROM BILLING_RULE WHERE rule_type=" +
        quote(mysql, billing_type) + " AND is_active=1 LIMIT 1";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                free_minutes = row[0] ? std::stoi(row[0]) : 30;
                hourly_rate = row[1] ? std::stod(row[1]) : hourly_rate;
                max_daily_fee = row[2] ? std::stod(row[2]) : 50.0;
                tier_config = row[3] ? row[3] : "";
            }
            mysql_free_result(res);
        }
    }

    sql = "SELECT TIMESTAMPDIFF(MINUTE, " + quote(mysql, check_in_time) + ", NOW())";
    int duration_min = 0;
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) duration_min = std::stoi(row[0]);
            mysql_free_result(res);
        }
    }

    int effective_free = free_minutes + free_minutes_extra;
    if (duration_min <= effective_free) return 0.0;

    double fee = 0.0;

    if (billing_type == "tiered" && !tier_config.empty()) {
        double remaining_hours = std::ceil((duration_min - effective_free) / 60.0);
        auto tier_json = crow::json::load(tier_config);
        if (tier_json) {
            for (const auto& tier : tier_json) {
                int tier_hours = tier["hours"].i();
                double tier_rate = tier["rate"].d();
                double apply_hours = std::min(remaining_hours, (double)tier_hours);
                fee += apply_hours * tier_rate;
                remaining_hours -= apply_hours;
                if (remaining_hours <= 0) break;
            }
        }
        if (remaining_hours > 0) fee += remaining_hours * hourly_rate;
    } else {
        double hours = std::ceil((duration_min - effective_free) / 60.0);
        fee = hours * hourly_rate;
    }

    if (max_daily_fee > 0 && fee > max_daily_fee) fee = max_daily_fee;

    fee = std::round(fee * 100.0) / 100.0;
    return fee;
}

bool VehicleService::validatePlate(const std::string& plate) {
    return PlateService::validatePlate(plate);
}
