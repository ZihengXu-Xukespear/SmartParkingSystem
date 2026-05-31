#pragma once
#include <string>
#include <unordered_set>
#include <vector>

namespace Permissions {

// Permission nodes
constexpr auto SYSTEM_INIT      = "system.init";
constexpr auto USER_VIEW        = "user.view";
constexpr auto USER_MANAGE      = "user.manage";
constexpr auto PARKING_VIEW     = "parking.view";
constexpr auto PARKING_SETTINGS = "parking.settings";
constexpr auto BILLING_VIEW     = "billing.view";
constexpr auto BILLING_MANAGE   = "billing.manage";
constexpr auto VEHICLE_CHECKIN  = "vehicle.checkin";
constexpr auto VEHICLE_CHECKOUT = "vehicle.checkout";
constexpr auto VEHICLE_QUERY    = "vehicle.query";
constexpr auto VEHICLE_DELETE   = "vehicle.delete";
constexpr auto RESERVATION_CREATE = "reservation.create";
constexpr auto RESERVATION_VIEW   = "reservation.view";
constexpr auto RESERVATION_CANCEL = "reservation.cancel";
constexpr auto PLATE_RECOGNIZE  = "plate.recognize";
constexpr auto BALANCE_VIEW    = "balance.view";
constexpr auto BALANCE_MANAGE  = "balance.manage";
constexpr auto PASS_PLAN_MANAGE = "passplan.manage";
constexpr auto BLACKLIST_MANAGE = "vehicle.blacklist";
constexpr auto REPORT_VIEW     = "report.view";
constexpr auto EXPORT_DATA     = "vehicle.export";
constexpr auto NOTICE_MANAGE   = "notice.manage";
constexpr auto MESSAGE_SEND    = "message.send";
constexpr auto MESSAGE_MANAGE  = "message.manage";

// All permissions list
inline const std::vector<const char*> ALL = {
    SYSTEM_INIT,
    USER_VIEW, USER_MANAGE,
    PARKING_VIEW, PARKING_SETTINGS,
    BILLING_VIEW, BILLING_MANAGE,
    VEHICLE_CHECKIN, VEHICLE_CHECKOUT, VEHICLE_QUERY, VEHICLE_DELETE,
    RESERVATION_CREATE, RESERVATION_VIEW, RESERVATION_CANCEL,
    PLATE_RECOGNIZE,
    BALANCE_VIEW, BALANCE_MANAGE,
    PASS_PLAN_MANAGE,
    BLACKLIST_MANAGE,
    REPORT_VIEW, EXPORT_DATA,
    NOTICE_MANAGE,
    MESSAGE_SEND, MESSAGE_MANAGE
};

// Role → permissions mapping
inline std::unordered_set<std::string> getPermissionsForRole(const std::string& role) {
    if (role == "admin" || role == "root") {
        std::unordered_set<std::string> all;
        for (auto p : ALL) all.insert(p);
        return all;
    }
    // "user" — regular users: view parking, billing, own records, reservations, balance
    return {
        PARKING_VIEW,
        BILLING_VIEW,
        RESERVATION_CREATE, RESERVATION_VIEW, RESERVATION_CANCEL,
        BALANCE_VIEW,
        MESSAGE_SEND
    };
}

} // namespace Permissions
