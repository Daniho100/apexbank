#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {
class MerchantController : public drogon::HttpController<MerchantController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(MerchantController::stub, "/stub", drogon::Get);
    METHOD_LIST_END
    void stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};
}
