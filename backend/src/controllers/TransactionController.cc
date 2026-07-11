#include "TransactionController.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::controllers {

void TransactionController::getTransactions(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    std::string accountId = req->getParameter("account_id");
    if (accountId.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "account_id parameter is required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

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
        // Security check: Only admins can view other users' accounts
        if (role != "administrator") {
            auto checkRes = db->execSqlSync("SELECT id FROM accounts WHERE id = $1 AND user_id = $2", accountId, userId);
            if (checkRes.empty()) {
                // Check if accountId was passed as account_number instead of UUID
                checkRes = db->execSqlSync("SELECT id FROM accounts WHERE account_number = $1 AND user_id = $2", accountId, userId);
                if (checkRes.empty()) {
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(Json::Value(Json::arrayValue));
                    callback(resp);
                    return;
                }
                accountId = checkRes[0]["id"].as<std::string>();
            }
        } else {
            // Admin flow: resolve account number to ID if necessary
            auto checkRes = db->execSqlSync("SELECT id FROM accounts WHERE id = $1", accountId);
            if (checkRes.empty()) {
                checkRes = db->execSqlSync("SELECT id FROM accounts WHERE account_number = $1", accountId);
                if (!checkRes.empty()) {
                    accountId = checkRes[0]["id"].as<std::string>();
                }
            }
        }

        auto result = db->execSqlSync(
            "SELECT id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description, created_at "
            "FROM transactions WHERE sender_account_id = $1 OR receiver_account_id = $1 ORDER BY created_at DESC",
            accountId
        );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value t;
            t["id"] = row["id"].isNull() ? "" : row["id"].as<std::string>();
            t["type"] = row["type"].isNull() ? "" : row["type"].as<std::string>();
            t["amount"] = row["amount"].isNull() ? 0.0 : row["amount"].as<double>();
            t["status"] = row["status"].isNull() ? "" : row["status"].as<std::string>();
            t["idempotency_key"] = row["idempotency_key"].isNull() ? "" : row["idempotency_key"].as<std::string>();
            t["sender_account_id"] = row["sender_account_id"].isNull() ? "" : row["sender_account_id"].as<std::string>();
            t["receiver_account_id"] = row["receiver_account_id"].isNull() ? "" : row["receiver_account_id"].as<std::string>();
            t["reference_number"] = row["reference_number"].isNull() ? "" : row["reference_number"].as<std::string>();
            t["description"] = row["description"].isNull() ? "" : row["description"].as<std::string>();
            t["created_at"] = row["created_at"].isNull() ? "" : row["created_at"].as<std::string>();
            arr.append(t);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch transactions: " << e.what();
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

int transaction_controller_force_link_val = 42;
