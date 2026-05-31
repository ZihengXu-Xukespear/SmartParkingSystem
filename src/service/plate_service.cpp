#include "plate_service.h"
#include "../plate_recognizer.h"
#include <mysql.h>
#include <sstream>

PlateService& PlateService::instance() {
    static PlateService inst;
    return inst;
}

PlateService::PlateResult PlateService::recognize(const std::string& image_data) {
    PlateResult result;

    auto recog_result = PlateRecognizer::instance().recognize(image_data);

    result.plate_number = recog_result.plate_number;
    result.confidence = recog_result.confidence;
    result.color = recog_result.plate_color;
    result.message = recog_result.message;

    return result;
}

PlateService::PlateRegistrationInfo PlateService::checkRegistration(const std::string& plate) {
    PlateRegistrationInfo info;
    info.plate_number = plate;

    if (plate.empty()) {
        info.message = "车牌号为空";
        return info;
    }

    auto conn = getConnection();
    if (!conn || !conn->get()) {
        info.message = "数据库连接失败";
        return info;
    }

    MYSQL* mysql = conn->get();
    std::string escaped = escape(mysql, plate);

    // Check in CAR_RECORD (has the car ever entered?)
    {
        std::string sql = "SELECT COUNT(*) FROM CAR_RECORD WHERE license_plate = '" + escaped + "'";
        if (mysql_query(mysql, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(mysql);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) {
                    int count = std::stoi(row[0]);
                    info.is_registered = (count > 0);
                }
                mysql_free_result(res);
            }
        }
    }

    // Check if currently parked
    {
        std::string sql = "SELECT check_in_time FROM CAR_RECORD "
                          "WHERE license_plate = '" + escaped + "' AND check_out_time IS NULL "
                          "ORDER BY check_in_time DESC LIMIT 1";
        if (mysql_query(mysql, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(mysql);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) {
                    info.in_parking = true;
                    info.last_check_in = row[0];
                }
                mysql_free_result(res);
            }
        }
    }

    // Check MONTHLY_PASS
    {
        std::string sql = "SELECT end_date FROM MONTHLY_PASS "
                          "WHERE license_plate = '" + escaped + "' "
                          "AND end_date >= CURDATE() "
                          "ORDER BY end_date DESC LIMIT 1";
        if (mysql_query(mysql, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(mysql);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) {
                    info.has_monthly_pass = true;
                    info.monthly_pass_end = row[0];
                    info.is_registered = true;
                }
                mysql_free_result(res);
            }
        }
    }

    // Check BLACKLIST
    {
        std::string sql = "SELECT reason FROM VEHICLE_BLACKLIST "
                          "WHERE license_plate = '" + escaped + "' LIMIT 1";
        if (mysql_query(mysql, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(mysql);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) {
                    info.is_blacklisted = true;
                    info.blacklist_reason = row[0];
                }
                mysql_free_result(res);
            }
        }
    }

    // Build message
    if (info.is_blacklisted) {
        info.message = "该车辆已被列入黑名单：" + info.blacklist_reason;
    } else if (info.is_registered) {
        std::stringstream ss;
        ss << "该车辆已登记。";
        if (info.in_parking)
            ss << "当前在场内，入库时间：" << info.last_check_in;
        if (info.has_monthly_pass)
            ss << " (月卡有效至" << info.monthly_pass_end << ")";
        info.message = ss.str();
    } else {
        info.message = "该车辆未登记，请先办理登记手续";
    }

    return info;
}

bool PlateService::validatePlate(const std::string& plate) {
    // Standard plate: province(1) + letter(1) + 5 alphanum = 7 Chinese chars → 9-10 UTF-8 bytes
    // New energy plate: province(1) + letter(1) + 6 alphanum = 8 Chinese chars → 10-11 UTF-8 bytes
    if (plate.size() < 9 || plate.size() > 11) return false;

    // Verify province prefix (each Chinese char is 3 bytes in UTF-8)
    const std::string provinces = "京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤川青藏琼宁";
    bool validProvince = false;
    for (size_t i = 0; i < provinces.size(); i += 3) {
        if (plate.substr(0, 3) == provinces.substr(i, 3)) {
            validProvince = true;
            break;
        }
    }
    if (!validProvince) return false;

    // Position 3 must be an uppercase letter (the city/region code)
    if (plate[3] < 'A' || plate[3] > 'Z') return false;

    // Remaining characters (positions 4+) must be alphanumeric (A-Z or 0-9)
    for (size_t i = 4; i < plate.size(); i++) {
        char c = plate[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return false;
    }

    // Length check: 7 chars (9 bytes) or 8 chars (10-11 bytes, new energy plates may include D/F)
    // 7-char: province(3) + letter(1) + 5 digits(5) = 9 bytes minimum
    // 8-char: province(3) + letter(1) + 6 alphanum(6) = 10 bytes (new energy)
    // Allow 9-11 bytes to be safe
    return true;
}
