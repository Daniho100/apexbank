#pragma once
#include <drogon/HttpFilter.h>

namespace banking::middleware {

class AuthMiddleware : public drogon::HttpFilter<AuthMiddleware> {
public:
    virtual void doFilter(
        const drogon::HttpRequestPtr& req,
        drogon::FilterCallback&& fcb,
        drogon::FilterChainCallback&& fccb
    ) override;
};

} // namespace banking::middleware
