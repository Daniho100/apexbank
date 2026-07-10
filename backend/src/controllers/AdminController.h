#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {
class AdminController : public drogon::HttpController<AdminController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(AdminController::stub, "/stub", drogon::Get);
    METHOD_LIST_END
    void stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
}
