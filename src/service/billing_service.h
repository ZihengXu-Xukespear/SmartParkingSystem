#pragma once
#include "base_service.h"
#include "../model/billing.h"

class BillingService : public BaseService {
public:
    static BillingService& instance();
    std::vector<BillingRule> getRules();
    bool updateRule(int id, const BillingRule& rule);
    std::vector<MonthlyPass> getMonthlyPasses();
    bool addMonthlyPass(const MonthlyPass& pass);
    bool updateMonthlyPass(int id, const MonthlyPass& pass);
    bool deleteMonthlyPass(int id);

    // 获取当前生效的计费规则
    BillingRule getActiveRule();

    // 根据入场/离场时间，按规则计算停车费用
    double calculateParkingFee(time_t inTime, time_t outTime, std::string& ruleDesc);

    // 检查用户月卡是否在有效期内（免计费）
    bool checkMonthlyPassValid(int userId, const std::string& plate, std::string& passInfo);

private:
    BillingService() = default;
};
