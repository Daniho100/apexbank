#pragma once
#include <string>
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::services {

class LedgerService {
public:
    // Generate a secure transaction reference number
    static std::string generateReference(const std::string& prefix);

    // Check if an idempotency key has already been executed and return the cached response
    static std::optional<Json::Value> checkIdempotency(const std::string& idempotencyKey);

    // Process customer cash deposit
    static Json::Value processDeposit(
        const std::string& accountNo,
        double amount,
        const std::string& idempotencyKey,
        const std::string& description = "Cash Deposit"
    );

    // Process customer withdrawal
    static Json::Value processWithdrawal(
        const std::string& accountNo,
        double amount,
        const std::string& idempotencyKey,
        const std::string& description = "Cash Withdrawal"
    );

    // Process internal account-to-account transfer
    static Json::Value processTransfer(
        const std::string& senderAccountNo,
        const std::string& receiverAccountNo,
        double amount,
        const std::string& idempotencyKey,
        const std::string& description = "Internal Transfer"
    );

    // Execute arbitrary ledger postings inside a shared transactional block (for loan auto-deduct/interests)
    static void postLedgerEntry(
        const std::shared_ptr<drogon::orm::Transaction>& trans,
        const std::string& transactionId,
        const std::string& accountId,
        const std::string& type, // "DEBIT" or "CREDIT"
        double amount,
        double balanceAfter,
        const std::string& description
    );

    // Recalculate account balance directly from ledger entries to verify cached balance correctness (Reconciliation)
    static double reconcileBalance(const std::string& accountId);
};

} // namespace banking::services
