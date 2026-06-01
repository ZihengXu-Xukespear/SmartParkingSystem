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
                          "AND (P_name != '' AND P_name IS NOT NULL) "
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
    // Chinese license plate format rules:
    //
    // Standard plate (7 chars, 9 UTF-8 bytes):
    //   Pattern: Province(3B) + CityLetter(1B) + 5 alphanumeric(5B)
    //   Example: 京A12345, 沪B5F678
    //
    // New energy plate (8 chars, 10 UTF-8 bytes):
    //   Small car:  Province(3B) + CityLetter(1B) + D/F(1B) + 5 digits(5B)
    //     Example: 京AD12345
    //   Large car:  Province(3B) + CityLetter(1B) + 5 digits(5B) + D/F(1B)
    //     Example: 京A12345D
    //
    // After the province character (3 bytes UTF-8), all remaining characters
    // are ASCII (A-Z, 0-9), so byte indexing is straightforward.

    // Normalize: strip any middle dot separator (· U+00B7, 2 bytes) and spaces
    std::string normalized;
    normalized.reserve(plate.size());
    for (size_t i = 0; i < plate.size(); ) {
        unsigned char c = static_cast<unsigned char>(plate[i]);
        if (c == 0xC2 && i + 1 < plate.size() &&
            static_cast<unsigned char>(plate[i + 1]) == 0xB7) {
            // Skip UTF-8 encoded middle dot (· = U+00B7 = 0xC2 0xB7)
            i += 2;
            continue;
        }
        if (plate[i] == ' ') { i++; continue; }
        normalized += plate[i];
        i++;
    }

    size_t len = normalized.size();

    // Standard plate: 9 bytes, new energy: 10 bytes
    if (len != 9 && len != 10) return false;

    // Verify province prefix (first 3 bytes = one Chinese character in UTF-8)
    const std::string provinces =
        "京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤川青藏琼宁港澳";
    bool validProvince = false;
    for (size_t i = 0; i < provinces.size(); i += 3) {
        if (normalized.substr(0, 3) == provinces.substr(i, 3)) {
            validProvince = true;
            break;
        }
    }
    if (!validProvince) return false;

    // Second character (byte index 3) must be an uppercase letter (city code)
    if (normalized[3] < 'A' || normalized[3] > 'Z') return false;

    if (len == 9) {
        // Standard 7-char plate: positions 2-6 (bytes 4-8) must be A-Z or 0-9
        for (size_t i = 4; i < 9; i++) {
            char c = normalized[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
                return false;
        }
        return true;
    }

    // len == 10: New energy plate (8 chars)
    // Two valid formats:
    //   Small car (省AD12345): byte 4 is D/F, bytes 5-9 are digits
    //   Large car (省A12345D): bytes 4-8 are digits, byte 9 is D/F
    bool dAtPos2 = (normalized[4] == 'D' || normalized[4] == 'F');
    bool dAtLast = (normalized[9] == 'D' || normalized[9] == 'F');

    if (dAtPos2 && !dAtLast) {
        // Small new energy car: 省xDxxxxx (all x positions must be digits)
        for (size_t i = 5; i < 10; i++) {
            if (normalized[i] < '0' || normalized[i] > '9') return false;
        }
        return true;
    }

    if (dAtLast && !dAtPos2) {
        // Large new energy car: 省xxxxxD (all x positions must be digits)
        for (size_t i = 4; i < 9; i++) {
            if (normalized[i] < '0' || normalized[i] > '9') return false;
        }
        return true;
    }

    // Invalid: D/F in both positions, or neither position has D/F
    return false;
}
