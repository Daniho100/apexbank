#include "FixedDepositManager.h"
#include "LedgerService.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace banking::services {

Json::Value FixedDepositManager::createFixedDeposit(
    const std::string& userId,
    double amount,
    int durationMonths,
    double annualRate
) {
    if (amount <= 0.0 || durationMonths <= 0) {
        throw std::invalid_argument("Fixed deposit amount and duration must be greater than zero.");
    }
    
    auto db = drogon::app().getDbClient();
    auto trans = db->newTransaction();
    
    try {
        // 1. Lock customer savings account
        auto custResult = trans->execSqlSync(
            "SELECT id, balance, status FROM accounts WHERE user_id = $1 AND type = 'savings' FOR UPDATE",
            userId
        );
        if (custResult.empty()) {
            throw std::runtime_error("Primary savings account not found.");
        }
        auto custRow = custResult[0];
        if (custRow["status"].as<std::string>() != "active") {
            throw std::runtime_error("Customer account is not active.");
        }
        std::string accountId = custRow["id"].as<std::string>();
        double currentCustBalance = custRow["balance"].as<double>();
        
        // 2. Validate customer balance
        if (currentCustBalance < amount) {
            throw std::runtime_error("Insufficient funds in savings account to open Fixed Deposit.");
        }
        
        // 3. Lock System Treasury
        auto treasResult = trans->execSqlSync(
            "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
        );
        if (treasResult.empty()) {
            throw std::runtime_error("System Treasury not found.");
        }
        std::string treasuryAccountId = treasResult[0]["id"].as<std::string>();
        double currentTreasBalance = treasResult[0]["balance"].as<double>();
        
        // 4. Calculate maturity date
        // Let's use PostgreSQL interval arithmetic to generate the exact maturity date
        auto dateResult = trans->execSqlSync(
            "SELECT CURRENT_TIMESTAMP + ($1 || ' Month')::INTERVAL as maturity_date",
            std::to_string(durationMonths)
        );
        std::string maturityDate = dateResult[0]["maturity_date"].as<std::string>();
        
        // 5. Generate Certificate Number
        std::stringstream ss;
        ss << "FD_CERT_" << durationMonths << "M_";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        ss << dis(gen);
        std::string certNo = ss.str();
        
        // 6. Compute new balances
        double newCustBalance = currentCustBalance - amount;
        double newTreasBalance = currentTreasBalance + amount;
        
        // 7. Update balances in Database
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newCustBalance, accountId);
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newTreasBalance, treasuryAccountId);
        
        // 8. Create Fixed Deposit Record
        std::string fdId = drogon::utils::getUuid();
        trans->execSqlSync(
            "INSERT INTO fixed_deposits (id, user_id, amount, duration_months, interest_rate, status, maturity_date, certificate_number) "
            "VALUES ($1, $2, $3, $4, $5, 'active', $6, $7)",
            fdId,
            userId,
            amount,
            durationMonths,
            annualRate,
            maturityDate,
            certNo
        );
        
        // 9. Create Transaction Record
        std::string txnId = drogon::utils::getUuid();
        std::string ref = LedgerService::generateReference("FDC");
        std::string desc = "Fixed Deposit opening: Certificate " + certNo;
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description) "
            "VALUES ($1, 'fixed_deposit_creation', $2, 'completed', $3, $4, $5, $6, $7)",
            txnId,
            amount,
            drogon::utils::getUuid(),
            accountId,
            treasuryAccountId,
            ref,
            desc
        );
        
        // 10. Post Ledger Entries (Double-Entry Bookkeeping)
        LedgerService::postLedgerEntry(trans, txnId, accountId, "DEBIT", amount, newCustBalance, desc);
        LedgerService::postLedgerEntry(trans, txnId, treasuryAccountId, "CREDIT", amount, newTreasBalance, "Fixed deposit intake from User savings account");
        
        // Audit log
        trans->execSqlSync(
            "INSERT INTO audit_logs (user_id, action, description) "
            "VALUES ($1, 'fixed_deposit_creation', $2)",
            userId,
            "Opened fixed deposit " + fdId + " of amount " + std::to_string(amount) + " NGN. Cert No: " + certNo
        );
        
        // Notification
        trans->execSqlSync(
            "INSERT INTO notifications (user_id, title, content, type) "
            "VALUES ($1, 'Fixed Deposit Opened', $2, 'in_app')",
            userId,
            "Your Fixed Deposit of " + std::to_string(amount) + " NGN has been opened successfully. Certificate: " + certNo
        );
        
        trans->execSqlSync("COMMIT");
        
        Json::Value resp;
        resp["status"] = "success";
        resp["fixed_deposit_id"] = fdId;
        resp["certificate_number"] = certNo;
        resp["maturity_date"] = maturityDate;
        resp["balance_after"] = newCustBalance;
        return resp;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Fixed deposit creation failed: " << e.what();
        throw;
    }
}

