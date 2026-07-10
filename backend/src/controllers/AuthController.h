#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class AuthController : public drogon::HttpController<AuthController> {
public:
    PATH_LIST_BEGIN
    PATH_ADD("/api/auth");
    PATH_LIST_END

    METHOD_LIST_BEGIN
    METHOD_ADD(AuthController::registerUser, "/register", drogon::Post);
    METHOD_ADD(AuthController::loginUser, "/login", drogon::Post);
    METHOD_LIST_END

    void registerUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void loginUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
