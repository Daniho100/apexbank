#include "LedgerService.h"
#include "LoanEngine.h"
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <stdexcept>

namespace banking::services {

std::string LedgerService::generateReference(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
#ifdef _WIN32
    localtime_s(&buf, &time_t_now);
#else
    localtime_r(&time_t_now, &buf);
#endif

    std::stringstream ss;
    ss << prefix << "_" 
       << std::put_time(&buf, "%Y%m%d%H%M%S");
       
    // Add 6 hex random characters to ensure collision avoidance
    unsigned char rand_bytes[3];
    if (RAND_bytes(rand_bytes, sizeof(rand_bytes)) == 1) {
        for (unsigned char b : rand_bytes) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
    } else {
        // Fallback to std::rand if OpenSSL RAND_bytes fails
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for(int i=0; i<3; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
        }
    }
    
    std::string ref = ss.str();
    std::transform(ref.begin(), ref.end(), ref.begin(), ::toupper);
    return ref;
}

std::optional<Json::Value> LedgerService::checkIdempotency(const std::string& idempotencyKey) {
    if (idempotencyKey.empty()) return std::nullopt;
    
    auto db = drogon::app().getDbClient();
    if (!db) return std::nullopt;
    
    try {
        auto result = db->execSqlSync(
            "SELECT id, type, amount, status, reference_number, description, metadata, created_at "
            "FROM transactions WHERE idempotency_key = $1",
            idempotencyKey
        );
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        auto row = result[0];
        Json::Value resp;
        resp["transaction_id"] = row["id"].as<std::string>();
        resp["type"] = row["type"].as<std::string>();
        resp["amount"] = row["amount"].as<double>();
        resp["status"] = row["status"].as<std::string>();
        resp["reference_number"] = row["reference_number"].as<std::string>();
        resp["description"] = row["description"].as<std::string>();
        resp["created_at"] = row["created_at"].as<std::string>();
        
        if (!row["metadata"].isNull() && !row["metadata"].as<std::string>().empty()) {
            Json::Reader reader;
            Json::Value meta;
            if (reader.parse(row["metadata"].as<std::string>(), meta)) {
                resp["response_cache"] = meta["response_cache"];
            }
        }
        
        return resp;
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to check transaction idempotency: " << e.what();
        return std::nullopt;
    }
}

void LedgerService::postLedgerEntry(
    const std::shared_ptr<drogon::orm::Transaction>& trans,
    const std::string& transactionId,
    const std::string& accountId,
    const std::string& type,
    double amount,
    double balanceAfter,
    const std::string& description
) {
    trans->execSqlSync(
        "INSERT INTO ledger_entries (transaction_id, account_id, type, amount, balance_after, description) "
        "VALUES ($1, $2, $3, $4, $5, $6)",
        transactionId,
        accountId,
        type,
        amount,
        balanceAfter,
        description
    );
}

Json::Value LedgerService::processDeposit(
    const std::string& accountNo,
    double amount,
    const std::string& idempotencyKey,
    const std::string& description
) {
    if (amount <= 0.0) {
        throw std::invalid_argument("Deposit amount must be greater than zero.");
    }
    
    // Check if transaction has already run (idempotency safety)
    auto cached = checkIdempotency(idempotencyKey);
    if (cached.has_value()) {
        if (cached.value().isMember("response_cache")) {
            return cached.value()["response_cache"];
        }
        return cached.value();
    }
    
    auto db = drogon::app().getDbClient();
    auto trans = db->newTransaction();
    
    try {
        // 1. Lock customer account
        auto custResult = trans->execSqlSync(
            "SELECT id, balance, status FROM accounts WHERE account_number = $1 FOR UPDATE",
            accountNo
        );
        if (custResult.empty()) {
            throw std::runtime_error("Account not found: " + accountNo);
        }
        auto custRow = custResult[0];
        if (custRow["status"].as<std::string>() != "active") {
            throw std::runtime_error("Deposit failed: Account is " + custRow["status"].as<std::string>() + ".");
        }
        std::string customerAccountId = custRow["id"].as<std::string>();
        double currentCustBalance = custRow["balance"].as<double>();
        
        // 2. Lock System Treasury
        auto treasResult = trans->execSqlSync(
            "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
        );
        if (treasResult.empty()) {
            throw std::runtime_error("Critical Error: System Treasury account not found!");
        }
        auto treasRow = treasResult[0];
        std::string treasuryAccountId = treasRow["id"].as<std::string>();
        double currentTreasBalance = treasRow["balance"].as<double>();
        
        // 3. Compute new balances
        double newCustBalance = currentCustBalance + amount;
        double newTreasBalance = currentTreasBalance - amount;
        
        // 4. Update cached balance in DB
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newCustBalance, customerAccountId);
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newTreasBalance, treasuryAccountId);
        
