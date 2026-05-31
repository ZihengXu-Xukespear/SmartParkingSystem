#include "pass_plan_service.h"
#include "balance_service.h"

PassPlanService& PassPlanService::instance() {
    static PassPlanService inst;
    return inst;
}

std::vector<PassPlan> PassPlanService::getActivePlans(const std::string& P_name) {
    std::string sql = "SELECT id,plan_name,duration_days,price,description,is_active,COALESCE(P_name,'') FROM PASS_PLAN WHERE is_active=1";
    if (!P_name.empty()) {
        auto conn = getConnection();
        if (conn) sql += " AND P_name=" + quote(conn->get(), P_name);
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
    std::string planName = row[0] ? row[0] : "月卡";
    int days = row[1] ? std::stoi(row[1]) : 30;
    double price = row[2] ? std::stod(row[2]) : 0;
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
        " AND is_active=1 AND end_date >= CURDATE() ORDER BY end_date DESC LIMIT 1";
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
        sql = "INSERT INTO MONTHLY_PASS (license_plate,pass_type,start_date,end_date,fee,is_active,user_id,plan_id) VALUES (" +
            quote(mysql, licensePlate) + "," + quote(mysql, planName) + ",CURDATE(),DATE_ADD(CURDATE(),INTERVAL " +
            std::to_string(days) + " DAY)," + std::to_string(price) + ",1," +
            std::to_string(userId) + "," + std::to_string(planId) + ")";
        if (mysql_query(mysql, sql.c_str()) != 0) {
            BalanceService::instance().refund(userId, price, "refund", "月卡购买失败退款 " + licensePlate);
            error = "创建月卡失败，已退款";
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
