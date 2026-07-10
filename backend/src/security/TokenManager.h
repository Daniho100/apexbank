#pragma once
#include <string>
#include <drogon/drogon.h>

namespace banking::security {

struct TokenPayload {
    std::string userId;
    std::string email;
    std::string role;
    std::string sessionId;
    int64_t expiresAt;
};

class TokenManager {
public:
    // Generate JWT access token (HS256)
    static std::string generateAccessToken(const TokenPayload& payload, const std::string& secret);

    // Verify and decode JWT access token
    static std::optional<TokenPayload> verifyAccessToken(const std::string& token, const std::string& secret);

    // Generate random secure refresh token (64 hex characters)
    static std::string generateRefreshToken();

private:
    // Base64URL encoding helper
    static std::string base64UrlEncode(const std::string& input);

    // Base64URL decoding helper
    static std::string base64UrlDecode(const std::string& input);

    // HMAC-SHA256 signature helper
    static std::string hmacSha256(const std::string& data, const std::string& key);
};

} // namespace banking::security
