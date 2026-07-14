#include "AdminController.h"
#include "services/LoanEngine.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::controllers {

// Helper to check admin role
bool checkAdmin(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>& callback) {
    auto role = req->attributes()->get<std::string>("role");
    if (role != "administrator") {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Forbidden";
            json["message"] = "Administrative privileges required.";
            return json;
        }());
        resp->setStatusCode(drogon::k403Forbidden);
        callback(resp);
        return false;
    }
    return true;
}

void AdminController::approveLoan(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!checkAdmin(req, callback)) return;
    auto adminId = req->attributes()->get<std::string>("userId");

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
    if (loanId.empty()) {
        loanId = reqJson.get("loanId", "").asString();
    }

    if (loanId.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Loan ID is required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto res = services::LoanEngine::approveAndDisburse(loanId, adminId);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Loan approval failed: " << e.what();
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

void AdminController::getUsers(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!checkAdmin(req, callback)) return;
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
            "SELECT id, email, role, status, loan_limit, created_at FROM users "
            "WHERE email != 'system_treasury@banking.com' ORDER BY created_at DESC"
        );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value u;
            u["id"] = row["id"].as<std::string>();
            u["email"] = row["email"].as<std::string>();
            u["role"] = row["role"].as<std::string>();
            u["status"] = row["status"].as<std::string>();
            u["loan_limit"] = row["loan_limit"].as<double>();
            u["created_at"] = row["created_at"].as<std::string>();
            arr.append(u);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch users: " << e.what();
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

