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
    return checkIn(plate, billing_type, "", 0, error);
}

bool VehicleService::checkIn(const std::string& plate, const std::string& billing_type, const std::string& P_name, std::string& error) {
    return checkIn(plate, billing_type, P_name, 0, error);
}

bool VehicleService::checkIn(const std::string& plate, const std::string& billing_type, const std::string& P_name, int spotNum, std::string& error) {
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

    // Determine parking lot: explicit param > reservation's lot > default
    std::string parkingName = P_name.empty() ? AppConfig::instance().parking_name : P_name;
    int reservationId = 0;
    int spotNumber = spotNum;

    // If an explicit spot was chosen, check it's not already occupied
    if (spotNumber > 0) {
        std::string spotSql = "SELECT id FROM CAR_RECORD WHERE spot_number=" + std::to_string(spotNumber) +
            " AND check_out_time IS NULL AND P_name=" + quote(mysql, parkingName);
        if (mysql_query(mysql, spotSql.c_str()) == 0) {
            MYSQL_RES* sres = mysql_store_result(mysql);
            if (sres && mysql_num_rows(sres) > 0) {
                mysql_free_result(sres);
                error = parkingName + " " + std::to_string(spotNumber) + " 号车位已被占用";
                return false;
            }
            if (sres) mysql_free_result(sres);
        }
    }

    sql = "SELECT id,spot_number,P_name FROM RESERVATION WHERE license_plate=" + quote(mysql, plate) +
        " AND status='active' LIMIT 1";
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* rres = mysql_store_result(mysql);
        if (rres && mysql_num_rows(rres) > 0) {
            MYSQL_ROW rrow = mysql_fetch_row(rres);
            reservationId = std::stoi(rrow[0]);
            spotNumber = rrow[1] ? std::stoi(rrow[1]) : 0;
            if (P_name.empty() && rrow[2]) parkingName = rrow[2];
            std::string updSql = "UPDATE RESERVATION SET status='completed' WHERE id=" + std::to_string(reservationId);
            mysql_query(mysql, updSql.c_str());
        }
        if (rres) mysql_free_result(rres);
    }

    sql = "UPDATE PARKING_LOT SET P_current_count = P_current_count + 1 WHERE P_name=" +
        quote(mysql, parkingName) + " AND P_current_count + P_reserve_count < P_total_count";
    if (executeQueryAffected(mysql, sql) <= 0) { error = "停车场已满"; return false; }

    std::string spotCol = spotNumber > 0 ? ",spot_number" : "";
    std::string spotVal = spotNumber > 0 ? "," + std::to_string(spotNumber) : "";

    sql = "INSERT INTO CAR_RECORD (license_plate,check_in_time,location,billing_type,reservation_id,P_name" + spotCol + ") VALUES (" +
        quote(mysql, plate) + ",NOW()," + quote(mysql, parkingName) + "," +
        quote(mysql, billing_type) + "," + std::to_string(reservationId) + "," +
        quote(mysql, parkingName) + spotVal + ")";
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

    // Get the car's parking lot for billing rule lookup
    std::string carLot = AppConfig::instance().parking_name;
    {
        std::string lotSql = "SELECT COALESCE(P_name,location) FROM CAR_RECORD WHERE id=" + std::to_string(rec_id);
        if (mysql_query(mysql, lotSql.c_str()) == 0) {
            MYSQL_RES* lres = mysql_store_result(mysql);
            if (lres) {
                MYSQL_ROW lrow = mysql_fetch_row(lres);
                if (lrow && lrow[0]) carLot = lrow[0];
                mysql_free_result(lres);
            }
        }
    }

    fee = calculateFee(mysql, plate, check_in, billing_type, carLot, reservationId);

    if (fee > 0.01) {
        int payerId = userId;
        // If the plate has a monthly pass owner, deduct from that user instead of the operator
        std::string ownerSql = "SELECT user_id FROM MONTHLY_PASS WHERE license_plate=" +
            quote(mysql, plate) + " AND is_active=1 AND start_date <= CURDATE() AND end_date >= CURDATE() LIMIT 1";
        if (mysql_query(mysql, ownerSql.c_str()) == 0) {
            MYSQL_RES* ores = mysql_store_result(mysql);
            if (ores && mysql_num_rows(ores) > 0) {
                MYSQL_ROW orow = mysql_fetch_row(ores);
                if (orow && orow[0] && std::stoi(orow[0]) > 0)
                    payerId = std::stoi(orow[0]);
            }
            if (ores) mysql_free_result(ores);
        }
        std::string deductErr;
        if (!BalanceService::instance().deduct(payerId, fee, "checkout",
            "停车费 " + plate, deductErr)) {
            error = deductErr;
            return false;
        }
    }

    sql = "UPDATE CAR_RECORD SET check_out_time=NOW(), fee=" + std::to_string(fee) +
        ", exit_deadline=DATE_ADD(NOW(), INTERVAL 10 MINUTE)" +
        " WHERE id=" + std::to_string(rec_id);
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "更新记录失败"; return false; }

    sql = "UPDATE PARKING_LOT SET P_current_count = GREATEST(P_current_count - 1, 0) WHERE P_name=" +
        quote(mysql, carLot);
    mysql_query(mysql, sql.c_str());

    sql = "SELECT id,license_plate,check_in_time,check_out_time,fee,location,billing_type,"
        "'' AS duration,COALESCE(exit_deadline,''),COALESCE(P_name,location),COALESCE(spot_number,0) FROM CAR_RECORD WHERE id=" +
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
            record.duration = row[7] ? row[7] : "";
            record.exit_deadline = row[8] ? row[8] : "";
            record.P_name = row[9] ? row[9] : "";
            record.spot_number = row[10] ? std::stoi(row[10]) : 0;
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

    std::string sql = "SELECT id,license_plate,check_in_time,check_out_time,fee,location,billing_type,"
        "CASE WHEN check_out_time IS NULL THEN CONCAT(FLOOR(TIMESTAMPDIFF(MINUTE,check_in_time,NOW())/60),'小时',MOD(TIMESTAMPDIFF(MINUTE,check_in_time,NOW()),60),'分') ELSE '' END AS duration,"
        "COALESCE(exit_deadline,''),COALESCE(P_name,location),COALESCE(spot_number,0) FROM CAR_RECORD WHERE 1=1";
    if (!plate.empty()) sql += " AND license_plate=" + quote(mysql, plate);
    if (!start_date.empty()) sql += " AND check_in_time >= " + quote(mysql, start_date + " 00:00:00");
    if (!end_date.empty()) sql += " AND check_in_time <= " + quote(mysql, end_date + " 23:59:59");
    sql += " ORDER BY check_in_time DESC LIMIT 500";

    return list(sql);
}

