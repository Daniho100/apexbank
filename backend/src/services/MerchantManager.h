#pragma once
#include <string>
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::services {

class MerchantManager {
public:
    // Register a merchant business profile
    static Json::Value registerMerchant(
        const std::string& userId,
        const std::string& businessName,
        const std::string& webhookUrl
    );

    // Create a merchant invoice
    static Json::Value createInvoice(
        const std::string& merchantUserId,
        const std::string& customerAccountNo,
        double amount,
        const std::string& description
    );

    // Pay a merchant invoice and dispatch webhook event
    static Json::Value payInvoice(
        const std::string& invoiceId,
        const std::string& customerAccountNo,
        const std::string& idempotencyKey
    );

    // Generate cryptographic SHA256-HMAC webhook payload signature
    static std::string generateWebhookSignature(
        const std::string& payload,
        const std::string& apiKey
    );

    // Dispatch webhook event to merchant (Asynchronous simulated dispatcher)
    static void dispatchWebhook(
        const std::string& webhookUrl,
        const std::string& payload,
        const std::string& signature
    );
};

} // namespace banking::services
