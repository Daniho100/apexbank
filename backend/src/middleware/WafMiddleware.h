#pragma once
#include <drogon/HttpFilter.h>
#include <string>

namespace banking::middleware {

class WafMiddleware : public drogon::HttpFilter<WafMiddleware> {
public:
    virtual void doFilter(
        const drogon::HttpRequestPtr& req,
        drogon::FilterCallback&& fcb,
        drogon::FilterChainCallback&& fccb
    ) override;

    static bool isRequestMalicious(const drogon::HttpRequestPtr& req, std::string& blockReason);

private:
    static bool isMalicious(const std::string& input);
};

} // namespace banking::middleware
