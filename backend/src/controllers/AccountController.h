#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {
class AccountController : public drogon::HttpController<AccountController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(AccountController::stub, "/stub", drogon::Get);
    METHOD_LIST_END
    void stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
}
