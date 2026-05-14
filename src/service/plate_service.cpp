#include "plate_service.h"
#ifdef ENABLE_OPENCV
#include "../plate_recognizer.h"
#endif
#include <mysql.h>
#include <sstream>

PlateService& PlateService::instance() {
    static PlateService inst;
    return inst;
}

PlateService::PlateResult PlateService::recognize(const std::string& image_data) {
    PlateResult result;

#ifdef ENABLE_OPENCV
    // Use the OpenCV-based plate recognizer
    auto recog_result = PlateRecognizer::instance().recognize(image_data);

    result.plate_number = recog_result.plate_number;
    result.confidence = recog_result.confidence;
    result.color = recog_result.plate_color;
    result.message = recog_result.message;
#else
    result.plate_number = "";
    result.confidence = 0.0;
    result.color = "unknown";
    result.message = "车牌识别功能未启用（未安装 OpenCV）";
#endif

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
    if (plate.size() < 9 || plate.size() > 10) return false;

    const std::string provinces = "京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤川青藏琼宁";
    for (size_t i = 0; i < provinces.size(); i += 3) {
        if (plate.substr(0, 3) == provinces.substr(i, 3)) {
            if (plate[3] >= 'A' && plate[3] <= 'Z')
                return true;
        }
    }
    return false;
}
