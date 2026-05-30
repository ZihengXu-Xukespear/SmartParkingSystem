#include "billing_service.h"
#include "base_service.h"
#include <map>
#include <string>
#include <vector>
#include <cmath>

BillingService& BillingService::instance() {
    static BillingService inst;
    return inst;
}

std::vector<BillingRule> BillingService::getRules() {
    std::vector<BillingRule> rules;
    auto conn = getConnection();
    if (!conn) return rules;

    if (mysql_query(conn->get(), "SELECT id,rule_name,rule_type,free_minutes,hourly_rate,max_daily_fee,tier_config,description,is_active,COALESCE(P_name,'') FROM BILLING_RULE ORDER BY id") != 0)
        return rules;
    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return rules;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        BillingRule r;
        r.id = std::stoi(row[0]);
        r.rule_name = row[1] ? row[1] : "";
        r.rule_type = row[2] ? row[2] : "";
        r.free_minutes = row[3] ? std::stoi(row[3]) : 30;
        r.hourly_rate = row[4] ? std::stod(row[4]) : 5.0;
        r.max_daily_fee = row[5] ? std::stod(row[5]) : 50.0;
        r.tier_config = row[6] ? row[6] : "";
        r.description = row[7] ? row[7] : "";
        r.is_active = row[8] ? std::stoi(row[8]) == 1 : true;
        r.P_name = row[9] ? row[9] : "";
        rules.push_back(r);
    }
    mysql_free_result(res);
    return rules;
}

bool BillingService::addRule(const BillingRule& rule) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "INSERT INTO BILLING_RULE (rule_name,rule_type,free_minutes,hourly_rate,max_daily_fee,tier_config,description,is_active,P_name) VALUES (" +
        quote(mysql, rule.rule_name) + "," + quote(mysql, rule.rule_type) + "," +
        std::to_string(rule.free_minutes) + "," + std::to_string(rule.hourly_rate) + "," +
        std::to_string(rule.max_daily_fee) + "," + quote(mysql, rule.tier_config) + "," +
        quote(mysql, rule.description) + "," + std::to_string(rule.is_active ? 1 : 0) + "," +
        quote(mysql, rule.P_name) + ")";
    return executeQuery(mysql, sql);
}

bool BillingService::deleteRule(int id) {
    auto conn = getConnection();
    if (!conn) return false;
    std::string sql = "DELETE FROM BILLING_RULE WHERE id=" + std::to_string(id);
    return executeQuery(conn->get(), sql);
}

bool BillingService::updateRule(int id, const BillingRule& rule) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    std::string sql = "UPDATE BILLING_RULE SET rule_name=" + quote(mysql, rule.rule_name) +
        ", rule_type=" + quote(mysql, rule.rule_type) +
        ", free_minutes=" + std::to_string(rule.free_minutes) +
        ", hourly_rate=" + std::to_string(rule.hourly_rate) +
        ", max_daily_fee=" + std::to_string(rule.max_daily_fee) +
        ", tier_config=" + quote(mysql, rule.tier_config) +
        ", description=" + quote(mysql, rule.description) +
        ", is_active=" + std::to_string(rule.is_active ? 1 : 0) +
        ", P_name=" + quote(mysql, rule.P_name) +
        " WHERE id=" + std::to_string(id);
    return executeQuery(mysql, sql);
}

