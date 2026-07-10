#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class AuthController : public drogon::HttpController<AuthController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(AuthController::registerUser, "^/api/auth/register", drogon::Post);
    METHOD_ADD(AuthController::loginUser, "^/api/auth/login", drogon::Post);
    METHOD_LIST_END

    void registerUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void loginUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
