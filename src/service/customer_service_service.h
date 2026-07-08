#pragma once
#include "crud_service.h"
#include "../model/cs_message.h"
#include "crow.h"
#include <string>
#include <vector>

// Result of a user "send" action.
struct AskResult {
    std::string mode;          // "ai" (AI answered) | "human" (routed to a human agent)
    std::string reply;         // AI reply in ai mode; empty in human mode
    bool show_actions = false; // show the [继续询问] / [转接人工客服] buttons
    int session_id = 0;
    std::string status;        // current session status
    std::string error;
    bool ok = false;
};

// AI-first + human-handoff customer service.
//   user flow: ask() -> AI answers (with tools) -> show actions -> escalate() -> admin
class CustomerServiceService : public CrudService<CSMessage> {
public:
    static CustomerServiceService& instance() {
        static CustomerServiceService inst;
        return inst;
    }

    // ---- user-facing ----
    AskResult ask(int userId, const std::string& text);
    bool escalate(int userId, std::string& error);
    crow::json::wvalue getSession(int userId);   // active session + messages + status
    bool markReadByUser(int userId);

    // ---- admin-facing ----
    crow::json::wvalue adminListSessions(const std::string& statusFilter);
    crow::json::wvalue adminGetSession(int sessionId);
    bool adminReply(int adminId, int sessionId, const std::string& content, std::string& error);
    bool adminClose(int sessionId, std::string& error);
    int adminPendingCount();   // escalated sessions still waiting for a human
    int adminUnreadCount();    // user messages not yet seen by any admin
    bool adminMarkRead(int sessionId);

protected:
    CSMessage mapRow(MYSQL_ROW row) override;

private:
    CustomerServiceService() = default;

    // session helpers
    int getOrCreateSession(int userId);          // returns latest non-closed session (creates 'ai' if none)
    int getActiveSessionId(int userId);          // latest non-closed session or 0 (no create)
    std::string getSessionStatus(int sessionId);
    bool setSessionStatus(int sessionId, const std::string& status, int handledBy);
    bool appendMessage(int sessionId, int userId, const std::string& senderType,
                       int senderId, const std::string& content, bool unreadByAdmin);
    void touchSession(int sessionId);

    // agent building blocks
    std::string buildSystemPrompt(int userId);
    std::vector<crow::json::wvalue> buildContextMessages(int sessionId, int userId);
    std::string runAgent(int sessionId, int userId);   // executes the tool-calling loop, returns final reply
    std::string dispatchTool(const std::string& name, const std::string& argsJson, int userId);

    // read-only tools (return JSON strings the model can read)
    std::string toolGetBalance(int userId);
    std::string toolGetVehicles(int userId);
    std::string toolGetParkingStatus(int userId, const std::string& plate);
    std::string toolEstimateFee(const std::string& ruleType, int minutes);
    std::string toolGetMonthlyPass(int userId);
    std::string toolCheckBlacklist(const std::string& plate);
    std::string toolGetPricing();
    std::string toolGetOccupancy();
};
