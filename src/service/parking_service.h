#pragma once
#include "base_service.h"
#include "../model/parking_lot.h"

class ParkingService : public BaseService {
public:
    static ParkingService& instance();
    ParkingLot getStatus(const std::string& P_name = "");
    std::vector<ParkingLot> getAllLots();
    bool addLot(const std::string& P_name, int total_count, double fee);
    bool deleteLot(int id, std::string& error);
    bool updateSettings(const std::string& P_name, double fee, int total_count, const std::string& new_name = "");
private:
    ParkingService() = default;
};
