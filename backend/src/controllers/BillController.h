#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {
class BillController : public drogon::HttpController<BillController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(BillController::stub, "/stub", drogon::Get);
    METHOD_LIST_END
    void stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
}
