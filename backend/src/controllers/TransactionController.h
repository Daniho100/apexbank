#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {
class TransactionController : public drogon::HttpController<TransactionController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(TransactionController::stub, "/stub", drogon::Get);
    METHOD_LIST_END
    void stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
}
