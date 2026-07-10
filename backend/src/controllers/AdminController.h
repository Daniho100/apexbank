#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class AdminController : public drogon::HttpController<AdminController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminController::approveLoan, "/api/admin/loans/approve", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AdminController::getUsers, "/api/admin/users", drogon::Get, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AdminController::setUserStatus, "/api/admin/users/status", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AdminController::getAuditLogs, "/api/admin/audit-logs", drogon::Get, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AdminController::getUserActivity, "/api/admin/users/{userId}/activity", drogon::Get, "banking::middleware::AuthMiddleware");
    METHOD_LIST_END

    void approveLoan(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getUsers(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void setUserStatus(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getAuditLogs(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getUserActivity(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& userId);
};

} // namespace banking::controllers
