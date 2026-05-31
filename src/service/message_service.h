#pragma once
#include "crud_service.h"
#include "../model/message.h"

class MessageService : public CrudService<Message> {
public:
    static MessageService& instance() {
        static MessageService inst;
        return inst;
    }

    bool sendMessage(int senderId, int receiverId, const std::string& content, std::string& error);
    crow::json::wvalue getConversations();
    std::vector<Message> getHistory(int userId, int otherUserId, int limit = 200);
    bool markAsRead(int receiverId, int senderId);
    int getUnreadCount(int userId);

protected:
    Message mapRow(MYSQL_ROW row) override;

private:
    MessageService() = default;
};
