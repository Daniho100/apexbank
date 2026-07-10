#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {
class LoanController : public drogon::HttpController<LoanController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(LoanController::stub, "/stub", drogon::Get);
    METHOD_LIST_END
    void stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
}
