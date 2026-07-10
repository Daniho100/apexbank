#include "NigerianUtilityProviders.h"
#include "LedgerService.h"
#include <random>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace banking::services {

std::string NigerianUtilityProviders::generateElectricityToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9);
    
    std::stringstream ss;
    for (int i = 0; i < 20; ++i) {
        if (i > 0 && i % 4 == 0) {
            ss << "-";
        }
        ss << dis(gen);
    }
    return ss.str();
}

Json::Value NigerianUtilityProviders::buyAirtime(
    const std::string& accountNo,
    const std::string& provider,
    const std::string& phoneNumber,
    double amount,
    const std::string& idempotencyKey
) {
    // 1. Validate inputs
    if (provider != "MTN" && provider != "Airtel" && provider != "Glo" && provider != "9mobile") {
        throw std::invalid_argument("Invalid telco provider: " + provider);
    }
    if (phoneNumber.empty() || phoneNumber.size() < 10) {
        throw std::invalid_argument("Invalid phone number.");
    }
    
    // 2. Process account debit via LedgerService
    // Ledger withdrawal returns transaction response if successful
    std::string desc = "Airtime Purchase: " + provider + " to " + phoneNumber;
    Json::Value debitResult = LedgerService::processWithdrawal(accountNo, amount, idempotencyKey, desc);
    
    // 3. Construct provider payload (simulated real-time provider response)
    Json::Value providerResponse;
    providerResponse["status"] = "success";
    providerResponse["network"] = provider;
    providerResponse["recipient"] = phoneNumber;
    providerResponse["amount"] = amount;
    providerResponse["provider_reference"] = "TELCO_" + LedgerService::generateReference("REF");
    providerResponse["account_deduct_details"] = debitResult;
    
    return providerResponse;
}

Json::Value NigerianUtilityProviders::buyData(
    const std::string& accountNo,
    const std::string& provider,
    const std::string& phoneNumber,
    const std::string& bundleName,
    double amount,
    const std::string& idempotencyKey
) {
    if (provider != "MTN" && provider != "Airtel" && provider != "Glo" && provider != "9mobile") {
        throw std::invalid_argument("Invalid telco provider: " + provider);
    }
    if (phoneNumber.empty()) {
        throw std::invalid_argument("Invalid phone number.");
    }
    
    std::string desc = "Data Bundle Purchase (" + bundleName + "): " + provider + " to " + phoneNumber;
    Json::Value debitResult = LedgerService::processWithdrawal(accountNo, amount, idempotencyKey, desc);
    
    Json::Value providerResponse;
    providerResponse["status"] = "success";
    providerResponse["network"] = provider;
    providerResponse["recipient"] = phoneNumber;
    providerResponse["bundle_name"] = bundleName;
    providerResponse["amount"] = amount;
    providerResponse["provider_reference"] = "DATA_" + LedgerService::generateReference("REF");
    providerResponse["account_deduct_details"] = debitResult;
    
    return providerResponse;
}

Json::Value NigerianUtilityProviders::purchaseElectricity(
    const std::string& accountNo,
    const std::string& meterNumber,
    double amount,
    const std::string& idempotencyKey
) {
    if (meterNumber.empty()) {
        throw std::invalid_argument("Meter number cannot be empty.");
    }
    
    // Check if the transaction with this key was already run
    auto cached = LedgerService::checkIdempotency(idempotencyKey);
    if (cached.has_value()) {
        if (cached.value().isMember("response_cache")) {
            return cached.value()["response_cache"];
        }
        return cached.value();
    }
    
    // Tariff is 85.0 NGN per unit (kWh)
    double units = amount / 85.00;
    std::string token = generateElectricityToken();
    
    // Process withdrawal
    std::string desc = "Electricity purchase for Meter " + meterNumber;
    Json::Value debitResult = LedgerService::processWithdrawal(accountNo, amount, idempotencyKey, desc);
    
    // Store generated token in Database
    auto db = drogon::app().getDbClient();
    db->execSqlSync(
        "INSERT INTO electricity_tokens (token, meter_number, units, amount, status) "
        "VALUES ($1, $2, $3, $4, 'unused')",
        token,
        meterNumber,
        units,
        amount
    );
    
    Json::Value providerResponse;
    providerResponse["status"] = "success";
    providerResponse["meter_number"] = meterNumber;
    providerResponse["token"] = token;
    providerResponse["units_kwh"] = units;
    providerResponse["amount"] = amount;
    providerResponse["message"] = "Electricity token generated successfully. Input token into prepaid meter to charge units.";
    providerResponse["account_deduct_details"] = debitResult;
    
    // Invalidate/cache this response in transactions table for idempotency
    // Since processWithdrawal writes the transaction with the idempotencyKey, we just need to append our response to transaction metadata
    try {
        Json::Value metadataJson;
        metadataJson["response_cache"] = providerResponse;
        Json::FastWriter writer;
        db->execSqlSync(
            "UPDATE transactions SET metadata = $1 WHERE idempotency_key = $2",
            writer.write(metadataJson),
            idempotencyKey
        );
    } catch (...) {}
    
    return providerResponse;
}

