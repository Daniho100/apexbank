#pragma once
#include <drogon/HttpFilter.h>

namespace banking::middleware {

class RateLimitMiddleware : public drogon::HttpFilter<RateLimitMiddleware> {
public:
    virtual void doFilter(
        const drogon::HttpRequestPtr& req,
        drogon::FilterCallback&& fcb,
        drogon::FilterChainCallback&& fccb
    ) override;
};

} // namespace banking::middleware
