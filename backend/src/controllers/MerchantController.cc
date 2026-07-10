#include "MerchantController.h"
namespace banking::controllers {
void MerchantController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub MerchantController");
    callback(resp);
}
}

int merchant_controller_force_link_val = 42;
