#include "AdminController.h"
namespace banking::controllers {
void AdminController::stub(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Stub AdminController");
    callback(resp);
}
}
