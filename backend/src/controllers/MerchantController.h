#pragma once
#include <drogon/HttpController.h>

namespace banking::controllers {

class MerchantController : public drogon::HttpController<MerchantController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MerchantController::getProfile, "/api/merchants/profile", drogon::Get, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(MerchantController::registerMerchant, "/api/merchants/register", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(MerchantController::createInvoice, "/api/merchants/invoices", drogon::Post, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(MerchantController::getInvoices, "/api/merchants/invoices", drogon::Get, "banking::middleware::AuthMiddleware");
    ADD_METHOD_TO(MerchantController::payInvoice, "/api/merchants/invoices/pay", drogon::Post, "banking::middleware::AuthMiddleware");
    METHOD_LIST_END

    void getProfile(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void registerMerchant(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void createInvoice(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getInvoices(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void payInvoice(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace banking::controllers