        // 5. Create Transaction Record
        std::string transactionId = drogon::utils::getUuid();
        std::string reference = generateReference("DEP");
        
        Json::Value responseJson;
        responseJson["status"] = "success";
        responseJson["message"] = "Deposit processed successfully";
        responseJson["reference_number"] = reference;
        responseJson["amount"] = amount;
        responseJson["balance_after"] = newCustBalance;
        responseJson["account_number"] = accountNo;
        responseJson["description"] = description;
        
        Json::Value metadataJson;
        metadataJson["response_cache"] = responseJson;
        Json::FastWriter writer;
        std::string metadataStr = writer.write(metadataJson);
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description, metadata) "
            "VALUES ($1, 'deposit', $2, 'completed', $3, $4, $5, $6, $7, $8)",
            transactionId,
            amount,
            idempotencyKey,
            treasuryAccountId,
            customerAccountId,
            reference,
            description,
            metadataStr
        );
        
        // 6. Post Ledger Entries (Double-Entry Bookkeeping)
        postLedgerEntry(trans, transactionId, treasuryAccountId, "DEBIT", amount, newTreasBalance, "System payout for Deposit to " + accountNo);
        postLedgerEntry(trans, transactionId, customerAccountId, "CREDIT", amount, newCustBalance, description);
        
        // 7. Auto-deduct overdue repayments if they exist
        double autoDeducted = LoanEngine::checkAndAutoDeductOverdue(trans, customerAccountId, amount, transactionId);
        if (autoDeducted > 0.0) {
            responseJson["auto_repayment_deducted"] = autoDeducted;
            responseJson["balance_after"] = newCustBalance - autoDeducted;
            
            metadataJson["response_cache"] = responseJson;
            trans->execSqlSync("UPDATE transactions SET metadata = $1 WHERE id = $2", writer.write(metadataJson), transactionId);
        }
        
        trans->execSqlSync("COMMIT");
        return responseJson;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Deposit failed & rolled back: " << e.what();
        throw;
    }
}

