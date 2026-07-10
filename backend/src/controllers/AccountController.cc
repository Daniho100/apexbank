#include "AccountController.h"
namespace banking::controllers {
void AccountController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub AccountController");
    callback(resp);
}
}