void AdminController::setUserStatus(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!checkAdmin(req, callback)) return;
    auto adminId = req->attributes()->get<std::string>("userId");
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
    std::string userId = reqJson.get("user_id", "").asString();
    if (userId.empty()) {
        userId = reqJson.get("userId", "").asString();
    }
    std::string status = reqJson.get("status", "").asString();

    if (userId.empty() || status.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "User ID and target status are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    try {
        auto checkRes = db->execSqlSync("SELECT email FROM users WHERE id = $1", userId);
        if (checkRes.empty()) {
            throw std::runtime_error("User profile not found.");
        }
        std::string userEmail = checkRes[0]["email"].as<std::string>();

        auto trans = db->newTransaction();
        trans->execSqlSync("UPDATE users SET status = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", status, userId);

        std::string desc = "Changed status of user " + userEmail + " to " + status;
        trans->execSqlSync(
            "INSERT INTO audit_logs (user_id, action, description) VALUES ($1, 'user_status_change', $2)",
            adminId, desc
        );

        std::string notifDesc = "Your account status has been updated to " + status + " by the bank administration.";
        trans->execSqlSync(
            "INSERT INTO notifications (user_id, title, content, type) VALUES ($1, 'Account Status Updated', $2, 'in_app')",
            userId, notifDesc
        );

        trans->execSqlSync("COMMIT");

        Json::Value res;
        res["status"] = "success";
        res["message"] = "User status updated to " + status;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "User status toggle failed: " << e.what();
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

void AdminController::getAuditLogs(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!checkAdmin(req, callback)) return;
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
            "SELECT a.id, a.user_id, u.email, a.action, a.description, a.ip_address, a.created_at "
            "FROM audit_logs a LEFT JOIN users u ON a.user_id = u.id "
            "ORDER BY a.created_at DESC LIMIT 500"
        );

        Json::Value arr(Json::arrayValue);
        for (auto const& row : result) {
            Json::Value log;
            log["id"] = row["id"].as<std::string>();
            log["user_id"] = row["user_id"].isNull() ? "" : row["user_id"].as<std::string>();
            log["user_email"] = row["email"].isNull() ? "" : row["email"].as<std::string>();
            log["action"] = row["action"].as<std::string>();
            log["description"] = row["description"].as<std::string>();
            log["ip_address"] = row["ip_address"].isNull() ? "" : row["ip_address"].as<std::string>();
            log["created_at"] = row["created_at"].as<std::string>();
            arr.append(log);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch system audit logs: " << e.what();
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

void AdminController::getUserActivity(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& userId
) {
    if (!checkAdmin(req, callback)) return;
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
        // 1. Fetch user accounts
        auto accsResult = db->execSqlSync(
            "SELECT id, account_number, type, balance, status, currency, created_at "
            "FROM accounts WHERE user_id = $1 ORDER BY created_at ASC",
            userId
        );
        Json::Value accountsJson(Json::arrayValue);
        for (auto const& row : accsResult) {
            Json::Value acc;
            acc["id"] = row["id"].as<std::string>();
            acc["account_number"] = row["account_number"].as<std::string>();
            acc["type"] = row["type"].as<std::string>();
            acc["balance"] = row["balance"].as<double>();
            acc["status"] = row["status"].as<std::string>();
            acc["currency"] = row["currency"].as<std::string>();
            acc["created_at"] = row["created_at"].as<std::string>();
            accountsJson.append(acc);
        }

        // 2. Fetch user transactions
        auto txnsResult = db->execSqlSync(
            "SELECT id, type, amount, status, reference_number, description, sender_account_id, receiver_account_id, created_at "
            "FROM transactions WHERE sender_account_id IN (SELECT id FROM accounts WHERE user_id = $1) "
            "OR receiver_account_id IN (SELECT id FROM accounts WHERE user_id = $1) "
            "ORDER BY created_at DESC",
            userId
        );
        Json::Value txnsJson(Json::arrayValue);
        for (auto const& row : txnsResult) {
            Json::Value txn;
            txn["id"] = row["id"].isNull() ? "" : row["id"].as<std::string>();
            txn["type"] = row["type"].isNull() ? "" : row["type"].as<std::string>();
            txn["amount"] = row["amount"].isNull() ? 0.0 : row["amount"].as<double>();
            txn["status"] = row["status"].isNull() ? "" : row["status"].as<std::string>();
            txn["reference_number"] = row["reference_number"].isNull() ? "" : row["reference_number"].as<std::string>();
            txn["description"] = row["description"].isNull() ? "" : row["description"].as<std::string>();
            txn["sender_account_id"] = row["sender_account_id"].isNull() ? "" : row["sender_account_id"].as<std::string>();
            txn["receiver_account_id"] = row["receiver_account_id"].isNull() ? "" : row["receiver_account_id"].as<std::string>();
            txn["created_at"] = row["created_at"].isNull() ? "" : row["created_at"].as<std::string>();
            txnsJson.append(txn);
        }

        // 3. Fetch user audit logs
        auto logsResult = db->execSqlSync(
            "SELECT id, action, description, ip_address, created_at "
            "FROM audit_logs WHERE user_id = $1 ORDER BY created_at DESC",
            userId
        );
        Json::Value logsJson(Json::arrayValue);
        for (auto const& row : logsResult) {
            Json::Value log;
            log["id"] = row["id"].as<std::string>();
            log["action"] = row["action"].as<std::string>();
            log["description"] = row["description"].as<std::string>();
            log["ip_address"] = row["ip_address"].isNull() ? "" : row["ip_address"].as<std::string>();
            log["created_at"] = row["created_at"].as<std::string>();
            logsJson.append(log);
        }

        Json::Value root;
        root["accounts"] = accountsJson;
        root["transactions"] = txnsJson;
        root["audit_logs"] = logsJson;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch user activity: " << e.what();
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

void AdminController::deleteUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!checkAdmin(req, callback)) return;
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
    std::string userId = reqJson.get("user_id", "").asString();
    if (userId.empty()) {
        userId = reqJson.get("userId", "").asString();
    }

    if (userId.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "User ID is required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

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

    auto trans = db->newTransaction();
    try {
        auto checkRes = trans->execSqlSync("SELECT email, role FROM users WHERE id = $1", userId);
        if (checkRes.empty()) {
            throw std::runtime_error("User not found.");
        }
        std::string role = checkRes[0]["role"].as<std::string>();
        std::string email = checkRes[0]["email"].as<std::string>();
        if (role == "administrator" || email == "system_treasury@banking.com") {
            throw std::runtime_error("Cannot delete administrative or system treasury profiles.");
        }

        trans->execSqlSync(
            "DELETE FROM ledger_entries WHERE transaction_id IN ("
            "  SELECT id FROM transactions WHERE sender_account_id IN (SELECT id FROM accounts WHERE user_id = $1) "
            "  OR receiver_account_id IN (SELECT id FROM accounts WHERE user_id = $1)"
            ")",
            userId
        );
        trans->execSqlSync(
            "DELETE FROM ledger_entries WHERE account_id IN (SELECT id FROM accounts WHERE user_id = $1)",
            userId
        );
        trans->execSqlSync(
            "DELETE FROM transactions WHERE sender_account_id IN (SELECT id FROM accounts WHERE user_id = $1) "
            "OR receiver_account_id IN (SELECT id FROM accounts WHERE user_id = $1)",
            userId
        );
        trans->execSqlSync(
            "DELETE FROM loan_schedules WHERE loan_id IN (SELECT id FROM loans WHERE user_id = $1)",
            userId
        );
        trans->execSqlSync(
            "DELETE FROM loans WHERE user_id = $1",
            userId
        );
        trans->execSqlSync(
            "DELETE FROM fixed_deposits WHERE user_id = $1",
            userId
        );
        trans->execSqlSync(
            "DELETE FROM accounts WHERE user_id = $1",
            userId
        );
        trans->execSqlSync(
            "DELETE FROM users WHERE id = $1",
            userId
        );

        trans->execSqlSync("COMMIT");

        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["status"] = "success";
            json["message"] = "User profile and all associated wallets, loans, and ledgers deleted successfully.";
            return json;
        }());
        callback(resp);
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Failed to delete user: " << e.what();
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

void AdminController::updateLoanLimit(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!checkAdmin(req, callback)) return;
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
    std::string userId = reqJson.get("user_id", "").asString();
    if (userId.empty()) {
        userId = reqJson.get("userId", "").asString();
    }
    double loanLimit = reqJson.get("loan_limit", 0.0).asDouble();
    if (loanLimit == 0.0) {
        loanLimit = reqJson.get("loanLimit", 0.0).asDouble();
    }

    if (userId.empty() || loanLimit <= 0.0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "User ID and a positive loan limit are required.";
            return json;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

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
        db->execSqlSync(
            "UPDATE users SET loan_limit = $1 WHERE id = $2",
            loanLimit,
            userId
        );

        auto resp = drogon::HttpResponse::newHttpJsonResponse([loanLimit](){
            Json::Value json;
            json["status"] = "success";
            json["loan_limit"] = loanLimit;
            json["message"] = "User credit/loan limit updated successfully.";
            return json;
        }());
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to update loan limit: " << e.what();
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

void AdminController::getSystemStats(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!checkAdmin(req, callback)) return;
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
        auto statsRes = db->execSqlSync(
            "SELECT "
            "  (SELECT COUNT(*) FROM users WHERE email != 'system_treasury@banking.com') as total_users, "
            "  (SELECT COALESCE(SUM(balance), 0.0) FROM accounts WHERE type = 'savings') as total_savings, "
            "  (SELECT COALESCE(SUM(outstanding_balance), 0.0) FROM loans WHERE status IN ('approved', 'disbursed', 'active')) as total_loans, "
            "  (SELECT COALESCE(SUM(balance), 0.0) FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001') as treasury_balance"
        );

        Json::Value stats;
        if (!statsRes.empty()) {
            auto row = statsRes[0];
            stats["total_users"] = row["total_users"].as<int>();
            stats["total_savings"] = row["total_savings"].as<double>();
            stats["total_loans"] = row["total_loans"].as<double>();
            stats["treasury_balance"] = row["treasury_balance"].as<double>();
        } else {
            stats["total_users"] = 0;
            stats["total_savings"] = 0.0;
            stats["total_loans"] = 0.0;
            stats["treasury_balance"] = 0.0;
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(stats);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to fetch system stats: " << e.what();
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

int admin_controller_force_link_val = 42;
