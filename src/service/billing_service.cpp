#include "billing_service.h"
#include "base_service.h" 

BillingService& BillingService::instance() {
    static BillingService inst;
    return inst;
}

std::vector<BillingRule> BillingService::getRules() {
    std::vector<BillingRule> rules;
    auto conn = getConnection();
    if (!conn) return rules;

    if (mysql_query(conn->get(), "SELECT id,rule_name,rule_type,free_minutes,hourly_rate,max_daily_fee,tier_config,description,is_active FROM BILLING_RULE ORDER BY id") != 0)
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
        rules.push_back(r);
    }
    mysql_free_result(res);
    return rules;
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
        " WHERE id=" + std::to_string(id);
    return executeQuery(mysql, sql);
}

std::vector<MonthlyPass> BillingService::getMonthlyPasses() {
    std::vector<MonthlyPass> passes;
    auto conn = getConnection();
    if (!conn) return passes;

    if (mysql_query(conn->get(), "SELECT id,license_plate,pass_type,start_date,end_date,fee,is_active,user_id,plan_id FROM MONTHLY_PASS ORDER BY id") != 0)
        return passes;
    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return passes;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        MonthlyPass p;
        p.id = std::stoi(row[0]);
        p.license_plate = row[1] ? row[1] : "";
        p.pass_type = row[2] ? row[2] : "";
        p.start_date = row[3] ? row[3] : "";
        p.end_date = row[4] ? row[4] : "";
        p.fee = row[5] ? std::stod(row[5]) : 0.0;
        p.is_active = row[6] ? std::stoi(row[6]) == 1 : true;
        p.user_id = row[7] ? std::stoi(row[7]) : 0;
        p.plan_id = row[8] ? std::stoi(row[8]) : 0;
        passes.push_back(p);
    }
    mysql_free_result(res);
    return passes;
}

bool BillingService::addMonthlyPass(const MonthlyPass& pass) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    std::string sql = "INSERT INTO MONTHLY_PASS (license_plate,pass_type,start_date,end_date,fee,is_active,user_id,plan_id) VALUES (" +
        quote(mysql, pass.license_plate) + "," +
        quote(mysql, pass.pass_type) + "," +
        quote(mysql, pass.start_date) + "," +
        quote(mysql, pass.end_date) + "," +
        std::to_string(pass.fee) + ",1," +
        std::to_string(pass.user_id) + "," + std::to_string(pass.plan_id) + ")";
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

// 获取当前生效的计费规则（按is_active=1排序取第一条）
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

// 根据入场/离场时间，按规则计算停车费用（含免费时长、日封顶）
double BillingService::calculateParkingFee(time_t inTime, time_t outTime, std::string& ruleDesc) {
    // 1. 参数合法性校验
    if (inTime >= outTime) {
        ruleDesc = "时间参数无效";
        return 0.0;
    }

    // 2. 获取当前生效的计费规则
    BillingRule rule = getActiveRule();
    if (!rule.is_active) {
        ruleDesc = "无有效计费规则";
        return 0.0;
    }
    ruleDesc = rule.rule_name;

    // 3. 计算停车总分钟数
    long long totalMinutes = std::difftime(outTime, inTime) / 60;

    // 4. 扣除免费时长
    if (totalMinutes <= rule.free_minutes) {
        return 0.0;
    }
    long long chargeMinutes = totalMinutes - rule.free_minutes;

    // 5. 计算小时数（向上取整）
    double hours = std::ceil(chargeMinutes / 60.0);
    double fee = hours * rule.hourly_rate;

    // 6. 日封顶处理
    if (fee > rule.max_daily_fee) {
        fee = rule.max_daily_fee;
    }

    return fee;
}

// 检查用户月卡是否在有效期内（免计费）
bool BillingService::checkMonthlyPassValid(int userId, const std::string& plate, std::string& passInfo) {
    auto conn = getConnection();  // 这里修复！！！
    if (!conn) return false;

    std::string sql = "SELECT id,pass_type,end_date FROM MONTHLY_PASS "
        "WHERE user_id=" + std::to_string(userId) +
        " AND license_plate='" + plate + "' AND is_active=1 AND end_date >= NOW() LIMIT 1";

    if (mysql_query(conn->get(), sql.c_str()) != 0) return false;
    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        passInfo = "有效" + std::string(row[1]) + "月卡，有效期至：" + std::string(row[2]);
        mysql_free_result(res);
        return true;
    }

    mysql_free_result(res);
    passInfo = "无有效月卡";
    return false;
}