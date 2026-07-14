#include "LoanEngine.h"
#include "LedgerService.h"
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace banking::services {

double LoanEngine::calculateEMI(double principal, double annualRate, int durationMonths) {
    if (durationMonths <= 0) return 0.0;
    if (annualRate <= 0.0) {
        return principal / durationMonths;
    }
    
    double monthlyRate = (annualRate / 12.0) / 100.0;
    // Amortization Formula: M = P * [r(1+r)^n] / [(1+r)^n - 1]
    double emi = principal * (monthlyRate * std::pow(1 + monthlyRate, durationMonths)) / 
                 (std::pow(1 + monthlyRate, durationMonths) - 1);
    return emi;
}

std::vector<Installment> LoanEngine::generateSchedule(double principal, double annualRate, int durationMonths) {
    std::vector<Installment> schedule;
    double outstanding = principal;
    double monthlyRate = (annualRate / 12.0) / 100.0;
    double emi = calculateEMI(principal, annualRate, durationMonths);
    
    auto now = std::chrono::system_clock::now();
    
    for (int i = 1; i <= durationMonths; ++i) {
        double interest = 0.0;
        double principalPaid = 0.0;
        
        if (annualRate > 0.0) {
            interest = outstanding * monthlyRate;
            principalPaid = emi - interest;
        } else {
            principalPaid = principal / durationMonths;
        }
        
        if (principalPaid > outstanding || i == durationMonths) {
            principalPaid = outstanding;
            emi = principalPaid + interest;
        }
        
        outstanding -= principalPaid;
        if (outstanding < 0.0) outstanding = 0.0;
        
        // Calculate due date (approx 30 days per month offset)
        auto futureTime = now + std::chrono::hours(24 * 30 * i);
        auto timeTFuture = std::chrono::system_clock::to_time_t(futureTime);
        struct tm buf;
#ifdef _WIN32
        localtime_s(&buf, &timeTFuture);
#else
        localtime_r(&timeTFuture, &buf);
#endif
        std::stringstream ss;
        ss << std::put_time(&buf, "%Y-%m-%d 12:00:00");
        
        schedule.push_back({i, principalPaid, interest, emi, ss.str()});
    }
    
    return schedule;
}

bool LoanEngine::checkEligibility(const std::string& userId, std::string& outReason) {
    auto db = drogon::app().getDbClient();
    if (!db) {
        outReason = "Database client unavailable";
        return false;
    }
    
    try {
        // 1. Check for defaulted loans
        auto defaultResult = db->execSqlSync(
            "SELECT id FROM loans WHERE user_id = $1 AND status = 'defaulted'",
            userId
        );
        if (!defaultResult.empty()) {
            outReason = "User has active defaulted loans.";
            return false;
        }
        
        // 2. Check for overdue installments in schedules
        auto overdueResult = db->execSqlSync(
            "SELECT ls.id "
            "FROM loan_schedules ls "
            "JOIN loans l ON ls.loan_id = l.id "
            "WHERE l.user_id = $1 AND ls.status IN ('unpaid', 'partial', 'overdue') AND ls.due_date < CURRENT_TIMESTAMP",
            userId
        );
        if (!overdueResult.empty()) {
            outReason = "User has outstanding overdue repayments.";
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        outReason = "Verification error: " + std::string(e.what());
        return false;
    }
}

Json::Value LoanEngine::applyForLoan(const std::string& userId, double amount, int durationMonths, double annualRate, const std::string& name) {
    if (amount <= 0.0 || durationMonths <= 0) {
        throw std::invalid_argument("Invalid loan amount or duration.");
    }
    
    std::string reason;
    if (!checkEligibility(userId, reason)) {
        Json::Value error;
        error["status"] = "rejected";
        error["reason"] = reason;
        return error;
    }
    
    auto db = drogon::app().getDbClient();
    if (!db) {
        throw std::runtime_error("Database connection lost.");
    }

    // Check user loan limit
    auto limitResult = db->execSqlSync("SELECT loan_limit FROM users WHERE id = $1", userId);
    double limit = 1000000.0;
    if (!limitResult.empty() && !limitResult[0]["loan_limit"].isNull()) {
        limit = limitResult[0]["loan_limit"].as<double>();
    }
    
    auto outstandingResult = db->execSqlSync(
        "SELECT SUM(outstanding_balance) as total FROM loans WHERE user_id = $1 AND status IN ('approved', 'disbursed', 'active')",
        userId
    );
    double totalOutstanding = 0.0;
    if (!outstandingResult.empty() && !outstandingResult[0]["total"].isNull()) {
        totalOutstanding = outstandingResult[0]["total"].as<double>();
    }
    
    if (totalOutstanding + amount > limit) {
        Json::Value error;
        error["status"] = "rejected";
        error["reason"] = "Requested loan amount exceeds user credit limit (Limit: " + 
                          std::to_string((int)limit) + " NGN, Outstanding balance: " + 
                          std::to_string((int)totalOutstanding) + " NGN).";
        return error;
    }

    double monthlyRepayment = calculateEMI(amount, annualRate, durationMonths);
    
    std::string loanId = drogon::utils::getUuid();
    std::string referenceNumber = LedgerService::generateReference("LN");
    db->execSqlSync(
        "INSERT INTO loans (id, user_id, amount, interest_rate, duration_months, monthly_repayment, outstanding_balance, status, name, reference_number) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, 'pending', $8, $9)",
        loanId,
        userId,
        amount,
        annualRate,
        durationMonths,
        monthlyRepayment,
        amount,
        name,
        referenceNumber
    );
    
    // Log audit event
    db->execSqlSync(
        "INSERT INTO audit_logs (user_id, action, description) "
        "VALUES ($1, 'loan_application', $2)",
        userId,
        "Applied for loan of " + std::to_string(amount) + " NGN for " + std::to_string(durationMonths) + " months."
    );
    
    Json::Value resp;
    resp["status"] = "pending";
    resp["loan_id"] = loanId;
    resp["monthly_repayment"] = monthlyRepayment;
    resp["message"] = "Loan application submitted successfully.";
    return resp;
}

Json::Value LoanEngine::approveAndDisburse(const std::string& loanId, const std::string& adminId) {
    auto db = drogon::app().getDbClient();
    auto trans = db->newTransaction();
    
    try {
        // 1. Lock Loan
        auto loanResult = trans->execSqlSync(
            "SELECT user_id, amount, interest_rate, duration_months, status FROM loans WHERE id = $1 FOR UPDATE",
            loanId
        );
        if (loanResult.empty()) {
            throw std::runtime_error("Loan not found.");
        }
        auto loanRow = loanResult[0];
        if (loanRow["status"].as<std::string>() != "pending") {
            throw std::runtime_error("Loan is already processed (status: " + loanRow["status"].as<std::string>() + ").");
        }
        
        std::string userId = loanRow["user_id"].as<std::string>();
        double amount = loanRow["amount"].as<double>();
        double rate = loanRow["interest_rate"].as<double>();
        int duration = loanRow["duration_months"].as<int>();
        
        // 2. Fetch customer's primary account to credit
        auto accountResult = trans->execSqlSync(
            "SELECT id, account_number, balance, status FROM accounts WHERE user_id = $1 AND type = 'savings' FOR UPDATE",
            userId
        );
        if (accountResult.empty()) {
            throw std::runtime_error("Customer savings account not found for disbursement.");
        }
        auto accRow = accountResult[0];
        if (accRow["status"].as<std::string>() != "active") {
            throw std::runtime_error("Customer account is not active.");
        }
        std::string accountId = accRow["id"].as<std::string>();
        std::string accountNo = accRow["account_number"].as<std::string>();
        double currentBalance = accRow["balance"].as<double>();
        
        // 3. Lock System Treasury
        auto treasResult = trans->execSqlSync(
            "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
        );
        if (treasResult.empty()) {
            throw std::runtime_error("System Treasury not found.");
        }
        auto treasRow = treasResult[0];
        std::string treasuryAccountId = treasRow["id"].as<std::string>();
        double currentTreasBalance = treasRow["balance"].as<double>();
        
        // 5. Generate schedules in DB and calculate total outstanding (principal + interest)
        auto schedules = generateSchedule(amount, rate, duration);
        double totalOutstanding = 0.0;
        for (const auto& inst : schedules) {
            totalOutstanding += inst.total;
            trans->execSqlSync(
                "INSERT INTO loan_schedules (loan_id, installment_number, amount_due, principal_due, interest_due, status, due_date) "
                "VALUES ($1, $2, $3, $4, $5, 'unpaid', $6)",
                loanId,
                inst.number,
                inst.total,
                inst.principal,
                inst.interest,
                inst.dueDate
            );
        }
        
        // 4. Update Loan state to disbursed/active and set outstanding_balance to total (principal + interest)
        trans->execSqlSync(
            "UPDATE loans SET status = 'active', outstanding_balance = $1, approved_at = CURRENT_TIMESTAMP, disbursed_at = CURRENT_TIMESTAMP WHERE id = $2",
            totalOutstanding, loanId
        );
        
        // 6. Double-entry ledger processing (Treasury pays customer)
        double newCustBalance = currentBalance + amount;
        double newTreasBalance = currentTreasBalance - amount;
        
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newCustBalance, accountId);
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newTreasBalance, treasuryAccountId);
        
        std::string transactionId = drogon::utils::getUuid();
        std::string reference = LedgerService::generateReference("LND");
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description) "
            "VALUES ($1, 'loan_disbursement', $2, 'completed', $3, $4, $5, $6, 'Loan disbursement payout')",
            transactionId,
            amount,
            drogon::utils::getUuid(), // generated key
            treasuryAccountId,
            accountId,
            reference
        );
        
        LedgerService::postLedgerEntry(trans, transactionId, treasuryAccountId, "DEBIT", amount, newTreasBalance, "Disbursement debit for loan " + loanId);
        LedgerService::postLedgerEntry(trans, transactionId, accountId, "CREDIT", amount, newCustBalance, "Loan disbursement credit");
        
        // 7. Audit log
        trans->execSqlSync(
            "INSERT INTO audit_logs (user_id, action, description) "
            "VALUES ($1, 'loan_disbursement', $2)",
            adminId,
            "Approved and disbursed loan " + loanId + " to user " + userId
        );
        
        // 8. Notification
        trans->execSqlSync(
            "INSERT INTO notifications (user_id, title, content, type) "
            "VALUES ($1, 'Loan Disbursed', $2, 'in_app')",
            userId,
            "Your loan of " + std::to_string(amount) + " NGN has been disbursed into your savings account."
        );
        
        trans->execSqlSync("COMMIT");
        
        Json::Value resp;
        resp["status"] = "success";
        resp["loan_id"] = loanId;
        resp["disbursed_amount"] = amount;
        return resp;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Loan disbursement failed: " << e.what();
        throw;
    }
}

