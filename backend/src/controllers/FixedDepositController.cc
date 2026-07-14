#include "FixedDepositController.h"
#include "services/FixedDepositManager.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::controllers {

void FixedDepositController::getDeposits(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto userId = req->attributes()->get<std::string>("userId");
    auto role = req->attributes()->get<std::string>("role");
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
        services::FixedDepositManager::checkMaturities();
        
        auto result = (role == "administrator") ?
            db->execSqlSync(
                "SELECT id, user_id, amount, duration_months, interest_rate, status, start_date, maturity_date, certificate_number "
                "FROM fixed_deposits ORDER BY start_date DESC"
            ) :
            db->execSqlSync(
                "SELECT id, user_id, amount, duration_months, interest_rate, status, start_date, maturity_date, certificate_number "
                "FROM fixed_deposits WHERE user_id = $1 ORDER BY start_date DESC",
                userId
            );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value fd;
            fd["id"] = row["id"].as<std::string>();
            fd["user_id"] = row["user_id"].as<std::string>();
            fd["amount"] = row["amount"].as<double>();
            fd["duration_months"] = row["duration_months"].as<int>();
            fd["interest_rate"] = row["interest_rate"].as<double>();
            fd["status"] = row["status"].as<std::string>();
            fd["start_date"] = row["start_date"].as<std::string>();
            fd["maturity_date"] = row["maturity_date"].as<std::string>();
            fd["certificate_number"] = row["certificate_number"].as<std::string>();
            arr.append(fd);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch fixed deposits: " << e.what();
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

void FixedDepositController::create(
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
    double amount = reqJson.get("amount", 0.0).asDouble();
    int duration = reqJson.get("duration", 0).asInt();
    if (duration == 0) {
        duration = reqJson.get("duration_months", 0).asInt();
    }

    if (amount <= 0.0 || duration <= 0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Positive amount and term duration are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    // Match frontend rate logic: 3M = 5%, 6M = 7%, 12M+ = 10%
    double interestRate = 5.0;
    if (duration == 6) interestRate = 7.0;
    else if (duration >= 12) interestRate = 10.0;

    try {
        auto res = services::FixedDepositManager::createFixedDeposit(userId, amount, duration, interestRate);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Fixed deposit creation failed: " << e.what();
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

void FixedDepositController::liquidate(
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
    std::string fdId = reqJson.get("fixed_deposit_id", "").asString();
    if (fdId.empty()) {
        fdId = reqJson.get("fdId", "").asString();
    }

    if (fdId.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Fixed deposit ID is required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::FixedDepositManager::earlyWithdraw(fdId, userId);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Fixed deposit liquidation failed: " << e.what();
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

int fixed_deposit_controller_force_link_val = 42;
