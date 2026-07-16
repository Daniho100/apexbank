#pragma once
#include <drogon/drogon.h>

namespace banking::security {

class PayloadEncryptionAdvice {
public:
    static void handlePreRouting(
        const drogon::HttpRequestPtr &req,
        drogon::AdviceCallback &&acb,
        drogon::AdviceChainCallback &&callback
    );

    static void handlePostHandling(
        const drogon::HttpRequestPtr &req,
        const drogon::HttpResponsePtr &resp
    );
};

} // namespace banking::security
