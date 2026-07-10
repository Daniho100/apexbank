#include <gtest/gtest.h>
#include "services/LedgerService.h"
#include <drogon/drogon.h>
#include <random>

using namespace banking::services;

class IdempotencyTest : public ::testing::Test {
protected:
    std::string testUserId;
    std::string accountNo;
    std::string accountId;

    void SetUp() override {
        auto db = drogon::app().getDbClient();
        ASSERT_NE(db, nullptr);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000000, 999999999);
        accountNo = "1" + std::to_string(dis(gen));

        testUserId = drogon::utils::getUuid();
        accountId = drogon::utils::getUuid();

        db->execSqlSync(
            "INSERT INTO users (id, email, password_hash, salt, role) "
            "VALUES ($1, $2, 'hash', 'salt', 'customer')",
            testUserId, "test_user_idem_" + accountNo + "@test.com"
        );

        db->execSqlSync(
            "INSERT INTO accounts (id, user_id, account_number, type, balance) "
            "VALUES ($1, $2, $3, 'savings', 0.0)",
            accountId, testUserId, accountNo
        );
    }

    void TearDown() override {
        auto db = drogon::app().getDbClient();
        if (db) {
            db->execSqlSync("DELETE FROM ledger_entries WHERE account_id = $1", accountId);
            db->execSqlSync("DELETE FROM transactions WHERE sender_account_id = $1 OR receiver_account_id = $1", accountId);
            db->execSqlSync("DELETE FROM accounts WHERE id = $1", accountId);
            db->execSqlSync("DELETE FROM users WHERE id = $1", testUserId);
        }
    }
};

TEST_F(IdempotencyTest, EnforceIdempotencyKeys) {
    auto db = drogon::app().getDbClient();
    std::string idempotencyKey = drogon::utils::getUuid();

    // 1. Submit deposit first time
    auto firstResult = LedgerService::processDeposit(accountNo, 10000.0, idempotencyKey, "First Try");
    EXPECT_EQ(firstResult["status"].asString(), "success");
    std::string firstRef = firstResult["reference_number"].asString();

    // Verify balance is 10,000 NGN
    auto balanceResult1 = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountId);
    EXPECT_DOUBLE_EQ(balanceResult1[0]["balance"].as<double>(), 10000.0);

    // 2. Submit duplicate deposit with same key
    auto secondResult = LedgerService::processDeposit(accountNo, 10000.0, idempotencyKey, "Second Try");
    EXPECT_EQ(secondResult["status"].asString(), "success");
    std::string secondRef = secondResult["reference_number"].asString();

    // Ref numbers should be identical (pulled from cache)
    EXPECT_EQ(firstRef, secondRef);

    // Balance should STILL be 10,000 NGN (no second credit executed!)
    auto balanceResult2 = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountId);
    EXPECT_DOUBLE_EQ(balanceResult2[0]["balance"].as<double>(), 10000.0);
}
