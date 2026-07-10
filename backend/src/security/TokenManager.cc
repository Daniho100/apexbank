#include "TokenManager.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <json/json.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <stdexcept>
#include <chrono>

namespace banking::security {

std::string TokenManager::base64UrlEncode(const std::string& input) {
    // 1. Standard Base64 Encode using OpenSSL EVP
    std::vector<unsigned char> buffer((input.size() + 2) / 3 * 4 + 1);
    int length = EVP_EncodeBlock(buffer.data(), reinterpret_cast<const unsigned char*>(input.data()), input.size());
    std::string base64(reinterpret_cast<char*>(buffer.data()), length);
    
    // 2. Base64 to Base64URL conversion
    std::string base64Url = "";
    for (char c : base64) {
        if (c == '+') base64Url += '-';
        else if (c == '/') base64Url += '_';
        else if (c == '=') continue; // strip padding
        else if (c == '\r' || c == '\n') continue; // strip newlines
        else base64Url += c;
    }
    return base64Url;
}

std::string TokenManager::base64UrlDecode(const std::string& input) {
    // 1. Base64URL to Base64 conversion
    std::string base64 = input;
    for (char& c : base64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    // 2. Restore padding
    while (base64.size() % 4 != 0) {
        base64 += '=';
    }
    
    // 3. Decode standard Base64 using OpenSSL EVP
    std::vector<unsigned char> buffer(base64.size() * 3 / 4 + 1);
    int length = EVP_DecodeBlock(buffer.data(), reinterpret_cast<const unsigned char*>(base64.data()), base64.size());
    if (length < 0) {
        throw std::runtime_error("Base64 decoding failed");
    }
    
    // Adjust length based on padding count in base64 string
    int padCount = 0;
    if (!base64.empty() && base64[base64.size() - 1] == '=') padCount++;
    if (base64.size() > 1 && base64[base64.size() - 2] == '=') padCount++;
    
    return std::string(reinterpret_cast<char*>(buffer.data()), length - padCount);
}

std::string TokenManager::hmacSha256(const std::string& data, const std::string& key) {
    unsigned int len = 0;
    unsigned char result[EVP_MAX_MD_SIZE];
    
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    // OpenSSL 3.x compliance
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         result, &len);
#else
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         result, &len);
#endif

    return std::string(reinterpret_cast<char*>(result), len);
}

std::string TokenManager::generateAccessToken(const TokenPayload& payload, const std::string& secret) {
    // 1. Create Header
    Json::Value headerJson;
    headerJson["alg"] = "HS256";
    headerJson["typ"] = "JWT";
    Json::FastWriter writer;
    std::string headerEncoded = base64UrlEncode(writer.write(headerJson));
    
    // 2. Create Payload
    Json::Value payloadJson;
    payloadJson["user_id"] = payload.userId;
    payloadJson["email"] = payload.email;
    payloadJson["role"] = payload.role;
    payloadJson["session_id"] = payload.sessionId;
    payloadJson["exp"] = (Json::Value::UInt64)payload.expiresAt;
    
    // Add current time as issued at (iat)
    auto now = std::chrono::system_clock::now();
    auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    payloadJson["iat"] = (Json::Value::UInt64)iat;
    
    std::string payloadEncoded = base64UrlEncode(writer.write(payloadJson));
    
    // 3. Create Signature
    std::string signingInput = headerEncoded + "." + payloadEncoded;
    std::string signatureRaw = hmacSha256(signingInput, secret);
    std::string signatureEncoded = base64UrlEncode(signatureRaw);
    
    return signingInput + "." + signatureEncoded;
}

std::optional<TokenPayload> TokenManager::verifyAccessToken(const std::string& token, const std::string& secret) {
    // Split token by dots
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }
    
    if (parts.size() != 3) {
        return std::nullopt; // Invalid JWT structure
    }
    
    // 1. Verify Signature
    std::string signingInput = parts[0] + "." + parts[1];
    std::string expectedSignatureRaw = hmacSha256(signingInput, secret);
    std::string expectedSignature = base64UrlEncode(expectedSignatureRaw);
    
    if (parts[2] != expectedSignature) {
        return std::nullopt; // Signature verification failed
    }
    
    // 2. Decode Payload
    try {
        std::string payloadJsonStr = base64UrlDecode(parts[1]);
        Json::Value payloadJson;
        Json::Reader reader;
        if (!reader.parse(payloadJsonStr, payloadJson)) {
            return std::nullopt;
        }
        
        // Check expiration
        auto now = std::chrono::system_clock::now();
        int64_t currentTime = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        int64_t exp = payloadJson["exp"].asInt64();
        
        if (currentTime > exp) {
            return std::nullopt; // Token expired
        }
        
        TokenPayload payload;
        payload.userId = payloadJson["user_id"].asString();
        payload.email = payloadJson["email"].asString();
        payload.role = payloadJson["role"].asString();
        payload.sessionId = payloadJson["session_id"].asString();
        payload.expiresAt = exp;
        
        return payload;
    } catch (...) {
        return std::nullopt; // Parsing or decoding failed
    }
}

std::string TokenManager::generateRefreshToken() {
    std::vector<unsigned char> buffer(32); // 256 bits of entropy
    if (RAND_bytes(buffer.data(), buffer.size()) != 1) {
        throw std::runtime_error("Failed to generate secure random refresh token using OpenSSL");
    }
    
    std::stringstream ss;
    for (unsigned char b : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return ss.str();
}

} // namespace banking::security