Json::Value FixedDepositManager::earlyWithdraw(
    const std::string& fdId,
    const std::string& userId
) {
    auto db = drogon::app().getDbClient();
    auto trans = db->newTransaction();
    
    try {
        // 1. Lock fixed deposit
        auto fdResult = trans->execSqlSync(
            "SELECT id, amount, interest_rate, certificate_number, status, start_date, "
            "EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - start_date)) / 86400 as days_elapsed "
            "FROM fixed_deposits WHERE id = $1 AND user_id = $2 FOR UPDATE",
            fdId, userId
        );
        
        if (fdResult.empty()) {
            throw std::runtime_error("Fixed deposit not found.");
        }
        auto fdRow = fdResult[0];
        if (fdRow["status"].as<std::string>() != "active") {
            throw std::runtime_error("Fixed deposit is not active (status: " + fdRow["status"].as<std::string>() + ").");
        }
        
        double amount = fdRow["amount"].as<double>();
        double rate = fdRow["interest_rate"].as<double>();
        std::string certNo = fdRow["certificate_number"].as<std::string>();
        double daysElapsed = fdRow["days_elapsed"].as<double>();
        if (daysElapsed < 0) daysElapsed = 0;
        
        // 2. Calculate accrued interest
        double annualRate = rate / 100.0;
        double accruedInterest = amount * annualRate * (daysElapsed / 365.0);
        
        // Apply 50% early withdrawal penalty on accrued interest
        double penalty = accruedInterest * 0.5;
        double finalInterestPaid = accruedInterest - penalty;
        double totalPayout = amount + finalInterestPaid;
        
        // 3. Lock Customer savings account
        auto custResult = trans->execSqlSync(
            "SELECT id, balance, status FROM accounts WHERE user_id = $1 AND type = 'savings' FOR UPDATE",
            userId
        );
        if (custResult.empty()) {
            throw std::runtime_error("Primary savings account not found.");
        }
        auto custRow = custResult[0];
        std::string accountId = custRow["id"].as<std::string>();
        double currentCustBalance = custRow["balance"].as<double>();
        
        // 4. Lock System Treasury
        auto treasResult = trans->execSqlSync(
            "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
        );
        if (treasResult.empty()) {
            throw std::runtime_error("System Treasury not found.");
        }
        std::string treasuryAccountId = treasResult[0]["id"].as<std::string>();
        double currentTreasBalance = treasResult[0]["balance"].as<double>();
        
        // 5. Update Fixed Deposit Status
        trans->execSqlSync(
            "UPDATE fixed_deposits SET status = 'early_withdrawn', updated_at = CURRENT_TIMESTAMP WHERE id = $1",
            fdId
        );
        
        // 6. Update balances
        double newCustBalance = currentCustBalance + totalPayout;
        double newTreasBalance = currentTreasBalance - totalPayout;
        
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newCustBalance, accountId);
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newTreasBalance, treasuryAccountId);
        
        // 7. Create Transaction Record
        std::string txnId = drogon::utils::getUuid();
        std::string ref = LedgerService::generateReference("FDL");
        std::string desc = "Fixed Deposit early liquidation: Certificate " + certNo;
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description) "
            "VALUES ($1, 'fixed_deposit_maturity', $2, 'completed', $3, $4, $5, $6, $7)",
            txnId,
            totalPayout,
            drogon::utils::getUuid(),
            treasuryAccountId,
            accountId,
            ref,
            desc
        );
        
        // 8. Post Ledger Entries (Double-Entry Bookkeeping)
        LedgerService::postLedgerEntry(trans, txnId, treasuryAccountId, "DEBIT", totalPayout, newTreasBalance, "Early liquidation payout for Certificate " + certNo);
        LedgerService::postLedgerEntry(trans, txnId, accountId, "CREDIT", totalPayout, newCustBalance, desc);
        
        // Audit log
        trans->execSqlSync(
            "INSERT INTO audit_logs (user_id, action, description) "
            "VALUES ($1, 'fixed_deposit_early_withdrawal', $2)",
            userId,
            "Early liquidated fixed deposit " + fdId + ". Paid principal: " + std::to_string(amount) + " NGN, interest: " + std::to_string(finalInterestPaid) + " NGN (after penalty). Cert No: " + certNo
        );
        
        // Notification
        trans->execSqlSync(
            "INSERT INTO notifications (user_id, title, content, type) "
            "VALUES ($1, 'Fixed Deposit Early Liquidated', $2, 'in_app')",
            userId,
            "Your Fixed Deposit Certificate " + certNo + " was early liquidated. Paid: " + std::to_string(totalPayout) + " NGN (accrued interest penalized by 50%)."
        );
        
        trans->execSqlSync("COMMIT");
        
        Json::Value resp;
        resp["status"] = "success";
        resp["fixed_deposit_id"] = fdId;
        resp["payout_amount"] = totalPayout;
        resp["principal"] = amount;
        resp["interest_paid"] = finalInterestPaid;
        resp["interest_penalized"] = penalty;
        resp["balance_after"] = newCustBalance;
        return resp;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Fixed deposit early liquidation failed: " << e.what();
        throw;
    }
}

