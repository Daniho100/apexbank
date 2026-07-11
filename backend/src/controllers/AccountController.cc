#include "AccountController.h"
#include "services/LedgerService.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <mutex>
#include <random>
#include <sstream>

namespace banking::controllers {

static std::once_flag initFlag;

void AccountController::runMigrations() {
    auto db = drogon::app().getDbClient();
    if (db) {
        try {
            db->execSqlSync("ALTER TABLE loans ADD COLUMN IF NOT EXISTS name VARCHAR(255) NOT NULL DEFAULT 'Personal Loan'");
            db->execSqlSync("ALTER TABLE loans ADD COLUMN IF NOT EXISTS reference_number VARCHAR(100) UNIQUE");
            
            // Backfill reference_number for existing loans if any
            auto res = db->execSqlSync("SELECT id FROM loans WHERE reference_number IS NULL");
            for (auto const& row : res) {
                std::string id = row["id"].as<std::string>();
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(100000, 999999);
                std::string ref = "LN-" + std::to_string(dis(gen));
                db->execSqlSync("UPDATE loans SET reference_number = $1 WHERE id = $2", ref, id);
            }
            
            db->execSqlSync("ALTER TABLE loans ALTER COLUMN reference_number SET NOT NULL");
            db->execSqlSync("CREATE UNIQUE INDEX IF NOT EXISTS idx_loans_reference ON loans(reference_number)");
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to run DB migrations: " << e.what();
        }
    }
}

void AccountController::getAccounts(
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
            "SELECT id, user_id, account_number, type, balance, status, currency, created_at "
            "FROM accounts WHERE user_id = $1 ORDER BY created_at ASC",
            userId
        );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value acc;
            acc["id"] = row["id"].as<std::string>();
            acc["user_id"] = row["user_id"].as<std::string>();
            acc["account_number"] = row["account_number"].as<std::string>();
            acc["type"] = row["type"].as<std::string>();
            acc["balance"] = row["balance"].as<double>();
            acc["status"] = row["status"].as<std::string>();
            acc["currency"] = row["currency"].as<std::string>();
            acc["created_at"] = row["created_at"].as<std::string>();
            arr.append(acc);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch accounts: " << e.what();
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

void AccountController::deposit(
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
    double amount = reqJson.get("amount", 0.0).asDouble();
    std::string description = reqJson.get("description", "Cash Deposit").asString();

    if (accountNo.empty() || amount <= 0.0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Account number and a positive amount are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::LedgerService::processDeposit(accountNo, amount, idempotencyKey, description);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Deposit processing failed: " << e.what();
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

void AccountController::withdraw(
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
    double amount = reqJson.get("amount", 0.0).asDouble();
    std::string description = reqJson.get("description", "ATM Withdrawal").asString();

    if (accountNo.empty() || amount <= 0.0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Account number and a positive amount are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::LedgerService::processWithdrawal(accountNo, amount, idempotencyKey, description);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Withdrawal processing failed: " << e.what();
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

void AccountController::transfer(
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
    std::string senderAccountNo = reqJson.get("sender_account_number", "").asString();
    if (senderAccountNo.empty()) {
        senderAccountNo = reqJson.get("senderNo", "").asString();
    }
    std::string receiverAccountNo = reqJson.get("receiver_account_number", "").asString();
    if (receiverAccountNo.empty()) {
        receiverAccountNo = reqJson.get("receiverNo", "").asString();
    }
    if (receiverAccountNo.empty()) {
        receiverAccountNo = reqJson.get("transferTarget", "").asString();
    }
    double amount = reqJson.get("amount", 0.0).asDouble();
    std::string description = reqJson.get("description", "Internal Transfer").asString();

    if (senderAccountNo.empty() || receiverAccountNo.empty() || amount <= 0.0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Sender, receiver accounts and positive amount are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::LedgerService::processTransfer(senderAccountNo, receiverAccountNo, amount, idempotencyKey, description);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Transfer processing failed: " << e.what();
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

void AccountController::getNotifications(
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
            "SELECT id, user_id, title, content, type, is_read, created_at "
            "FROM notifications WHERE user_id = $1 ORDER BY created_at DESC",
            userId
        );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value note;
            note["id"] = row["id"].as<std::string>();
            note["user_id"] = row["user_id"].as<std::string>();
            note["title"] = row["title"].as<std::string>();
            note["content"] = row["content"].as<std::string>();
            note["type"] = row["type"].as<std::string>();
            note["is_read"] = row["is_read"].as<bool>();
            note["created_at"] = row["created_at"].as<std::string>();
            arr.append(note);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch notifications: " << e.what();
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

} // namespace banking::controllers

int account_controller_force_link_val = 42;
