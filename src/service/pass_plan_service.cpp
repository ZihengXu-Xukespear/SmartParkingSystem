#include "pass_plan_service.h"
#include "balance_service.h"
#include "database/db_init.h"

#include <vector>
#include <map>
#include <sstream>
#include <mysql.h>

PassPlanService& PassPlanService::instance() {
    static PassPlanService inst;
    return inst;
}

std::vector<PassPlan> PassPlanService::getActivePlans(const std::string& P_name) {
    std::vector<PassPlan> plans;
    auto conn = getConnection();
    if (!conn) return plans;

    std::string sql;
    if (P_name.empty()) {
        sql = "SELECT id, plan_name, duration_days, price, description, P_name, is_active "
            "FROM pass_plan WHERE is_active = 1 AND P_name = ''";
    }
    else {
        sql = "SELECT id, plan_name, duration_days, price, description, P_name, is_active "
            "FROM pass_plan WHERE is_active = 1 "
            "AND (P_name = '' OR P_name = '" + P_name + "')";
    }

    if (mysql_query(conn->get(), sql.c_str())) return plans;
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
        p.is_active = row[6] ? (std::stoi(row[6]) == 1) : true;
        plans.push_back(p);
    }

    mysql_free_result(res);
    return plans;
}

std::vector<PassPlan> PassPlanService::getAllPlans() {
    return list("SELECT id,plan_name,duration_days,price,description,is_active,COALESCE(P_name,'') FROM PASS_PLAN ORDER BY duration_days");
}

bool PassPlanService::addPlan(const PassPlan& plan) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "INSERT INTO PASS_PLAN (plan_name,duration_days,price,description,is_active,P_name) VALUES (" +
        quote(mysql, plan.plan_name) + "," + std::to_string(plan.duration_days) + "," +
        std::to_string(plan.price) + "," + quote(mysql, plan.description) + ",1," +
        quote(mysql, plan.P_name) + ")";
    return executeQuery(mysql, sql);
}

bool PassPlanService::updatePlan(int id, const PassPlan& plan) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "UPDATE PASS_PLAN SET plan_name=" + quote(mysql, plan.plan_name) +
        ", duration_days=" + std::to_string(plan.duration_days) +
        ", price=" + std::to_string(plan.price) +
        ", description=" + quote(mysql, plan.description) +
        ", is_active=" + std::to_string(plan.is_active ? 1 : 0) +
        ", P_name=" + quote(mysql, plan.P_name) +
        " WHERE id=" + std::to_string(id);
    return executeQuery(mysql, sql);
}

bool PassPlanService::deletePlan(int id) {
    return deleteById(id);
}

// ========================= 购买套餐（不自动续期，直接新增） =========================
bool PassPlanService::purchase(int userId, int planId, const std::string& licensePlate, std::string& error) {
    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();
    Transaction tx(mysql);

    std::string sql = "SELECT plan_name,duration_days,price FROM PASS_PLAN WHERE id=" +
        std::to_string(planId) + " AND is_active=1";
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "套餐不存在"; return false; }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res || mysql_num_rows(res) == 0) {
        if (res) mysql_free_result(res);
        error = "套餐不存在或已下架";
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    std::string planName = row[0] ? row[0] : "套餐";
    int days = row[1] ? std::stoi(row[1]) : 30;
    double price = row[2] ? std::stod(row[2]) : 0;
    mysql_free_result(res);

    double balance = BalanceService::instance().getBalance(userId);
    if (balance < price) {
        error = "余额不足";
        return false;
    }

    std::string deductError;
    if (!BalanceService::instance().deduct(userId, price, "purchase", "购买" + planName + " " + licensePlate, deductError)) {
        error = deductError;
        return false;
    }

    // 直接插入新记录，不合并
    sql = "INSERT INTO MONTHLY_PASS (user_id, license_plate, pass_type, start_date, end_date, fee, is_active) VALUES (" +
        std::to_string(userId) + "," +
        quote(mysql, licensePlate) + "," +
        quote(mysql, planName) + ",CURDATE(),DATE_ADD(CURDATE(),INTERVAL " +
        std::to_string(days) + " DAY)," +
        std::to_string(price) + ",1)";

    if (mysql_query(mysql, sql.c_str()) != 0) {
        BalanceService::instance().refund(userId, price, "refund", "购买失败退款 " + licensePlate);
        error = "购买失败，已退款";
        return false;
    }

    if (!tx.commit()) {
        BalanceService::instance().refund(userId, price, "refund", "事务提交失败退款 " + licensePlate);
        error = "系统异常，已退款";
        return false;
    }
    return true;
}

