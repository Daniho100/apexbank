#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class AuthController : public drogon::HttpController<AuthController> {
public:
    static const std::vector<std::string> &paths() {
        static const std::vector<std::string> _paths = { "/api/auth" };
        return _paths;
    }

    METHOD_LIST_BEGIN
    METHOD_ADD(AuthController::registerUser, "/register", drogon::Post);
    METHOD_ADD(AuthController::loginUser, "/login", drogon::Post);
    METHOD_LIST_END

    void registerUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void loginUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
