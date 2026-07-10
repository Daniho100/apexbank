#include "RateLimiter.h"
#include <drogon/drogon.h>
#include <chrono>
#include <random>
#include <iostream>

namespace banking::security {

bool RateLimiter::isAllowed(const std::string& key, int maxRequests, int windowSeconds) {
    auto redisClient = drogon::app().getRedisClient();
    if (!redisClient) {
        LOG_ERROR << "Redis client is not configured! Rate limiting bypassed (fail-open).";
        return true;
    }
    
    // Obtain current time in seconds (as double to support millisecond precision in zset)
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() / 1000.0;
    
    // Define the Lua script for sliding window rate limiting
    // Using a Lua script ensures atomicity and saves network round-trips
    const char* luaScript = 
        "local key = KEYS[1]\n"
        "local now = tonumber(ARGV[1])\n"
        "local window = tonumber(ARGV[2])\n"
        "local limit = tonumber(ARGV[3])\n"
        "local member = ARGV[4]\n"
        "local clearBefore = now - window\n"
        "\n"
        "redis.call('ZREMRANGEBYSCORE', key, 0, clearBefore)\n"
        "local currentRequests = redis.call('ZCARD', key)\n"
        "\n"
        "if currentRequests < limit then\n"
        "    redis.call('ZADD', key, now, member)\n"
        "    redis.call('EXPIRE', key, window)\n"
        "    return 1\n"
        "else\n"
        "    return 0\n"
        "end\n";
        
    // Generate a unique member identifier to prevent duplicates in zset
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000000);
    std::string member = std::to_string(now) + "_" + std::to_string(dis(gen));
    
    try {
        // Execute Lua Script on Redis
        // EVAL script numkeys key1 [key2 ...] arg1 [arg2 ...]
        auto result = redisClient->execCommandSync(
            [](const drogon::nosql::RedisResult& r) {
                return r;
            },
            "EVAL %s 1 %s %f %d %d %s",
            luaScript,
            key.c_str(),
            now,
            windowSeconds,
            maxRequests,
            member.c_str()
        );
        
        if (result.type() == drogon::nosql::RedisResultType::kInteger) {
            return result.asInteger() == 1;
        }
        
        LOG_WARN << "Redis EVAL returned unexpected type for rate limit check. Bypassing.";
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis rate limiting error: " << e.what() << ". Bypassing (fail-open).";
        return true;
    }
}

} // namespace banking::security
