#pragma once
#include <string>
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::services {

class FixedDepositManager {
public:
    // Create a new Fixed Deposit from user's savings balance
    static Json::Value createFixedDeposit(
        const std::string& userId,
        double amount,
        int durationMonths,
        double annualRate
    );

    // Liquidate fixed deposit early (with interest reduction penalty)
    static Json::Value earlyWithdraw(
        const std::string& fdId,
        const std::string& userId
    );

    // Scan for matured fixed deposits and process automatic payouts (Run in background workers)
    static void checkMaturities();
};

} // namespace banking::services