Json::Value LedgerService::processWithdrawal(
    const std::string& accountNo,
    double amount,
    const std::string& idempotencyKey,
    const std::string& description
) {
    if (amount <= 0.0) {
        throw std::invalid_argument("Withdrawal amount must be greater than zero.");
    }
    
    // Check if transaction has already run (idempotency safety)
    auto cached = checkIdempotency(idempotencyKey);
    if (cached.has_value()) {
        if (cached.value().isMember("response_cache")) {
            return cached.value()["response_cache"];
        }
        return cached.value();
    }
    
    auto db = drogon::app().getDbClient();
    auto trans = db->newTransaction();
    
    try {
        // 1. Lock customer account
        auto custResult = trans->execSqlSync(
            "SELECT id, balance, status FROM accounts WHERE account_number = $1 FOR UPDATE",
            accountNo
        );
        if (custResult.empty()) {
            throw std::runtime_error("Account not found: " + accountNo);
        }
        auto custRow = custResult[0];
        if (custRow["status"].as<std::string>() != "active") {
            throw std::runtime_error("Withdrawal failed: Account is " + custRow["status"].as<std::string>() + ".");
        }
        std::string customerAccountId = custRow["id"].as<std::string>();
        double currentCustBalance = custRow["balance"].as<double>();
        
        // 2. Validate Overdraft prevention
        if (currentCustBalance < amount) {
            throw std::runtime_error("Withdrawal failed: Insufficient funds.");
        }
        
        // 3. Lock System Treasury
        auto treasResult = trans->execSqlSync(
            "SELECT id, balance FROM accounts WHERE account_number = 'SYSTEM_TREASURY_001' FOR UPDATE"
        );
        if (treasResult.empty()) {
            throw std::runtime_error("Critical Error: System Treasury account not found!");
        }
        auto treasRow = treasResult[0];
        std::string treasuryAccountId = treasRow["id"].as<std::string>();
        double currentTreasBalance = treasRow["balance"].as<double>();
        
        // 4. Compute new balances
        double newCustBalance = currentCustBalance - amount;
        double newTreasBalance = currentTreasBalance + amount;
        
        // 5. Update cached balance in DB
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newCustBalance, customerAccountId);
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newTreasBalance, treasuryAccountId);
        
        // 6. Create Transaction Record
        std::string transactionId = drogon::utils::getUuid();
        std::string reference = generateReference("WTH");
        
        Json::Value responseJson;
        responseJson["status"] = "success";
        responseJson["message"] = "Withdrawal processed successfully";
        responseJson["reference_number"] = reference;
        responseJson["amount"] = amount;
        responseJson["balance_after"] = newCustBalance;
        responseJson["account_number"] = accountNo;
        responseJson["description"] = description;
        
        Json::Value metadataJson;
        metadataJson["response_cache"] = responseJson;
        Json::FastWriter writer;
        std::string metadataStr = writer.write(metadataJson);
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description, metadata) "
            "VALUES ($1, 'withdrawal', $2, 'completed', $3, $4, $5, $6, $7, $8)",
            transactionId,
            amount,
            idempotencyKey,
            customerAccountId,
            treasuryAccountId,
            reference,
            description,
            metadataStr
        );
        
        // 7. Post Ledger Entries
        postLedgerEntry(trans, transactionId, customerAccountId, "DEBIT", amount, newCustBalance, description);
        postLedgerEntry(trans, transactionId, treasuryAccountId, "CREDIT", amount, newTreasBalance, "System collection for withdrawal from " + accountNo);
        
        trans->execSqlSync("COMMIT");
        return responseJson;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Withdrawal failed & rolled back: " << e.what();
        throw;
    }
}