// ===================== 【用户用】带 userId（个人中心显示：月卡*2+季卡） =====================
std::vector<MonthlyPass> BillingService::getMonthlyPasses(int userId)
{
    std::vector<MonthlyPass> passes;
    auto conn = this->getConnection();
    if (!conn) return passes;

    std::string sql =
        "SELECT license_plate, pass_type, end_date, fee, start_date "
        "FROM MONTHLY_PASS "
        "WHERE user_id = " + std::to_string(userId) +
        " AND is_active = 1 AND end_date >= NOW()";

    if (mysql_query(conn->get(), sql.c_str()) != 0)
        return passes;

    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return passes;

    std::map<std::string, std::map<std::string, int>> plateTypeCount;
    std::map<std::string, double> plateTotalFee;
    std::map<std::string, std::string> plateLatestEnd;
    std::map<std::string, std::string> plateStartDate;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        std::string plate = row[0] ? row[0] : "";
        std::string type = row[1] ? row[1] : "";
        std::string end = row[2] ? row[2] : "";
        double fee = row[3] ? std::stod(row[3]) : 0.0;
        std::string start = row[4] ? row[4] : "";

        if (plate.empty() || type.empty()) continue;

        if (plateStartDate[plate].empty())
            plateStartDate[plate] = start;

        plateTypeCount[plate][type]++;
        plateTotalFee[plate] += fee;

        if (plateLatestEnd[plate].empty() || end > plateLatestEnd[plate])
            plateLatestEnd[plate] = end;
    }
    mysql_free_result(res);

    for (auto& entry : plateTypeCount)
    {
        std::string plate = entry.first;
        auto& countMap = entry.second;

        std::string combinedType;
        for (auto& p : countMap)
        {
            if (!combinedType.empty()) combinedType += "+";
            if (p.second == 1)
                combinedType += p.first;
            else
                combinedType += p.first + "*" + std::to_string(p.second);
        }

        MonthlyPass mp;
        mp.id = 0;
        mp.license_plate = plate;
        mp.pass_type = combinedType;
        mp.start_date = plateStartDate[plate];
        mp.end_date = plateLatestEnd[plate];
        mp.fee = plateTotalFee[plate];
        mp.is_active = true;
        mp.user_id = userId;
        passes.push_back(mp);
    }

    return passes;
}

// ===================== 【管理员用】不带 userId =====================
std::vector<MonthlyPass> BillingService::getMonthlyPasses()
{
    std::vector<MonthlyPass> passes;
    auto conn = this->getConnection();
    if (!conn) return passes;

    std::string sql =
        "SELECT user_id, license_plate, pass_type, end_date, fee, start_date "
        "FROM MONTHLY_PASS "
        "WHERE is_active = 1 AND end_date >= NOW()";

    if (mysql_query(conn->get(), sql.c_str()) != 0)
        return passes;

    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return passes;

    std::map<std::string, std::map<std::string, int>> plateTypeCount;
    std::map<std::string, double> plateTotalFee;
    std::map<std::string, std::string> plateLatestEnd;
    std::map<std::string, std::string> plateStartDate;
    std::map<std::string, int> plateUserId;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        int uid = std::stoi(row[0]);
        std::string plate = row[1] ? row[1] : "";
        std::string type = row[2] ? row[2] : "";
        std::string end = row[3] ? row[3] : "";
        double fee = row[4] ? std::stod(row[4]) : 0.0;
        std::string start = row[5] ? row[5] : "";

        if (plate.empty() || type.empty()) continue;

        if (plateStartDate[plate].empty())
            plateStartDate[plate] = start;

        plateUserId[plate] = uid;
        plateTypeCount[plate][type]++;
        plateTotalFee[plate] += fee;

        if (plateLatestEnd[plate].empty() || end > plateLatestEnd[plate])
            plateLatestEnd[plate] = end;
    }
    mysql_free_result(res);

    for (auto& entry : plateTypeCount)
    {
        std::string plate = entry.first;
        auto& countMap = entry.second;

        std::string combinedType;
        for (auto& p : countMap)
        {
            if (!combinedType.empty()) combinedType += "+";
            if (p.second == 1)
                combinedType += p.first;
            else
                combinedType += p.first + "*" + std::to_string(p.second);
        }

        MonthlyPass mp;
        mp.id = 0;
        mp.license_plate = plate;
        mp.pass_type = combinedType;
        mp.start_date = plateStartDate[plate];
        mp.end_date = plateLatestEnd[plate];
        mp.fee = plateTotalFee[plate];
        mp.is_active = true;
        mp.user_id = plateUserId[plate];
        passes.push_back(mp);
    }

    return passes;
}

// ===================== 【强制每次购买都新增一条记录】 =====================
bool BillingService::addMonthlyPass(const MonthlyPass& pass) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    // 永远插入新记录！！！
    std::string sql = "INSERT INTO MONTHLY_PASS ("
        "user_id, license_plate, pass_type, start_date, end_date, fee, is_active"
        ") VALUES (" +
        std::to_string(pass.user_id) + "," +
        quote(mysql, pass.license_plate) + "," +
        quote(mysql, pass.pass_type) + "," +
        quote(mysql, pass.start_date) + "," +
        quote(mysql, pass.end_date) + "," +
        std::to_string(pass.fee) + ", 1)";

    return executeQuery(mysql, sql);
}

