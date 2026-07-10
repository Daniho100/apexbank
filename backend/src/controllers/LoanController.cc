#include "LoanController.h"
namespace banking::controllers {
void LoanController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub LoanController");
    callback(resp);
}
}