std::vector<CarRecord> VehicleService::getParkedVehicles(const std::string& plate_filter) {
    auto conn = getConnection();
    if (!conn) return {};
    MYSQL* mysql = conn->get();

    std::string sql = "SELECT id,license_plate,check_in_time,check_out_time,fee,location,billing_type,"
        "CONCAT(FLOOR(TIMESTAMPDIFF(MINUTE,check_in_time,NOW())/60),'小时',MOD(TIMESTAMPDIFF(MINUTE,check_in_time,NOW()),60),'分') AS duration,"
        "COALESCE(exit_deadline,''),COALESCE(P_name,location),COALESCE(spot_number,0) FROM CAR_RECORD WHERE check_out_time IS NULL";
    if (!plate_filter.empty()) sql += " AND license_plate LIKE " + quote(mysql, "%" + plate_filter + "%");
    sql += " ORDER BY check_in_time DESC";

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
    r.duration = row[7] ? row[7] : "";
    r.exit_deadline = row[8] ? row[8] : "";
    r.P_name = row[9] ? row[9] : "";
    r.spot_number = row[10] ? std::stoi(row[10]) : 0;
    return r;
}

double VehicleService::calculateFee(MYSQL* mysql, const std::string& plate, const std::string& check_in_time,
                    const std::string& billing_type, const std::string& P_name, int reservationId) {
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

    // Try the requested billing type first, fall back to 'standard' if not found
    auto tryGetRule = [&](const std::string& ruleType) -> bool {
        std::string s = "SELECT free_minutes,hourly_rate,max_daily_fee,tier_config FROM BILLING_RULE WHERE rule_type=" +
            quote(mysql, ruleType) + " AND is_active=1 AND P_name=" + quote(mysql, P_name) + " LIMIT 1";
        if (mysql_query(mysql, s.c_str()) == 0) {
            MYSQL_RES* r = mysql_store_result(mysql);
            if (r) {
                MYSQL_ROW row = mysql_fetch_row(r);
                if (row) {
                    free_minutes = row[0] ? std::stoi(row[0]) : 30;
                    hourly_rate = row[1] ? std::stod(row[1]) : hourly_rate;
                    max_daily_fee = row[2] ? std::stod(row[2]) : 50.0;
                    tier_config = row[3] ? row[3] : "";
                    mysql_free_result(r);
                    return true;
                }
                mysql_free_result(r);
            }
        }
        return false;
    };
    if (!tryGetRule(billing_type)) tryGetRule("standard");

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
