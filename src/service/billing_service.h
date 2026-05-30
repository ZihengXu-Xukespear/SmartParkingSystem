#pragma once
#include "base_service.h"
#include "../model/billing.h"

class BillingService : public BaseService {
public:
    static BillingService& instance();
    std::vector<BillingRule> getRules();
    bool addRule(const BillingRule& rule);
    bool updateRule(int id, const BillingRule& rule);
    bool deleteRule(int id);
    std::vector<MonthlyPass> getMonthlyPasses();
    bool addMonthlyPass(const MonthlyPass& pass);
    bool updateMonthlyPass(int id, const MonthlyPass& pass);
    bool deleteMonthlyPass(int id);

    // ��ȡ��ǰ��Ч�ļƷѹ���
    BillingRule getActiveRule();

    // �����볡/�볡ʱ�䣬���������ͣ������
    double calculateParkingFee(time_t inTime, time_t outTime, std::string& ruleDesc);

    // ����û��¿��Ƿ�����Ч���ڣ���Ʒѣ�
    bool checkMonthlyPassValid(int userId, const std::string& plate, std::string& passInfo);

    std::vector<MonthlyPass> getMonthlyPasses(int userId);

    std::vector<PassPlan> getPassPlans();

private:
    BillingService() = default;
};
