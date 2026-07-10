#include "LoanController.h"
#include "services/LoanEngine.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::controllers {

void LoanController::getLoans(
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
        auto result = (role == "administrator") ?
            db->execSqlSync(
                "SELECT id, user_id, amount, interest_rate, duration_months, monthly_repayment, outstanding_balance, status, approved_at, disbursed_at, due_date, created_at "
                "FROM loans ORDER BY created_at DESC"
            ) :
            db->execSqlSync(
                "SELECT id, user_id, amount, interest_rate, duration_months, monthly_repayment, outstanding_balance, status, approved_at, disbursed_at, due_date, created_at "
                "FROM loans WHERE user_id = $1 ORDER BY created_at DESC",
                userId
            );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value loan;
            loan["id"] = row["id"].as<std::string>();
            loan["user_id"] = row["user_id"].as<std::string>();
            loan["amount"] = row["amount"].as<double>();
            loan["interest_rate"] = row["interest_rate"].as<double>();
            loan["duration_months"] = row["duration_months"].as<int>();
            loan["monthly_repayment"] = row["monthly_repayment"].as<double>();
            loan["outstanding_balance"] = row["outstanding_balance"].as<double>();
            loan["status"] = row["status"].as<std::string>();
            loan["approved_at"] = row["approved_at"].isNull() ? "" : row["approved_at"].as<std::string>();
            loan["disbursed_at"] = row["disbursed_at"].isNull() ? "" : row["disbursed_at"].as<std::string>();
            loan["due_date"] = row["due_date"].isNull() ? "" : row["due_date"].as<std::string>();
            loan["created_at"] = row["created_at"].as<std::string>();
            arr.append(loan);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch loans: " << e.what();
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

void LoanController::apply(
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

    try {
        auto res = services::LoanEngine::applyForLoan(userId, amount, duration, 10.0);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Loan application failed: " << e.what();
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

void LoanController::repay(
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
    std::string loanId = reqJson.get("loan_id", "").asString();
    std::string accountNo = reqJson.get("account_number", "").asString();
    if (accountNo.empty()) {
        accountNo = reqJson.get("accountNo", "").asString();
    }
    double amount = reqJson.get("amount", 0.0).asDouble();

    if (loanId.empty() || accountNo.empty() || amount <= 0.0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Loan ID, Account number, and positive payment amount are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::LoanEngine::processRepayment(loanId, accountNo, amount, idempotencyKey);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Loan repayment failed: " << e.what();
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

void LoanController::getSchedules(
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
        auto result = (role == "administrator") ?
            db->execSqlSync(
                "SELECT id, loan_id, installment_number, amount_due, principal_due, interest_due, amount_paid, due_date, status, paid_at "
                "FROM loan_schedules ORDER BY installment_number ASC"
            ) :
            db->execSqlSync(
                "SELECT s.id, s.loan_id, s.installment_number, s.amount_due, s.principal_due, s.interest_due, s.amount_paid, s.due_date, s.status, s.paid_at "
                "FROM loan_schedules s JOIN loans l ON s.loan_id = l.id WHERE l.user_id = $1 ORDER BY s.installment_number ASC",
                userId
            );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value s;
            s["id"] = row["id"].as<std::string>();
            s["loan_id"] = row["loan_id"].as<std::string>();
            s["installment_number"] = row["installment_number"].as<int>();
            s["amount_due"] = row["amount_due"].as<double>();
            s["principal_due"] = row["principal_due"].as<double>();
            s["interest_due"] = row["interest_due"].as<double>();
            s["amount_paid"] = row["amount_paid"].as<double>();
            s["due_date"] = row["due_date"].as<std::string>();
            s["status"] = row["status"].as<std::string>();
            s["paid_at"] = row["paid_at"].isNull() ? "" : row["paid_at"].as<std::string>();
            arr.append(s);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch loan schedules: " << e.what();
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

int loan_controller_force_link_val = 42;
