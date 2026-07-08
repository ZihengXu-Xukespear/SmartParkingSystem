#include "pass_plan_service.h"
#include "balance_service.h"

#include "database/db_init.h"

#include <vector>
#include <sstream>
#include <mysql.h>

PassPlanService& PassPlanService::instance() {
    static PassPlanService inst;
    return inst;
}

std::vector<PassPlan> PassPlanService::getActivePlans(const std::string& P_name) {
    std::string sql = "SELECT id,plan_name,duration_days,price,description,is_active,COALESCE(P_name,'') FROM PASS_PLAN WHERE is_active=1";
    if (!P_name.empty()) {
        auto conn = getConnection();
        if (conn) sql += " AND (P_name=" + quote(conn->get(), P_name) + " OR P_name='' OR P_name IS NULL)";
    }
    sql += " ORDER BY duration_days";
    return list(sql);
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

bool PassPlanService::purchase(int userId, int planId, const std::string& licensePlate, std::string& error) {
    auto conn = getConnection();
    if (!conn) { error = "数据库连接失败"; return false; }
    MYSQL* mysql = conn->get();
    Transaction tx(mysql);

    std::string sql = "SELECT plan_name,duration_days,price,COALESCE(P_name,'') FROM PASS_PLAN WHERE id=" +
        std::to_string(planId) + " AND is_active=1";
    if (mysql_query(mysql, sql.c_str()) != 0) { error = "套餐不存在"; return false; }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res || mysql_num_rows(res) == 0) {
        if (res) mysql_free_result(res);
        error = "套餐不存在或已下架";
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    std::string planName = row[0] ? row[0] : "月卡";
    int days = row[1] ? std::stoi(row[1]) : 30;
    double price = row[2] ? std::stod(row[2]) : 0;
    std::string planPName = row[3] ? row[3] : "";
    mysql_free_result(res);

    double balance = BalanceService::instance().getBalance(userId);
    if (balance < price) { error = "余额不足，需要" + std::to_string(price) + "元，当前余额" + std::to_string(balance) + "元"; return false; }

    std::string deductError;
    if (!BalanceService::instance().deduct(userId, price, "purchase",
        "购买" + planName + " " + licensePlate, deductError)) {
        error = deductError;
        return false;
    }

    sql = "SELECT id,end_date FROM MONTHLY_PASS WHERE license_plate=" + quote(mysql, licensePlate) +
        " AND is_active=1 AND end_date >= CURDATE() AND P_name=" + quote(mysql, planPName) +
        " ORDER BY end_date DESC LIMIT 1";
    int existingId = 0;
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* eres = mysql_store_result(mysql);
        if (eres && mysql_num_rows(eres) > 0) {
            MYSQL_ROW erow = mysql_fetch_row(eres);
            existingId = std::stoi(erow[0]);
        }
        if (eres) mysql_free_result(eres);
    }

    if (existingId > 0) {
        sql = "UPDATE MONTHLY_PASS SET end_date=DATE_ADD(end_date,INTERVAL " +
            std::to_string(days) + " DAY), fee=fee+" + std::to_string(price) +
            " WHERE id=" + std::to_string(existingId);
        if (mysql_query(mysql, sql.c_str()) != 0) {
            BalanceService::instance().refund(userId, price, "refund", "套餐续费失败退款 " + licensePlate);
            error = "续费失败，已退款";
            return false;
        }
    } else {
        sql = "INSERT INTO MONTHLY_PASS (license_plate,pass_type,start_date,end_date,fee,is_active,user_id,plan_id,P_name) VALUES (" +
            quote(mysql, licensePlate) + "," + quote(mysql, planName) + ",CURDATE(),DATE_ADD(CURDATE(),INTERVAL " +
            std::to_string(days) + " DAY)," + std::to_string(price) + ",1," +
            std::to_string(userId) + "," + std::to_string(planId) + "," + quote(mysql, planPName) + ")";
        if (mysql_query(mysql, sql.c_str()) != 0) {
            BalanceService::instance().refund(userId, price, "refund", planName + "购买失败退款 " + licensePlate);
            error = "创建" + planName + "失败，已退款";
            return false;
        }
    }

    if (!tx.commit()) {
        BalanceService::instance().refund(userId, price, "refund", "事务提交失败退款 " + licensePlate);
        error = "事务提交失败，已退款";
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

std::vector<UserPass> PassPlanService::getUserPurchasedPasses(int user_id) {
    std::vector<UserPass> passes;

    // 1. 先拿到连接池的 ConnGuard 指针
    auto conn_guard = getConnection();
    if (!conn_guard) {
        return passes;
    }

    // 2. 取出内部的 MYSQL* 句柄
    MYSQL* conn = conn_guard->get();

    std::stringstream sql;
    sql << "SELECT id, user_id, license_plate, pass_type, start_date, end_date, fee, is_active, COALESCE(P_name,'') "
        << "FROM MONTHLY_PASS WHERE user_id = " << user_id;

    // 3. 用取出的 MYSQL* 调用 C API
    if (mysql_query(conn, sql.str().c_str()) != 0) {
        return passes;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        return passes;
    }

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
        p.P_name = row[8] ? row[8] : "";
        passes.push_back(p);
    }

    mysql_free_result(res);
    // 4. 不需要手动关闭连接，ConnGuard 析构时会自动归还
    return passes;
}