#include <gtest/gtest.h>
#include "services/LedgerService.h"
#include <drogon/drogon.h>
#include <random>

using namespace banking::services;

class LedgerReconciliationTest : public ::testing::Test {
protected:
    std::string testUserIdA;
    std::string testUserIdB;
    std::string accountNoA;
    std::string accountNoB;
    std::string accountIdA;
    std::string accountIdB;

    void SetUp() override {
        auto db = drogon::app().getDbClient();
        ASSERT_NE(db, nullptr);

        // Generate random accounts numbers
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000000, 999999999);
        accountNoA = "1" + std::to_string(dis(gen));
        accountNoB = "1" + std::to_string(dis(gen));

        testUserIdA = drogon::utils::getUuid();
        testUserIdB = drogon::utils::getUuid();

        // 1. Create Test Users
        db->execSqlSync(
            "INSERT INTO users (id, email, password_hash, salt, role) "
            "VALUES ($1, $2, 'hash', 'salt', 'customer')",
            testUserIdA, "test_user_a_" + accountNoA + "@test.com"
        );
        db->execSqlSync(
            "INSERT INTO users (id, email, password_hash, salt, role) "
            "VALUES ($1, $2, 'hash', 'salt', 'customer')",
            testUserIdB, "test_user_b_" + accountNoB + "@test.com"
        );

        // 2. Create Accounts
        accountIdA = drogon::utils::getUuid();
        accountIdB = drogon::utils::getUuid();

        db->execSqlSync(
            "INSERT INTO accounts (id, user_id, account_number, type, balance) "
            "VALUES ($1, $2, $3, 'savings', 0.0)",
            accountIdA, testUserIdA, accountNoA
        );
        db->execSqlSync(
            "INSERT INTO accounts (id, user_id, account_number, type, balance) "
            "VALUES ($1, $2, $3, 'savings', 0.0)",
            accountIdB, testUserIdB, accountNoB
        );
    }

    void TearDown() override {
        auto db = drogon::app().getDbClient();
        if (db) {
            // Delete entries to keep database clean
            db->execSqlSync(
                "DELETE FROM ledger_entries WHERE transaction_id IN ("
                "  SELECT id FROM transactions WHERE sender_account_id IN ($1, $2) OR receiver_account_id IN ($1, $2)"
                ")", accountIdA, accountIdB
            );
            db->execSqlSync("DELETE FROM transactions WHERE sender_account_id IN ($1, $2) OR receiver_account_id IN ($1, $2)", accountIdA, accountIdB);
            db->execSqlSync("DELETE FROM accounts WHERE id IN ($1, $2)", accountIdA, accountIdB);
            db->execSqlSync("DELETE FROM users WHERE id IN ($1, $2)", testUserIdA, testUserIdB);
        }
    }
};

TEST_F(LedgerReconciliationTest, ReconcileDepositWithdrawTransfer) {
    auto db = drogon::app().getDbClient();
    
    // 1. Process cash deposit of 50,000 NGN into Account A
    std::string key1 = drogon::utils::getUuid();
    auto result1 = LedgerService::processDeposit(accountNoA, 50000.0, key1, "Cash Deposit A");
    EXPECT_EQ(result1["status"].asString(), "success");
    EXPECT_DOUBLE_EQ(result1["balance_after"].asDouble(), 50000.0);
    
    // Verify cached balance in DB
    auto accResult1 = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountIdA);
    EXPECT_DOUBLE_EQ(accResult1[0]["balance"].as<double>(), 50000.0);

    // 2. Process internal transfer of 20,000 NGN from Account A to Account B
    std::string key2 = drogon::utils::getUuid();
    auto result2 = LedgerService::processTransfer(accountNoA, accountNoB, 20000.0, key2, "Birthday Gift");
    EXPECT_EQ(result2["status"].asString(), "success");
    EXPECT_DOUBLE_EQ(result2["sender_balance_after"].asDouble(), 30000.0);
    
    // Verify cached balances
    auto accResultA = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountIdA);
    auto accResultB = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountIdB);
    EXPECT_DOUBLE_EQ(accResultA[0]["balance"].as<double>(), 30000.0);
    EXPECT_DOUBLE_EQ(accResultB[0]["balance"].as<double>(), 20000.0);

    // 3. Process withdrawal of 5,000 NGN from Account B
    std::string key3 = drogon::utils::getUuid();
    auto result3 = LedgerService::processWithdrawal(accountNoB, 5000.0, key3, "ATM Withdrawal");
    EXPECT_EQ(result3["status"].asString(), "success");
    EXPECT_DOUBLE_EQ(result3["balance_after"].asDouble(), 15000.0);

    // Verify cached balance B
    auto accResultB_after = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountIdB);
    EXPECT_DOUBLE_EQ(accResultB_after[0]["balance"].as<double>(), 15000.0);

    // 4. Run Balance Reconciliation directly from Ledger Entries (Source of truth audit)
    double reconBalanceA = LedgerService::reconcileBalance(accountIdA);
    double reconBalanceB = LedgerService::reconcileBalance(accountIdB);

    EXPECT_DOUBLE_EQ(reconBalanceA, 30000.0);
    EXPECT_DOUBLE_EQ(reconBalanceB, 15000.0);
}
