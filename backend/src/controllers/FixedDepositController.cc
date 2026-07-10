#include "FixedDepositController.h"
namespace banking::controllers {
void FixedDepositController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub FixedDepositController");
    callback(resp);
}
}

int fixed_deposit_controller_force_link_val = 42;