PassPlan PassPlanService::mapRow(MYSQL_ROW row) {
    PassPlan p;
    p.id = std::stoi(row[0]);
    p.plan_name = row[1] ? row[1] : "";
    p.duration_days = row[2] ? std::stoi(row[2]) : 30;
    p.price = row[3] ? std::stod(row[3]) : 0;
    p.description = row[4] ? row[4] : "";
    p.is_active = row[5] ? std::stoi(row[5]) : 1;
    p.P_name = row[6] ? row[6] : "";
    return p;
}

// ========================= 【核心】按车牌合并：名称 + 时间 + 金额 =========================
std::vector<UserPass> PassPlanService::getUserPurchasedPasses(int user_id) {
    std::vector<UserPass> passes;
    auto conn_guard = getConnection();
    if (!conn_guard) return passes;
    MYSQL* conn = conn_guard->get();

    std::string sql = "SELECT id, user_id, license_plate, pass_type, start_date, end_date, fee, is_active "
        "FROM MONTHLY_PASS WHERE user_id = " + std::to_string(user_id) + " AND is_active = 1";

    if (mysql_query(conn, sql.c_str()) != 0) return passes;
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return passes;

    std::map<std::string, UserPass> plateMap;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        UserPass p;
        p.id = std::stoi(row[0]);
        p.user_id = std::stoi(row[1]);
        p.license_plate = row[2] ? row[2] : "";
        p.pass_type = row[3] ? row[3] : "";
        p.start_date = row[4] ? row[4] : "";
        p.end_date = row[5] ? row[5] : "";
        p.fee = row[6] ? std::stod(row[6]) : 0.0;
        p.is_active = row[7] ? (std::stoi(row[7]) != 0) : false;

        if (plateMap.find(p.license_plate) == plateMap.end()) {
            plateMap[p.license_plate] = p;
        }
        else {
            plateMap[p.license_plate].pass_type += " + " + p.pass_type;
            if (p.end_date > plateMap[p.license_plate].end_date) {
                plateMap[p.license_plate].end_date = p.end_date;
            }
            plateMap[p.license_plate].fee += p.fee;
        }
    }

    mysql_free_result(res);
    for (auto& kv : plateMap) passes.push_back(kv.second);
    return passes;
}

PassPlan PassPlanService::getById(int id) {
    PassPlan plan;
    auto conn = getConnection();
    if (!conn) return plan;

    std::string sql = "SELECT id, plan_name, duration_days, price, description, P_name, is_active "
        "FROM pass_plan WHERE id = " + std::to_string(id);

    if (mysql_query(conn->get(), sql.c_str()) != 0)
        return plan;

    MYSQL_RES* res = mysql_store_result(conn->get());
    if (!res) return plan;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        plan.id = std::stoi(row[0]);
        plan.plan_name = row[1] ? row[1] : "";
        plan.duration_days = row[2] ? std::stoi(row[2]) : 0;
        plan.price = row[3] ? std::stod(row[3]) : 0.0;
        plan.description = row[4] ? row[4] : "";
        plan.P_name = row[5] ? row[5] : "";
        plan.is_active = row[6] ? (std::stoi(row[6]) == 1) : true;
    }

    mysql_free_result(res);
    return plan;
}