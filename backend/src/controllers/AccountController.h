#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class AccountController : public drogon::HttpController<AccountController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AccountController::getAccounts, "/api/accounts", drogon::Get, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AccountController::deposit, "/api/accounts/deposit", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AccountController::withdraw, "/api/accounts/withdraw", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AccountController::transfer, "/api/accounts/transfer", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(AccountController::getNotifications, "/api/notifications", drogon::Get, "banking::middleware::AuthMiddleware");
    METHOD_LIST_END

    void getAccounts(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void deposit(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void withdraw(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void transfer(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getNotifications(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    static void runMigrations();
};

} // namespace banking::controllers
