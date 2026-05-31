#pragma once
#include "crud_service.h"
#include "../model/blacklist.h"
#include "../model/interception_log.h"

class BlacklistService : public CrudService<BlacklistEntry> {
public:
    static BlacklistService& instance();
    std::vector<BlacklistEntry> getAll();
    bool isBlacklisted(const std::string& plate, std::string* reason_out = nullptr);
    bool add(const std::string& plate, const std::string& reason, std::string& error);
    bool remove(int id);

    bool logInterception(const std::string& plate, const std::string& reason);
    std::vector<InterceptionLog> getRecentInterceptions(int limit = 50);
    int getRecentInterceptionCount(int hours = 24);

protected:
    BlacklistEntry mapRow(MYSQL_ROW row) override;
private:
    BlacklistService() = default;
};