double LoanEngine::checkAndAutoDeductOverdue(
    const std::shared_ptr<drogon::orm::Transaction>& trans,
    const std::string& accountId,
    double incomingAmount,
    const std::string& correlationId
) {
    // 1. Fetch account and user details
    auto accResult = trans->execSqlSync(
        "SELECT user_id, balance, account_number FROM accounts WHERE id = $1",
        accountId
    );
    if (accResult.empty()) return 0.0;
    
    std::string userId = accResult[0]["user_id"].as<std::string>();
    std::string customerAccountNo = accResult[0]["account_number"].as<std::string>();
    double customerBalance = accResult[0]["balance"].as<double>();
    
    // 2. Fetch overdue installments
    auto overdueResult = trans->execSqlSync(
        "SELECT ls.id, ls.loan_id, ls.installment_number, ls.amount_due, ls.amount_paid, l.outstanding_balance "
        "FROM loan_schedules ls "
        "JOIN loans l ON ls.loan_id = l.id "
        "WHERE l.user_id = $1 AND ls.status IN ('unpaid', 'partial', 'overdue') AND ls.due_date < CURRENT_TIMESTAMP "
        "ORDER BY ls.due_date ASC",
        userId
    );
    
    if (overdueResult.empty()) {
        return 0.0; // No overdue loan schedules
    }
    
    // Fetch System Treasury Account
    auto treasResult = trans->execSqlSync(
        "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
    );
    if (treasResult.empty()) {
        throw std::runtime_error("System Treasury not found during auto-debit.");
    }
    std::string treasuryAccountId = treasResult[0]["id"].as<std::string>();
    double treasuryBalance = treasResult[0]["balance"].as<double>();
    
    double totalDeducted = 0.0;
    double currentIncoming = incomingAmount;
    
    for (size_t i = 0; i < overdueResult.size(); ++i) {
        if (currentIncoming <= 0.0) break;
        
        auto row = overdueResult[i];
        std::string scheduleId = row["id"].as<std::string>();
        std::string loanId = row["loan_id"].as<std::string>();
        double due = row["amount_due"].as<double>();
        double paid = row["amount_paid"].as<double>();
        double loanOutstanding = row["outstanding_balance"].as<double>();
        
        double unpaid = due - paid;
        double payment = std::min(currentIncoming, unpaid);
        
        // Update schedule
        double newPaid = paid + payment;
        std::string newStatus = (newPaid >= due) ? "paid" : "partial";
        trans->execSqlSync(
            "UPDATE loan_schedules SET amount_paid = $1, status = $2, paid_at = CURRENT_TIMESTAMP WHERE id = $3",
            newPaid, newStatus, scheduleId
        );
        
        // Update loan outstanding balance
        double newOutstanding = loanOutstanding - payment;
        if (newOutstanding < 0.0) newOutstanding = 0.0;
        std::string loanStatus = (newOutstanding <= 0.0) ? "completed" : "active";
        trans->execSqlSync(
            "UPDATE loans SET outstanding_balance = $1, status = $2, updated_at = CURRENT_TIMESTAMP WHERE id = $3",
            newOutstanding, loanStatus, loanId
        );
        
        // Deduct from customer balance
        customerBalance -= payment;
        treasuryBalance += payment;
        currentIncoming -= payment;
        totalDeducted += payment;
        
        // Record auto-debit transaction in ledger
        std::string txnId = drogon::utils::getUuid();
        std::string ref = LedgerService::generateReference("LNP");
        std::string desc = "Auto-deduction for overdue installment " + std::to_string(row["installment_number"].as<int>()) + " of Loan " + loanId;
        
        Json::Value metadataJson;
        metadataJson["correlation_id"] = correlationId;
        Json::FastWriter writer;
        std::string metadataStr = writer.write(metadataJson);
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description, metadata) "
            "VALUES ($1, 'loan_repayment', $2, 'completed', $3, $4, $5, $6, $7, $8)",
            txnId,
            payment,
            drogon::utils::getUuid(),
            accountId,
            treasuryAccountId,
            ref,
            desc,
            metadataStr
        );
        
        LedgerService::postLedgerEntry(trans, txnId, accountId, "DEBIT", payment, customerBalance, desc);
        LedgerService::postLedgerEntry(trans, txnId, treasuryAccountId, "CREDIT", payment, treasuryBalance, "Overdue loan repayment collection from " + customerAccountNo);
    }
    
    // Update accounts balances in database
    trans->execSqlSync("UPDATE accounts SET balance = $1 WHERE id = $2", customerBalance, accountId);
    trans->execSqlSync("UPDATE accounts SET balance = $1 WHERE id = $2", treasuryBalance, treasuryAccountId);
    
    // Notify customer
    trans->execSqlSync(
        "INSERT INTO notifications (user_id, title, content, type) "
        "VALUES ($1, 'Overdue Repayment Auto-Deducted', $2, 'in_app')",
        userId,
        "An overdue loan repayment of " + std::to_string(totalDeducted) + " NGN was automatically deducted from your incoming deposit."
    );
    
    return totalDeducted;
}

