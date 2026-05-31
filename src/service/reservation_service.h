#pragma once
#include "crud_service.h"
#include "../model/reservation.h"
#include <unordered_set>
#include <chrono>

class ReservationService : public CrudService<Reservation> {
public:
    static ReservationService& instance();
    bool create(const std::string& plate, const std::string& P_name, int userId, std::string& error);
    bool create(const std::string& plate, const std::string& P_name, int userId, int spotNum, std::string& error);
    std::vector<Reservation> list();
    bool cancel(int id);
    std::vector<Reservation> getHistory(const std::string& startDate = "", const std::string& endDate = "", int limit = 200, int offset = 0);
    crow::json::wvalue getSpotStatus(const std::string& P_name, int totalSpots);
    void cleanExpiredReservations();
protected:
    Reservation mapRow(MYSQL_ROW row) override;
private:
    ReservationService() = default;
    std::chrono::steady_clock::time_point lastCleanup_;
};
