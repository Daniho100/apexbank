#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class BillController : public drogon::HttpController<BillController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(BillController::pay, "/api/utilities/pay", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(BillController::validate, "/api/utilities/validate", drogon::Post, "banking::middleware::AuthMiddleware");
    METHOD_LIST_END

    void pay(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void validate(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