bool BillingService::updateMonthlyPass(int id, const MonthlyPass& pass) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "UPDATE MONTHLY_PASS SET license_plate=" + quote(mysql, pass.license_plate) +
        ", pass_type=" + quote(mysql, pass.pass_type) +
        ", start_date=" + quote(mysql, pass.start_date) +
        ", end_date=" + quote(mysql, pass.end_date) +
        ", fee=" + std::to_string(pass.fee) +
        " WHERE id=" + std::to_string(id);
    return executeQuery(mysql, sql);
}

bool BillingService::deleteMonthlyPass(int id) {
    auto conn = getConnection();
    if (!conn) return false;
    std::string sql = "DELETE FROM MONTHLY_PASS WHERE id=" + std::to_string(id);
    return executeQuery(conn->get(), sql);
}

BillingRule BillingService::getActiveRule() {
    BillingRule rule{};
    auto conn = getConnection();
    if (!conn) return rule;

    if (mysql_query(conn->get(),
        "SELECT id,rule_name,rule_type,free_minutes,hourly_rate,max_daily_fee,tier_config,description,is_active "
        "FROM BILLING_RULE WHERE is_active=1 ORDER BY id LIMIT 1") != 0)
        return rule;

    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return rule;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        rule.id = std::stoi(row[0]);
        rule.rule_name = row[1] ? row[1] : "";
        rule.rule_type = row[2] ? row[2] : "";
        rule.free_minutes = row[3] ? std::stoi(row[3]) : 30;
        rule.hourly_rate = row[4] ? std::stod(row[4]) : 5.0;
        rule.max_daily_fee = row[5] ? std::stod(row[5]) : 50.0;
        rule.tier_config = row[6] ? row[6] : "";
        rule.description = row[7] ? row[7] : "";
        rule.is_active = true;
    }
    mysql_free_result(res);
    return rule;
}

double BillingService::calculateParkingFee(time_t inTime, time_t outTime, std::string& ruleDesc) {
    if (inTime >= outTime) {
        ruleDesc = "时间无效";
        return 0.0;
    }

    BillingRule rule = getActiveRule();
    if (!rule.is_active) {
        ruleDesc = "无有效计费规则";
        return 0.0;
    }
    ruleDesc = rule.rule_name;

    long long totalMinutes = std::difftime(outTime, inTime) / 60;

    if (totalMinutes <= rule.free_minutes) {
        return 0.0;
    }
    long long chargeMinutes = totalMinutes - rule.free_minutes;

    double hours = std::ceil(chargeMinutes / 60.0);
    double fee = hours * rule.hourly_rate;

    if (fee > rule.max_daily_fee) {
        fee = rule.max_daily_fee;
    }

    return fee;
}

// ===================== 【检查所有套餐，而不是只查一个】 =====================
bool BillingService::checkMonthlyPassValid(int userId, const std::string& plate, std::string& passInfo) {
    auto conn = getConnection();
    if (!conn) return false;

    std::string sql = "SELECT pass_type, end_date FROM MONTHLY_PASS "
        "WHERE user_id = " + std::to_string(userId) +
        " AND license_plate = '" + plate + "' "
        "AND is_active = 1 AND end_date >= NOW()";

    if (mysql_query(conn->get(), sql.c_str()) != 0) return false;
    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return false;

    bool hasValid = false;
    passInfo = "";
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {
        hasValid = true;
        if (!passInfo.empty()) {
            passInfo += " | ";
        }
        passInfo += std::string(row[0]) + "（至" + std::string(row[1]) + "）";
    }

    mysql_free_result(res);
    return hasValid;
}

// ===================== 【套餐查询：所有停车场通用】 =====================
std::vector<PassPlan> BillingService::getPassPlans() {
    std::vector<PassPlan> plans;
    auto conn = getConnection();
    if (!conn) return plans;

    // 读取所有启用的套餐（空 P_name = 全停车场通用）
    std::string sql = "SELECT id, plan_name, duration_days, price, description, P_name "
        "FROM pass_plan WHERE is_active = 1";

    if (mysql_query(conn->get(), sql.c_str()) != 0)
        return plans;

    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return plans;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        PassPlan p;
        p.id = std::stoi(row[0]);
        p.plan_name = row[1] ? row[1] : "";
        p.duration_days = row[2] ? std::stoi(row[2]) : 0;
        p.price = row[3] ? std::stod(row[3]) : 0.0;
        p.description = row[4] ? row[4] : "";
        p.P_name = row[5] ? row[5] : "";
        plans.push_back(p);
    }

    mysql_free_result(res);
    return plans;
}