#include "IdempotencyMiddleware.h"
#include "services/LedgerService.h"
#include <drogon/drogon.h>

namespace banking::middleware {

void IdempotencyMiddleware::doFilter(
    const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&& fcb,
    drogon::FilterChainCallback&& fccb
) {
    // 1. Only enforce idempotency for modifying requests (POST, PUT, DELETE)
    if (req->method() != drogon::HttpMethod::Post &&
        req->method() != drogon::HttpMethod::Put &&
        req->method() != drogon::HttpMethod::Delete) {
        fccb();
        return;
    }
    
    // 2. Extract Idempotency-Key header
    std::string idempotencyKey = req->getHeader("Idempotency-Key");
    if (idempotencyKey.empty()) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            []() {
                Json::Value json;
                json["error"] = "Bad Request";
                json["message"] = "Idempotency-Key header is required for this transaction.";
                return json;
            }()
        );
        resp->setStatusCode(drogon::k400BadRequest);
        fcb(resp);
        return;
    }
    
    // 3. Query ledger database for existing key
    auto cachedOpt = services::LedgerService::checkIdempotency(idempotencyKey);
    if (cachedOpt.has_value()) {
        auto cached = cachedOpt.value();
        
        // Extract original response cache payload if exists
        Json::Value payload;
        if (cached.isMember("response_cache")) {
            payload = cached["response_cache"];
        } else {
            // Reconstruct base transaction info if cache payload is empty
            payload["status"] = cached["status"];
            payload["reference_number"] = cached["reference_number"];
            payload["amount"] = cached["amount"];
            payload["type"] = cached["type"];
            payload["message"] = "Duplicate transaction request (idempotent response)";
        }
        
        auto resp = drogon::HttpResponse::newHttpJsonResponse(payload);
        // Add header to tell the client this was a cached response
        resp->addHeader("X-Cache-Idempotent", "true");
        resp->addHeader("Idempotency-Key", idempotencyKey);
        
        LOG_INFO << "Idempotent hit: returned cached response for key: " << idempotencyKey;
        fcb(resp);
        return;
    }
    
    // 4. Proceed to execute original transaction
    fccb();
}

} // namespace banking::middleware