void FixedDepositManager::checkMaturities() {
    auto db = drogon::app().getDbClient();
    if (!db) return;
    
    try {
        // Query active matured fixed deposits
        auto maturedResult = db->execSqlSync(
            "SELECT id, user_id, amount, interest_rate, duration_months, certificate_number "
            "FROM fixed_deposits WHERE status = 'active' AND maturity_date <= CURRENT_TIMESTAMP"
        );
        
        if (maturedResult.empty()) return;
        
        LOG_INFO << "Found " << maturedResult.size() << " matured fixed deposits to liquidate.";
        
        for (size_t i = 0; i < maturedResult.size(); ++i) {
            auto row = maturedResult[i];
            std::string fdId = row["id"].as<std::string>();
            std::string userId = row["user_id"].as<std::string>();
            double amount = row["amount"].as<double>();
            double rate = row["interest_rate"].as<double>();
            int duration = row["duration_months"].as<int>();
            std::string certNo = row["certificate_number"].as<std::string>();
            
            auto trans = db->newTransaction();
            try {
                // Lock fixed deposit
                auto lockResult = trans->execSqlSync(
                    "SELECT status FROM fixed_deposits WHERE id = $1 FOR UPDATE",
                    fdId
                );
                if (lockResult.empty() || lockResult[0]["status"].as<std::string>() != "active") {
                    trans->rollback();
                    continue; // Already processed
                }
                
                // Calculate maturity interest: Simple Interest based on monthly fraction of year
                double annualRate = rate / 100.0;
                double interest = amount * annualRate * (duration / 12.0);
                double totalPayout = amount + interest;
                
                // Lock customer savings account
                auto custResult = trans->execSqlSync(
                    "SELECT id, balance FROM accounts WHERE user_id = $1 AND type = 'savings' FOR UPDATE",
                    userId
                );
                if (custResult.empty()) {
                    throw std::runtime_error("Primary savings account not found for user: " + userId);
                }
                std::string accountId = custResult[0]["id"].as<std::string>();
                double currentCustBalance = custResult[0]["balance"].as<double>();
                
                // Lock System Treasury
                auto treasResult = trans->execSqlSync(
                    "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
                );
                if (treasResult.empty()) {
                    throw std::runtime_error("System Treasury not found.");
                }
                std::string treasuryAccountId = treasResult[0]["id"].as<std::string>();
                double currentTreasBalance = treasResult[0]["balance"].as<double>();
                
                // Update fixed deposit status
                trans->execSqlSync(
                    "UPDATE fixed_deposits SET status = 'matured', updated_at = CURRENT_TIMESTAMP WHERE id = $1",
                    fdId
                );
                
                // Update balances
                double newCustBalance = currentCustBalance + totalPayout;
                double newTreasBalance = currentTreasBalance - totalPayout;
                
                trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newCustBalance, accountId);
                trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newTreasBalance, treasuryAccountId);
                
                // Create Transaction record
                std::string txnId = drogon::utils::getUuid();
                std::string ref = LedgerService::generateReference("FDM");
                std::string desc = "Fixed Deposit maturity payout: Certificate " + certNo;
                
                trans->execSqlSync(
                    "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description) "
                    "VALUES ($1, 'fixed_deposit_maturity', $2, 'completed', $3, $4, $5, $6, $7)",
                    txnId,
                    totalPayout,
                    drogon::utils::getUuid(),
                    treasuryAccountId,
                    accountId,
                    ref,
                    desc
                );
                
                // Post Ledger Entries (Double-Entry Bookkeeping)
                LedgerService::postLedgerEntry(trans, txnId, treasuryAccountId, "DEBIT", totalPayout, newTreasBalance, "Maturity liquidation payout for Certificate " + certNo);
                LedgerService::postLedgerEntry(trans, txnId, accountId, "CREDIT", totalPayout, newCustBalance, desc);
                
                // Audit log
                trans->execSqlSync(
                    "INSERT INTO audit_logs (user_id, action, description) "
                    "VALUES ($1, 'fixed_deposit_maturity_payout', $2)",
                    userId,
                    "Matured fixed deposit " + fdId + " payout of " + std::to_string(totalPayout) + " NGN (principal: " + std::to_string(amount) + ", interest: " + std::to_string(interest) + "). Cert No: " + certNo
                );
                
                // Notification
                trans->execSqlSync(
                    "INSERT INTO notifications (user_id, title, content, type) "
                    "VALUES ($1, 'Fixed Deposit Matured', $2, 'in_app')",
                    userId,
                    "Your Fixed Deposit Certificate " + certNo + " has matured! A total payout of " + std::to_string(totalPayout) + " NGN has been paid into your savings account."
                );
                
                trans->execSqlSync("COMMIT");
                LOG_INFO << "Processed maturity payout for FD Certificate: " << certNo;
                
            } catch (const std::exception& e) {
                trans->rollback();
                LOG_ERROR << "Failed to process maturity payout for FD " << fdId << ": " << e.what();
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "checkMaturities scan failed: " << e.what();
    }
}

} // namespace banking::services
