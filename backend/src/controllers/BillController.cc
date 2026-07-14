#include "BillController.h"
#include "services/NigerianUtilityProviders.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <algorithm>

namespace banking::controllers {

void BillController::pay(
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
    std::string accountNo = reqJson.get("account_number", "").asString();
    if (accountNo.empty()) {
        accountNo = reqJson.get("accountNo", "").asString();
    }
    std::string provider = reqJson.get("provider", "").asString();
    std::string type = reqJson.get("type", "").asString();
    
    std::string typeLower = type;
    std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);

    std::string recipient = reqJson.get("recipient", "").asString();
    if (recipient.empty()) {
        if (typeLower == "electricity") {
            recipient = reqJson.get("meter_number", "").asString();
        } else if (typeLower == "airtime" || typeLower == "data") {
            recipient = reqJson.get("phone_number", "").asString();
        }
    }
    if (recipient.empty()) {
        recipient = reqJson.get("meterNo", "").asString();
    }
    if (recipient.empty()) {
        recipient = reqJson.get("phone_number", "").asString();
    }
    if (recipient.empty()) {
        recipient = reqJson.get("meter_number", "").asString();
    }

    double amount = reqJson.get("amount", 0.0).asDouble();

    if (accountNo.empty() || amount <= 0.0 || type.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Account number, positive amount, and utility type are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        Json::Value res;

        if (typeLower == "airtime") {
            res = services::NigerianUtilityProviders::buyAirtime(accountNo, provider, recipient, amount, idempotencyKey);
        } else if (typeLower == "data") {
            std::string bundleName = reqJson.get("bundleName", "Default Bundle").asString();
            if (bundleName == "Default Bundle") {
                bundleName = reqJson.get("bundle_name", "Default Bundle").asString();
            }
            res = services::NigerianUtilityProviders::buyData(accountNo, provider, recipient, bundleName, amount, idempotencyKey);
        } else if (typeLower == "electricity") {
            res = services::NigerianUtilityProviders::purchaseElectricity(accountNo, recipient, amount, idempotencyKey);
        } else if (typeLower == "cabletv" || typeLower == "cable" || typeLower == "tv") {
            std::string packageName = reqJson.get("packageName", "Default Package").asString();
            if (packageName == "Default Package") {
                packageName = reqJson.get("package_name", "Default Package").asString();
            }
            res = services::NigerianUtilityProviders::payCableTV(accountNo, provider, recipient, packageName, amount, idempotencyKey);
        } else if (typeLower == "betting" || typeLower == "bet") {
            res = services::NigerianUtilityProviders::placeBet(accountNo, provider, recipient, amount, idempotencyKey);
        } else {
            throw std::invalid_argument("Unsupported utility payment type: " + type);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Utility payment processing failed: " << e.what();
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

void BillController::validate(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
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
    std::string token = reqJson.get("token", "").asString();
    if (token.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Token parameter is required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::NigerianUtilityProviders::validatePrepaidToken(token);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Token validation failed: " << e.what();
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

int bill_controller_force_link_val = 42;
