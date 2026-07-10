#pragma once
#include <string>

namespace banking::security {

class RateLimiter {
public:
    // Check if the request is allowed under the sliding-window rate limit
    static bool isAllowed(const std::string& key, int maxRequests, int windowSeconds);
};

} // namespace banking::security
