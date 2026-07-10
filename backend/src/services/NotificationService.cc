#include "NotificationService.h"
#include <mutex>
#include <unordered_map>
#include <iostream>

namespace banking::services {

// Thread-safe static connections map
static std::mutex connectionsMutex;
static std::unordered_map<std::string, drogon::WebSocketConnectionPtr> wsActiveConnections;

void NotificationService::registerConnection(const std::string& userId, const drogon::WebSocketConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    wsActiveConnections[userId] = conn;
    LOG_INFO << "WebSocket connection registered for User: " << userId;
}

void NotificationService::unregisterConnection(const std::string& userId) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    wsActiveConnections.erase(userId);
    LOG_INFO << "WebSocket connection unregistered for User: " << userId;
}

void NotificationService::sendWebSocketPush(const std::string& userId, const Json::Value& payload) {
    std::lock_guard<std::mutex> lock(connectionsMutex);
    auto it = wsActiveConnections.find(userId);
    if (it != wsActiveConnections.end()) {
        Json::FastWriter writer;
        std::string jsonStr = writer.write(payload);
        it->second->send(jsonStr);
        LOG_INFO << "Real-time WebSocket notification pushed to User: " << userId;
    }
}

void NotificationService::sendNotification(
    const std::string& userId,
    const std::string& title,
    const std::string& content,
    const std::string& type
) {
    auto db = drogon::app().getDbClient();
    if (!db) {
        LOG_ERROR << "DB not available during notification insert.";
        return;
    }
    
    try {
        // 1. Insert into PostgreSQL
        db->execSqlSync(
            "INSERT INTO notifications (user_id, title, content, type) VALUES ($1, $2, $3, $4)",
            userId,
            title,
            content,
            type
        );
        
        // 2. Simulated Email Dispatch
        if (type == "email" || type == "all") {
            LOG_INFO << "------------------------------------------------------------";
            LOG_INFO << "[SIMULATED EMAIL DISPATCH]";
            LOG_INFO << "To User UUID : " << userId;
            LOG_INFO << "Subject      : " << title;
            LOG_INFO << "Body         : " << content;
            LOG_INFO << "------------------------------------------------------------";
        }
        
        // 3. Simulated Real-time Push via WebSocket
        Json::Value wsPayload;
        wsPayload["title"] = title;
        wsPayload["content"] = content;
        wsPayload["type"] = type;
        wsPayload["timestamp"] = "2026-07-09T14:45:00Z";
        
        sendWebSocketPush(userId, wsPayload);
        
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to send notification: " << e.what();
    }
}

} // namespace banking::services
