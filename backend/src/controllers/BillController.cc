#include "BillController.h"
namespace banking::controllers {
void BillController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub BillController");
    callback(resp);
}
}

int bill_controller_force_link_val = 42;