Json::Value LoanEngine::processRepayment(
    const std::string& loanId,
    const std::string& accountNo,
    double amount,
    const std::string& idempotencyKey
) {
    if (amount <= 0.0) {
        throw std::invalid_argument("Repayment amount must be greater than zero.");
    }
    
    // Idempotency safety check
    auto cached = LedgerService::checkIdempotency(idempotencyKey);
    if (cached.has_value()) {
        if (cached.value().isMember("response_cache")) {
            return cached.value()["response_cache"];
        }
        return cached.value();
    }
    
    auto db = drogon::app().getDbClient();
    auto trans = db->newTransaction();
    
    try {
        // 1. Lock Customer Account
        auto accResult = trans->execSqlSync(
            "SELECT id, balance, status, user_id FROM accounts WHERE account_number = $1 FOR UPDATE",
            accountNo
        );
        if (accResult.empty()) {
            throw std::runtime_error("Account not found.");
        }
        auto accRow = accResult[0];
        if (accRow["status"].as<std::string>() != "active") {
            throw std::runtime_error("Customer account is not active.");
        }
        std::string accountId = accRow["id"].as<std::string>();
        double currentCustBalance = accRow["balance"].as<double>();
        std::string userId = accRow["user_id"].as<std::string>();
        
        if (currentCustBalance < amount) {
            throw std::runtime_error("Insufficient funds for loan repayment.");
        }
        
        auto loanResult = trans->execSqlSync(
            "SELECT id, outstanding_balance, status FROM loans WHERE (id::text = $1 OR reference_number = $1) AND user_id = $2 FOR UPDATE",
            loanId, userId
        );
        if (loanResult.empty()) {
            throw std::runtime_error("Loan not found for this user.");
        }
        auto loanRow = loanResult[0];
        std::string actualLoanId = loanRow["id"].as<std::string>();
        double outstanding = loanRow["outstanding_balance"].as<double>();
        
        if (outstanding <= 0.0 || loanRow["status"].as<std::string>() == "completed") {
            throw std::runtime_error("Loan is already fully repaid.");
        }
        
        // Limit repayment to outstanding balance
        double payment = std::min(amount, outstanding);
        
        // Lock system treasury
        auto treasResult = trans->execSqlSync(
            "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
        );
        if (treasResult.empty()) {
            throw std::runtime_error("System Treasury not found.");
        }
        std::string treasuryAccountId = treasResult[0]["id"].as<std::string>();
        double currentTreasBalance = treasResult[0]["balance"].as<double>();
        
        // 3. Update Loan and schedules
        double newOutstanding = outstanding - payment;
        std::string newStatus = (newOutstanding <= 0.0) ? "completed" : "active";
        trans->execSqlSync(
            "UPDATE loans SET outstanding_balance = $1, status = $2, updated_at = CURRENT_TIMESTAMP WHERE id = $3",
            newOutstanding, newStatus, actualLoanId
        );
        
        // Fetch unpaid schedules to allocate payment
        auto schedsResult = trans->execSqlSync(
            "SELECT id, amount_due, amount_paid FROM loan_schedules "
            "WHERE loan_id = $1 AND status IN ('unpaid', 'partial', 'overdue') "
            "ORDER BY due_date ASC",
            actualLoanId
        );
        
        double currentPaymentLeft = payment;
        for (size_t i = 0; i < schedsResult.size(); ++i) {
            if (currentPaymentLeft <= 0.0) break;
            
            auto row = schedsResult[i];
            std::string schedId = row["id"].as<std::string>();
            double due = row["amount_due"].as<double>();
            double paid = row["amount_paid"].as<double>();
            
            double unpaid = due - paid;
            double allocate = std::min(currentPaymentLeft, unpaid);
            
            double newPaid = paid + allocate;
            std::string schedStatus = (newPaid >= due) ? "paid" : "partial";
            
            trans->execSqlSync(
                "UPDATE loan_schedules SET amount_paid = $1, status = $2, paid_at = CURRENT_TIMESTAMP WHERE id = $3",
                newPaid, schedStatus, schedId
            );
            currentPaymentLeft -= allocate;
        }
        
        // 4. Update balances
        double newCustBalance = currentCustBalance - payment;
        double newTreasBalance = currentTreasBalance + payment;
        
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newCustBalance, accountId);
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newTreasBalance, treasuryAccountId);
        
        // 5. Create Transaction and Ledger entries
        std::string txnId = drogon::utils::getUuid();
        std::string ref = LedgerService::generateReference("LNP");
        std::string desc = "Manual loan repayment for loan " + actualLoanId;
        
        Json::Value responseJson;
        responseJson["status"] = "success";
        responseJson["message"] = "Loan repayment processed successfully";
        responseJson["reference_number"] = ref;
        responseJson["amount_paid"] = payment;
        responseJson["outstanding_balance_after"] = newOutstanding;
        responseJson["balance_after"] = newCustBalance;
        
        Json::Value metadataJson;
        metadataJson["response_cache"] = responseJson;
        Json::FastWriter writer;
        std::string metadataStr = writer.write(metadataJson);
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description, metadata) "
            "VALUES ($1, 'loan_repayment', $2, 'completed', $3, $4, $5, $6, $7, $8)",
            txnId,
            payment,
            idempotencyKey,
            accountId,
            treasuryAccountId,
            ref,
            desc,
            metadataStr
        );
        
        LedgerService::postLedgerEntry(trans, txnId, accountId, "DEBIT", payment, newCustBalance, desc);
        LedgerService::postLedgerEntry(trans, txnId, treasuryAccountId, "CREDIT", payment, newTreasBalance, "Loan manual repayment collection from " + accountNo);
        
        trans->execSqlSync("COMMIT");
        return responseJson;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Manual loan repayment failed: " << e.what();
        throw;
    }
}

} // namespace banking::services
