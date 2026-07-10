#include "TransactionController.h"
namespace banking::controllers {
void TransactionController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub TransactionController");
    callback(resp);
}
}

int transaction_controller_force_link_val = 42;
