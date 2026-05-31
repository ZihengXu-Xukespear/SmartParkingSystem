#pragma once
#include <string>
#include "base_service.h"

class PlateService : public BaseService {
public:
    static PlateService& instance();

    struct PlateResult {
        std::string plate_number;
        double confidence;
        std::string color;
        std::string message;
    };

    struct PlateRegistrationInfo {
        std::string plate_number;
        bool is_registered = false;
        bool in_parking = false;
        bool has_monthly_pass = false;
        bool is_blacklisted = false;
        std::string last_check_in;
        std::string monthly_pass_end;
        std::string blacklist_reason;
        std::string message;
    };

    // Recognize plate from base64 image data
    PlateResult recognize(const std::string& image_data);

    // Check if a plate is registered in the system
    PlateRegistrationInfo checkRegistration(const std::string& plate);

    static bool validatePlate(const std::string& plate);

private:
    PlateService() = default;
};
