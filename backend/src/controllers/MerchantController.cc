#include "MerchantController.h"
#include "services/MerchantManager.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::controllers {

void MerchantController::getProfile(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto userId = req->attributes()->get<std::string>("userId");
    auto db = drogon::app().getDbClient();
    
    if (!db) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Internal Server Error";
            json["message"] = "Database connection client unavailable.";
            return json;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
        return;
    }

    try {
        auto result = db->execSqlSync(
            "SELECT id, user_id, business_name, webhook_url FROM merchant_accounts WHERE user_id = $1",
            userId
        );

        if (result.empty()) {
            // Return 200 with null to indicate not registered
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody("null");
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            callback(resp);
            return;
        }

        auto row = result[0];
        Json::Value profile;
        profile["id"] = row["id"].as<std::string>();
        profile["user_id"] = row["user_id"].as<std::string>();
        profile["business_name"] = row["business_name"].as<std::string>();
        profile["webhook_url"] = row["webhook_url"].isNull() ? "" : row["webhook_url"].as<std::string>();

        auto resp = drogon::HttpResponse::newHttpJsonResponse(profile);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch merchant profile: " << e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&e](){
            Json::Value json;
            json["error"] = "Internal Server Error";
            json["message"] = e.what();
            return json;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
}

void MerchantController::registerMerchant(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto userId = req->attributes()->get<std::string>("userId");
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Invalid or empty JSON payload.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto& reqJson = *jsonPtr;
    std::string businessName = reqJson.get("business_name", "").asString();
    if (businessName.empty()) {
        businessName = reqJson.get("businessName", "").asString();
    }
    std::string webhookUrl = reqJson.get("webhook_url", "").asString();
    if (webhookUrl.empty()) {
        webhookUrl = reqJson.get("webhookUrl", "").asString();
    }

    if (businessName.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Business name is required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::MerchantManager::registerMerchant(userId, businessName, webhookUrl);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Merchant registration failed: " << e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&e](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = e.what();
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

void MerchantController::createInvoice(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto userId = req->attributes()->get<std::string>("userId");
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Invalid or empty JSON payload.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto& reqJson = *jsonPtr;
    std::string customerAcc = reqJson.get("customer_account_number", "").asString();
    if (customerAcc.empty()) {
        customerAcc = reqJson.get("customerAcc", "").asString();
    }
    double amount = reqJson.get("amount", 0.0).asDouble();
    std::string description = reqJson.get("description", "").asString();

    if (customerAcc.empty() || amount <= 0.0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Customer account number and positive amount are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::MerchantManager::createInvoice(userId, customerAcc, amount, description);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Merchant invoice creation failed: " << e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&e](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = e.what();
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

void MerchantController::getInvoices(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto userId = req->attributes()->get<std::string>("userId");
    auto db = drogon::app().getDbClient();
    
    if (!db) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Internal Server Error";
            json["message"] = "Database connection client unavailable.";
            return json;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
        return;
    }

    try {
        auto result = db->execSqlSync(
            "SELECT id, merchant_id, customer_account_number, amount, description, status, digital_signature, webhook_sent, created_at "
            "FROM merchant_invoices WHERE merchant_id = (SELECT id FROM merchant_accounts WHERE user_id = $1) ORDER BY created_at DESC",
            userId
        );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value inv;
            inv["id"] = row["id"].as<std::string>();
            inv["merchant_id"] = row["merchant_id"].as<std::string>();
            inv["customer_account_number"] = row["customer_account_number"].isNull() ? "" : row["customer_account_number"].as<std::string>();
            inv["amount"] = row["amount"].as<double>();
            inv["description"] = row["description"].as<std::string>();
            inv["status"] = row["status"].as<std::string>();
            inv["digital_signature"] = row["digital_signature"].isNull() ? "" : row["digital_signature"].as<std::string>();
            inv["webhook_sent"] = row["webhook_sent"].as<bool>();
            inv["created_at"] = row["created_at"].as<std::string>();
            arr.append(inv);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch merchant invoices: " << e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&e](){
            Json::Value json;
            json["error"] = "Internal Server Error";
            json["message"] = e.what();
            return json;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
}

void MerchantController::payInvoice(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    std::string idempotencyKey = req->getHeader("Idempotency-Key");
    if (idempotencyKey.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Idempotency-Key header is required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Invalid or empty JSON payload.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto& reqJson = *jsonPtr;
    std::string invoiceId = reqJson.get("invoice_id", "").asString();
    if (invoiceId.empty()) {
        invoiceId = reqJson.get("invoiceId", "").asString();
    }
    std::string customerAccountNo = reqJson.get("customer_account_number", "").asString();
    if (customerAccountNo.empty()) {
        customerAccountNo = reqJson.get("customerAccountNo", "").asString();
    }

    if (invoiceId.empty() || customerAccountNo.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Invoice ID and customer account number are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::MerchantManager::payInvoice(invoiceId, customerAccountNo, idempotencyKey);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Merchant invoice payment failed: " << e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&e](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = e.what();
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
    }
}

} // namespace banking::controllers

int merchant_controller_force_link_val = 42;
