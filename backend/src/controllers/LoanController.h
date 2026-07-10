#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class LoanController : public drogon::HttpController<LoanController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LoanController::getLoans, "/api/loans", drogon::Get, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(LoanController::apply, "/api/loans/apply", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(LoanController::repay, "/api/loans/repay", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(LoanController::getSchedules, "/api/loans/schedules", drogon::Get, "banking::middleware::AuthMiddleware");
    METHOD_LIST_END

    void getLoans(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void apply(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void repay(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getSchedules(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
