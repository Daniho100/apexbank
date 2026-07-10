#include "AuthController.h"
#include "security/Argon2Hasher.h"
#include "security/TokenManager.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace banking::controllers {

void AuthController::registerUser(
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
    std::string email = reqJson.get("email", "").asString();
    std::string password = reqJson.get("password", "").asString();
    std::string role = reqJson.get("role", "customer").asString();

    if (email.empty() || password.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Email and password are required fields.";
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
        // 1. Check if email already exists
        auto checkRes = db->execSqlSync("SELECT id FROM users WHERE email = $1", email);
        if (!checkRes.empty()) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
                Json::Value json;
                json["error"] = "Conflict";
                json["message"] = "This email is already registered.";
                return json;
            }());
            resp->setStatusCode(drogon::k400BadRequest);
            callback(resp);
            return;
        }

        // 2. Hash Password using Argon2id
        std::string salt = security::Argon2Hasher::generateSalt(16);
        std::string pepper = drogon::app().getCustomConfig().get("app_pepper", "banking_app_pepper_secure_salt_value_12345").asString();
        std::string hash = security::Argon2Hasher::hashPassword(password, salt, pepper);

        // 3. Insert user & savings account in transaction
        auto trans = db->newTransaction();
        auto userRes = trans->execSqlSync(
            "INSERT INTO users (email, password_hash, salt, role) VALUES ($1, $2, $3, $4) RETURNING id",
            email, hash, salt, role
        );
        
        std::string userId = userRes[0]["id"].as<std::string>();

        // Generate account number (starts with '1' and has 9 random digits)
        std::string accountNo = "1";
        for (int i = 0; i < 9; ++i) {
            accountNo += std::to_string(std::rand() % 10);
        }

        trans->execSqlSync(
            "INSERT INTO accounts (user_id, account_number, type, balance, status, currency) VALUES ($1, $2, 'savings', 0.0, 'active', 'NGN')",
            userId, accountNo
        );

        trans->execSqlSync("COMMIT");

        auto resp = drogon::HttpResponse::newHttpJsonResponse([&](){
            Json::Value json;
            json["message"] = "User registered successfully.";
            json["account_number"] = accountNo;
            return json;
        }());
        resp->setStatusCode(drogon::k201Created);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Registration exception: " << e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&](){
            Json::Value json;
            json["error"] = "Internal Server Error";
            json["message"] = e.what();
            return json;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
}

void AuthController::loginUser(
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
    std::string email = reqJson.get("email", "").asString();
    std::string password = reqJson.get("password", "").asString();

    if (email.empty() || password.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
            Json::Value json;
            json["error"] = "Bad Request";
            json["message"] = "Email and password are required fields.";
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
        // 1. Fetch user by email
        auto userRes = db->execSqlSync(
            "SELECT id, email, password_hash, salt, role, status FROM users WHERE email = $1", 
            email
        );
        if (userRes.empty()) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
                Json::Value json;
                json["error"] = "Unauthorized";
                json["message"] = "Invalid email address or passcode.";
                return json;
            }());
            resp->setStatusCode(drogon::k401Unauthorized);
            callback(resp);
            return;
        }

        auto user = userRes[0];
        std::string userId = user["id"].as<std::string>();
        std::string role = user["role"].as<std::string>();
        std::string status = user["status"].as<std::string>();
        std::string hash = user["password_hash"].as<std::string>();

        if (status != "active") {
            auto resp = drogon::HttpResponse::newHttpJsonResponse([&](){
                Json::Value json;
                json["error"] = "Forbidden";
                json["message"] = "Your user registry profile is frozen or locked. Contact administrator.";
                return json;
            }());
            resp->setStatusCode(drogon::k403Forbidden);
            callback(resp);
            return;
        }

        // 2. Verify Password using Argon2id
        std::string pepper = drogon::app().getCustomConfig().get("app_pepper", "banking_app_pepper_secure_salt_value_12345").asString();
        bool isVerified = security::Argon2Hasher::verifyPassword(hash, password, pepper);
        
        if (!isVerified) {
            auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
                Json::Value json;
                json["error"] = "Unauthorized";
                json["message"] = "Invalid email address or passcode.";
                return json;
            }());
            resp->setStatusCode(drogon::k401Unauthorized);
            callback(resp);
            return;
        }

        // 3. Generate Access Token JWT & Session
        std::string sessionId = drogon::utils::getUuid();
        security::TokenPayload payload;
        payload.userId = userId;
        payload.email = email;
        payload.role = role;
        payload.sessionId = sessionId;
        
        auto now = std::chrono::system_clock::now();
        payload.expiresAt = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() + 3600 * 24; // 1 day expiry
        
        std::string secret = drogon::app().getCustomConfig().get("jwt_access_secret", "banking_super_secret_access_key_2026_jwt_token_98765").asString();
        std::string token = security::TokenManager::generateAccessToken(payload, secret);

        // 4. Save session inside Postgres
        std::string refreshToken = security::TokenManager::generateRefreshToken();
        auto expiryTime = now + std::chrono::hours(24);
        auto time_t_expiry = std::chrono::system_clock::to_time_t(expiryTime);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &time_t_expiry);
#else
        localtime_r(&time_t_expiry, &buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S");
        std::string expiryStr = ss.str();

        db->execSqlSync(
            "INSERT INTO sessions (user_id, refresh_token, token_expires_at) VALUES ($1, $2, $3::timestamp)",
            userId, refreshToken, expiryStr
        );

        // 5. Return payload
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&](){
            Json::Value json;
            json["token"] = token;
            json["user"]["id"] = userId;
            json["user"]["email"] = email;
            json["user"]["role"] = role;
            json["user"]["status"] = status;
            return json;
        }());
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    } catch (const std::exception& e) {
        LOG_ERROR << "Login exception: " << e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse([&](){
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

int auth_controller_force_link_val = 42;
