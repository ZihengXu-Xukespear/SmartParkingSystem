#pragma once
#include "crud_service.h"
#include "../model/billing.h"

class PassPlanService : public CrudService<PassPlan> {
public:
    static PassPlanService& instance();
    std::vector<PassPlan> getActivePlans(const std::string& P_name = "");
    std::vector<PassPlan> getAllPlans();
    bool addPlan(const PassPlan& plan);
    bool updatePlan(int id, const PassPlan& plan);
    bool deletePlan(int id);
    bool purchase(int userId, int planId, const std::string& licensePlate, std::string& error);
protected:
    PassPlan mapRow(MYSQL_ROW row) override;
private:
    PassPlanService() = default;
};
