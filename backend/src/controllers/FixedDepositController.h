#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class FixedDepositController : public drogon::HttpController<FixedDepositController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FixedDepositController::getDeposits, "/api/fixed-deposits", drogon::Get, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(FixedDepositController::create, "/api/fixed-deposits/create", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(FixedDepositController::liquidate, "/api/fixed-deposits/liquidate", drogon::Post, "banking::middleware::AuthMiddleware");
    METHOD_LIST_END

    void getDeposits(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void create(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void liquidate(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