Json::Value NigerianUtilityProviders::validatePrepaidToken(const std::string& token) {
    auto db = drogon::app().getDbClient();
    if (!db) {
        throw std::runtime_error("Database connection lost.");
    }
    
    auto trans = db->newTransaction();
    
    try {
        auto result = trans->execSqlSync(
            "SELECT id, meter_number, units, status FROM electricity_tokens WHERE token = $1 FOR UPDATE",
            token
        );
        
        Json::Value resp;
        if (result.empty()) {
            resp["status"] = "failed";
            resp["message"] = "Invalid token. Meter recharge rejected.";
            trans->rollback();
            return resp;
        }
        
        auto row = result[0];
        std::string status = row["status"].as<std::string>();
        double units = row["units"].as<double>();
        std::string meter = row["meter_number"].as<std::string>();
        
        if (status == "used") {
            resp["status"] = "failed";
            resp["message"] = "Token has already been used. Meter recharge rejected.";
            trans->rollback();
            return resp;
        } else if (status == "expired") {
            resp["status"] = "failed";
            resp["message"] = "Token has expired. Meter recharge rejected.";
            trans->rollback();
            return resp;
        }
        
        // Mark as used
        trans->execSqlSync(
            "UPDATE electricity_tokens SET status = 'used', used_at = CURRENT_TIMESTAMP WHERE token = $1",
            token
        );
        
        trans->execSqlSync("COMMIT");
        
        resp["status"] = "success";
        resp["meter_number"] = meter;
        resp["units_added"] = units;
        resp["message"] = "Prepaid meter charged successfully with " + std::to_string(units) + " kWh.";
        return resp;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Validate prepaid token failed: " << e.what();
        throw;
    }
}

Json::Value NigerianUtilityProviders::payCableTV(
    const std::string& accountNo,
    const std::string& provider,
    const std::string& smartCardNumber,
    const std::string& packageName,
    double amount,
    const std::string& idempotencyKey
) {
    if (provider != "DSTV" && provider != "GOtv" && provider != "Startimes") {
        throw std::invalid_argument("Invalid cable TV provider: " + provider);
    }
    if (smartCardNumber.empty()) {
        throw std::invalid_argument("Smartcard number cannot be empty.");
    }
    
    std::string desc = "Cable TV Payment (" + packageName + "): " + provider + " to card " + smartCardNumber;
    Json::Value debitResult = LedgerService::processWithdrawal(accountNo, amount, idempotencyKey, desc);
    
    Json::Value providerResponse;
    providerResponse["status"] = "success";
    providerResponse["provider"] = provider;
    providerResponse["smartcard"] = smartCardNumber;
    providerResponse["package"] = packageName;
    providerResponse["amount"] = amount;
    providerResponse["message"] = "Subscription activated on device.";
    providerResponse["provider_reference"] = "TV_" + LedgerService::generateReference("REF");
    providerResponse["account_deduct_details"] = debitResult;
    
    return providerResponse;
}

Json::Value NigerianUtilityProviders::placeBet(
    const std::string& accountNo,
    const std::string& provider,
    const std::string& bettingCustomerId,
    double amount,
    const std::string& idempotencyKey
) {
    if (provider != "SportyBet" && provider != "Bet9ja") {
        throw std::invalid_argument("Invalid betting provider: " + provider);
    }
    if (bettingCustomerId.empty()) {
        throw std::invalid_argument("Betting Customer ID cannot be empty.");
    }
    
    std::string desc = "Betting wallet deposit (" + provider + "): Account " + bettingCustomerId;
    Json::Value debitResult = LedgerService::processWithdrawal(accountNo, amount, idempotencyKey, desc);
    
    Json::Value providerResponse;
    providerResponse["status"] = "success";
    providerResponse["provider"] = provider;
    providerResponse["customer_id"] = bettingCustomerId;
    providerResponse["amount"] = amount;
    providerResponse["message"] = "Funds successfully credited to betting wallet.";
    providerResponse["provider_reference"] = "BET_" + LedgerService::generateReference("REF");
    providerResponse["account_deduct_details"] = debitResult;
    
    return providerResponse;
}

} // namespace banking::services
