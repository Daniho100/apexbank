#pragma once
#include <string>
#include <drogon/drogon.h>
#include <drogon/WebSocketConnection.h>
#include <json/json.h>

namespace banking::services {

class NotificationService {
public:
    // Send a notification (persists to DB, logs simulated email, and pushes to active WS connection)
    static void sendNotification(
        const std::string& userId,
        const std::string& title,
        const std::string& content,
        const std::string& type = "in_app" // "in_app", "email", "all"
    );

    // Register active WebSocket connection for a user
    static void registerConnection(const std::string& userId, const drogon::WebSocketConnectionPtr& conn);

    // Remove active WebSocket connection
    static void unregisterConnection(const std::string& userId);

private:
    static void sendWebSocketPush(const std::string& userId, const Json::Value& payload);
};

} // namespace banking::services
