#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class TransactionController : public drogon::HttpController<TransactionController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(TransactionController::getTransactions, "/api/transactions", drogon::Get, "banking::middleware::AuthMiddleware");
    METHOD_LIST_END

    void getTransactions(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
