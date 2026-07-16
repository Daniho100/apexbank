#include "PayloadEncryptionAdvice.h"
#include "EncryptionUtil.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>

namespace banking::security {

void PayloadEncryptionAdvice::handlePreRouting(
    const drogon::HttpRequestPtr &req,
    drogon::AdviceCallback &&acb,
    drogon::AdviceChainCallback &&callback
) {
    std::string encryptedHeader = req->getHeader("X-Encrypted-Payload");
    if (encryptedHeader == "true" && !req->body().empty()) {
        std::string aesKey = drogon::app().getCustomConfig().get("aes_encryption_key", "banking_aes_256_gcm_secret_key_32_bytes").asString();
        try {
            std::string decrypted = EncryptionUtil::decrypt(req->body(), aesKey);
            
            // Set the decrypted body back to the request
            req->setBody(std::move(decrypted));
            
            // Tag request so post-handling advice knows it needs to encrypt the response
            req->attributes()->insert("is_payload_encrypted", true);
        } catch (const std::exception& e) {
            LOG_ERROR << "Request payload decryption failed: " << e.what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse([](){
                Json::Value json;
                json["error"] = "Bad Request";
                json["message"] = "Invalid or corrupted encrypted payload.";
                return json;
            }());
            resp->setStatusCode(drogon::k400BadRequest);
            acb(resp);
            return;
        }
    }
    
    callback();
}

void PayloadEncryptionAdvice::handlePostHandling(
    const drogon::HttpRequestPtr &req,
    const drogon::HttpResponsePtr &resp
) {
    if (req->attributes()->find("is_payload_encrypted")) {
        std::string body = std::string(resp->body());
        if (!body.empty()) {
            std::string aesKey = drogon::app().getCustomConfig().get("aes_encryption_key", "banking_aes_256_gcm_secret_key_32_bytes").asString();
            try {
                std::string encrypted = EncryptionUtil::encrypt(body, aesKey);
                resp->setBody(std::move(encrypted));
                resp->addHeader("X-Encrypted-Payload", "true");
                
                // Add CORS headers specifically for pre-routing / post-handling flow as well
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, Idempotency-Key, X-Encrypted-Payload");
                resp->addHeader("Access-Control-Expose-Headers", "Idempotency-Key, X-Encrypted-Payload");
                
                resp->setContentTypeCode(drogon::ContentType::CT_TEXT_PLAIN);
            } catch (const std::exception& e) {
                LOG_ERROR << "Response payload encryption failed: " << e.what();
            }
        }
    }
}

} // namespace banking::security
