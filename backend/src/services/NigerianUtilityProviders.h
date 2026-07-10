#pragma once
#include <string>
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::services {

class NigerianUtilityProviders {
public:
    // Buy Airtime (MTN, Airtel, Glo, 9mobile)
    static Json::Value buyAirtime(
        const std::string& accountNo,
        const std::string& provider, // "MTN", "Airtel", "Glo", "9mobile"
        const std::string& phoneNumber,
        double amount,
        const std::string& idempotencyKey
    );

    // Buy Data Bundles (MTN, Airtel, Glo, 9mobile)
    static Json::Value buyData(
        const std::string& accountNo,
        const std::string& provider,
        const std::string& phoneNumber,
        const std::string& bundleName,
        double amount,
        const std::string& idempotencyKey
    );

    // Purchase Prepaid Electricity (Prepaid Meter Token generation)
    static Json::Value purchaseElectricity(
        const std::string& accountNo,
        const std::string& meterNumber,
        double amount,
        const std::string& idempotencyKey
    );

    // Validate prepaid token and simulate adding units to prepaid meter
    static Json::Value validatePrepaidToken(
        const std::string& token
    );

    // Pay Cable TV subscription (DSTV, GOtv, Startimes)
    static Json::Value payCableTV(
        const std::string& accountNo,
        const std::string& provider, // "DSTV", "GOtv", "Startimes"
        const std::string& smartCardNumber,
        const std::string& packageName,
        double amount,
        const std::string& idempotencyKey
    );

    // Place Bet (dummy betting provider payouts/deductions)
    static Json::Value placeBet(
        const std::string& accountNo,
        const std::string& provider, // "SportyBet", "Bet9ja"
        const std::string& bettingCustomerId,
        double amount,
        const std::string& idempotencyKey
    );

private:
    // Generate a 20-digit unique electricity token
    static std::string generateElectricityToken();
};

} // namespace banking::services
