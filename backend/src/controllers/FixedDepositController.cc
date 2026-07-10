#include "FixedDepositController.h"
namespace banking::controllers {
void FixedDepositController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub FixedDepositController");
    callback(resp);
}
}
