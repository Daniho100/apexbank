#include "AuthMiddleware.h"
#include "security/TokenManager.h"
#include <drogon/drogon.h>

namespace banking::middleware {

void AuthMiddleware::doFilter(
    const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&& fcb,
    drogon::FilterChainCallback&& fccb
) {
    // 1. Get Authorization Header
    std::string authHeader = req->getHeader("Authorization");
    if (authHeader.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            []() {
                Json::Value json;
                json["error"] = "Unauthorized";
                json["message"] = "Authorization header is missing.";
                return json;
            }()
        );
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    // Check Bearer prefix
    if (authHeader.size() < 7 || authHeader.substr(0, 7) != "Bearer ") {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            []() {
                Json::Value json;
                json["error"] = "Unauthorized";
                json["message"] = "Invalid authorization format. Use 'Bearer <token>'.";
                return json;
            }()
        );
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    std::string token = authHeader.substr(7);

    // 2. Load JWT Secret from custom config
    auto customConfig = drogon::app().getCustomConfig();
    std::string secret = customConfig.get("jwt_access_secret", "banking_super_secret_access_key_2026_jwt_token_98765").asString();

    // 3. Verify Token
    auto payloadOpt = security::TokenManager::verifyAccessToken(token, secret);
    if (!payloadOpt.has_value()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            []() {
                Json::Value json;
                json["error"] = "Unauthorized";
                json["message"] = "Session expired or invalid token.";
                return json;
            }()
        );
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    // 4. Attach session attributes to the request context
    auto payload = payloadOpt.value();
    req->attributes()->insert("userId", payload.userId);
    req->attributes()->insert("email", payload.email);
    req->attributes()->insert("role", payload.role);
    req->attributes()->insert("sessionId", payload.sessionId);

    // Continue filter chain
    fccb();
}

} // namespace banking::middleware
