#include "RateLimitMiddleware.h"
#include "security/RateLimiter.h"
#include <drogon/drogon.h>

namespace banking::middleware {

void RateLimitMiddleware::doFilter(
    const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&& fcb,
    drogon::FilterChainCallback&& fccb
) {
    std::string key = "";
    
    // 1. Identify client: check if authenticated userId attribute exists
    auto userIdPtr = req->attributes()->get<std::string>("userId");
    if (userIdPtr != nullptr && !userIdPtr->empty()) {
        key = "rl_user:" + *userIdPtr;
    } else {
        // Fallback to peer IP address
        key = "rl_ip:" + req->peerAddr().toIp();
    }
    
    // 2. Enforce Rate Limit: 60 requests per 60 seconds
    int maxRequests = 60;
    int windowSeconds = 60;
    
    if (!security::RateLimiter::isAllowed(key, maxRequests, windowSeconds)) {
        auto resp = drogon::HttpResponse::newJsonHttpResponse(
            []() {
                Json::Value json;
                json["error"] = "Too Many Requests";
                json["message"] = "Rate limit exceeded. Please try again after 60 seconds.";
                return json;
            }()
        );
        resp->setStatusCode(drogon::k429TooManyRequests);
        resp->addHeader("Retry-After", std::to_string(windowSeconds));
        fcb(resp);
        return;
    }
    
    fccb();
}

} // namespace banking::middleware