Json::Value LedgerService::processTransfer(
    const std::string& senderAccountNo,
    const std::string& receiverAccountNo,
    double amount,
    const std::string& idempotencyKey,
    const std::string& description
) {
    if (amount <= 0.0) {
        throw std::invalid_argument("Transfer amount must be greater than zero.");
    }
    if (senderAccountNo == receiverAccountNo) {
        throw std::invalid_argument("Sender and receiver accounts must be different.");
    }
    
    // Check if transaction has already run (idempotency safety)
    auto cached = checkIdempotency(idempotencyKey);
    if (cached.has_value()) {
        if (cached.value().isMember("response_cache")) {
            return cached.value()["response_cache"];
        }
        return cached.value();
    }
    
    auto db = drogon::app().getDbClient();
    auto trans = db->newTransaction();
    
    try {
        // 1. Sort accounts to lock in consistent order (deadlock prevention!)
        std::string firstAcc = senderAccountNo;
        std::string secondAcc = receiverAccountNo;
        bool senderFirst = true;
        
        if (senderAccountNo > receiverAccountNo) {
            firstAcc = receiverAccountNo;
            secondAcc = senderAccountNo;
            senderFirst = false;
        }
        
        // Acquire locks in sorted order
        auto firstResult = trans->execSqlSync(
            "SELECT id, balance, status FROM accounts WHERE account_number = $1 FOR UPDATE",
            firstAcc
        );
        auto secondResult = trans->execSqlSync(
            "SELECT id, balance, status FROM accounts WHERE account_number = $1 FOR UPDATE",
            secondAcc
        );
        
        if (firstResult.empty() || secondResult.empty()) {
            throw std::runtime_error("One or both accounts were not found.");
        }
        
        auto senderRow = senderFirst ? firstResult[0] : secondResult[0];
        auto receiverRow = senderFirst ? secondResult[0] : firstResult[0];
        
        if (senderRow["status"].as<std::string>() != "active" || receiverRow["status"].as<std::string>() != "active") {
            throw std::runtime_error("Transfer failed: One or both accounts are inactive or frozen.");
        }
        
        std::string senderId = senderRow["id"].as<std::string>();
        std::string receiverId = receiverRow["id"].as<std::string>();
        double currentSenderBalance = senderRow["balance"].as<double>();
        double currentReceiverBalance = receiverRow["balance"].as<double>();
        
        // 2. Validate Sender Balance
        if (currentSenderBalance < amount) {
            throw std::runtime_error("Transfer failed: Insufficient funds.");
        }
        
        // 3. Compute new balances
        double newSenderBalance = currentSenderBalance - amount;
        double newReceiverBalance = currentReceiverBalance + amount;
        
        // 4. Update cached balance in DB
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newSenderBalance, senderId);
        trans->execSqlSync("UPDATE accounts SET balance = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2", newReceiverBalance, receiverId);
        
        // 5. Create Transaction Record
        std::string transactionId = drogon::utils::getUuid();
        std::string reference = generateReference("TXF");
        
        Json::Value responseJson;
        responseJson["status"] = "success";
        responseJson["message"] = "Transfer processed successfully";
        responseJson["reference_number"] = reference;
        responseJson["amount"] = amount;
        responseJson["sender_balance_after"] = newSenderBalance;
        responseJson["sender_account_number"] = senderAccountNo;
        responseJson["receiver_account_number"] = receiverAccountNo;
        responseJson["description"] = description;
        
        Json::Value metadataJson;
        metadataJson["response_cache"] = responseJson;
        Json::FastWriter writer;
        std::string metadataStr = writer.write(metadataJson);
        
        trans->execSqlSync(
            "INSERT INTO transactions (id, type, amount, status, idempotency_key, sender_account_id, receiver_account_id, reference_number, description, metadata) "
            "VALUES ($1, 'transfer', $2, 'completed', $3, $4, $5, $6, $7, $8)",
            transactionId,
            amount,
            idempotencyKey,
            senderId,
            receiverId,
            reference,
            description,
            metadataStr
        );
        
        // 6. Post Ledger Entries (Double-Entry Bookkeeping)
        postLedgerEntry(trans, transactionId, senderId, "DEBIT", amount, newSenderBalance, "Transfer to " + receiverAccountNo + ": " + description);
        postLedgerEntry(trans, transactionId, receiverId, "CREDIT", amount, newReceiverBalance, "Transfer from " + senderAccountNo + ": " + description);
        
        // 7. Auto-deduct overdue repayments from receiver if they exist
        double autoDeducted = LoanEngine::checkAndAutoDeductOverdue(trans, receiverId, amount, transactionId);
        if (autoDeducted > 0.0) {
            responseJson["receiver_auto_repayment_deducted"] = autoDeducted;
            responseJson["receiver_balance_after"] = newReceiverBalance - autoDeducted;
            
            metadataJson["response_cache"] = responseJson;
            trans->execSqlSync("UPDATE transactions SET metadata = $1 WHERE id = $2", writer.write(metadataJson), transactionId);
        }
        
        trans->execSqlSync("COMMIT");
        return responseJson;
        
    } catch (const std::exception& e) {
        trans->rollback();
        LOG_ERROR << "Transfer failed & rolled back: " << e.what();
        throw;
    }
}

double LedgerService::reconcileBalance(const std::string& accountId) {
    auto db = drogon::app().getDbClient();
    if (!db) return 0.0;
    
    try {
        // Summarize all ledger entries for the account
        // Credits are additions, Debits are subtractions.
        auto result = db->execSqlSync(
            "SELECT COALESCE(SUM(CASE WHEN type = 'CREDIT' THEN amount ELSE -amount END), 0) as derived_balance "
            "FROM ledger_entries WHERE account_id = $1",
            accountId
        );
        
        if (result.empty()) return 0.0;
        return result[0]["derived_balance"].as<double>();
    } catch (const std::exception& e) {
        LOG_ERROR << "Balance reconciliation failed for account " << accountId << ": " << e.what();
        throw;
    }
}

} // namespace banking::services
