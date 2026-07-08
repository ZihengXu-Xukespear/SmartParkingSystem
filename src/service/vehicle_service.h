#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <ctime>
#include "crud_service.h"
#include "../model/car_record.h"
#include "../model/parking_lot.h"
#include "../config.h"

class VehicleService : public CrudService<CarRecord> {
public:
    static VehicleService& instance();
    bool checkIn(const std::string& plate, const std::string& billing_type, std::string& error);
    bool checkIn(const std::string& plate, const std::string& billing_type, const std::string& P_name, std::string& error);
    bool checkIn(const std::string& plate, const std::string& billing_type, const std::string& P_name, int spotNum, std::string& error);
    bool checkIn(const std::string& plate, const std::string& billing_type, const std::string& P_name, int spotNum, int operatorId, std::string& error);
    bool checkIn(const std::string& plate, const std::string& billing_type, const std::string& P_name, int spotNum, int operatorId, const std::string& charging_plan, std::string& error);
    bool checkOut(const std::string& plate, int userId, double& fee, CarRecord& record, std::string& error);
    std::vector<CarRecord> queryRecords(const std::string& plate, const std::string& start_date, const std::string& end_date, int userId = 0, const std::string& userRole = "");
    bool deleteRecord(int id);
    std::vector<CarRecord> getParkedVehicles(const std::string& plate_filter = "", int userId = 0, const std::string& userRole = "");

protected:
    CarRecord mapRow(MYSQL_ROW row) override;
private:
    VehicleService() = default;
    double calculateFee(MYSQL* mysql, const std::string& plate, const std::string& check_in_time,
                        const std::string& billing_type, const std::string& P_name, int reservationId = 0);
    bool validatePlate(const std::string& plate);
};
