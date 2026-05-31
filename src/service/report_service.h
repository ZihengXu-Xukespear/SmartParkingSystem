#pragma once
#include "base_service.h"
#include <vector>
#include <string>

class ReportService : public BaseService {
public:
    static ReportService& instance();

    struct RevenueSummary {
        double today_income = 0;
        double month_income = 0;
        double total_income = 0;
        double parking_fees = 0;
        double pass_sales = 0;
        double reservation_prepaid = 0;
    };

    RevenueSummary getSummary();
    std::vector<std::pair<std::string, double>> getDailyRevenue(int days = 30);

    struct DailyReportRow {
        std::string date;
        double revenue = 0;
        int parking_count = 0;
        double pass_sales = 0;
    };

    std::vector<DailyReportRow> getDailyReport(const std::string& startDate, const std::string& endDate);

    struct RevenuePrediction {
        double daily_average = 0;
        double predicted_monthly = 0;
        int days_remaining = 0;
        int days_in_month = 0;
    };

    RevenuePrediction getRevenuePrediction();

private:
    ReportService() = default;
};
