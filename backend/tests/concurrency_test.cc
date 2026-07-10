#include <gtest/gtest.h>
#include "services/LedgerService.h"
#include <drogon/drogon.h>
#include <random>
#include <thread>
#include <vector>
#include <atomic>

using namespace banking::services;

class ConcurrencyTest : public ::testing::Test {
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

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000000, 999999999);
        accountNoA = "1" + std::to_string(dis(gen));
        accountNoB = "1" + std::to_string(dis(gen));

        testUserIdA = drogon::utils::getUuid();
        testUserIdB = drogon::utils::getUuid();

        db->execSqlSync("INSERT INTO users (id, email, password_hash, salt, role) VALUES ($1, $2, 'h', 's', 'customer')", testUserIdA, "test_user_a_" + accountNoA + "@test.com");
        db->execSqlSync("INSERT INTO users (id, email, password_hash, salt, role) VALUES ($1, $2, 'h', 's', 'customer')", testUserIdB, "test_user_b_" + accountNoB + "@test.com");

        accountIdA = drogon::utils::getUuid();
        accountIdB = drogon::utils::getUuid();

        db->execSqlSync("INSERT INTO accounts (id, user_id, account_number, type, balance) VALUES ($1, $2, $3, 'savings', 0.0)", accountIdA, testUserIdA, accountNoA);
        db->execSqlSync("INSERT INTO accounts (id, user_id, account_number, type, balance) VALUES ($1, $2, $3, 'savings', 0.0)", accountIdB, testUserIdB, accountNoB);
    }

    void TearDown() override {
        auto db = drogon::app().getDbClient();
        if (db) {
            db->execSqlSync("DELETE FROM ledger_entries WHERE account_id IN ($1, $2)", accountIdA, accountIdB);
            db->execSqlSync("DELETE FROM transactions WHERE sender_account_id IN ($1, $2) OR receiver_account_id IN ($1, $2)", accountIdA, accountIdB);
            db->execSqlSync("DELETE FROM accounts WHERE id IN ($1, $2)", accountIdA, accountIdB);
            db->execSqlSync("DELETE FROM users WHERE id IN ($1, $2)", testUserIdA, testUserIdB);
        }
    }
};

TEST_F(ConcurrencyTest, ConcurrentInternalTransfers) {
    auto db = drogon::app().getDbClient();

    // 1. Give Account A 5,000 NGN balance
    std::string key1 = drogon::utils::getUuid();
    LedgerService::processDeposit(accountNoA, 5000.0, key1, "Setup balance");

    // 2. Launch 6 threads each attempting to transfer 1,000 NGN from A to B simultaneously
    // Total A balance is 5,000 NGN, so exactly 5 transfers must succeed and 1 must fail!
    const int numThreads = 6;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &successCount, &failureCount]() {
            std::string key = drogon::utils::getUuid();
            try {
                auto res = LedgerService::processTransfer(accountNoA, accountNoB, 1000.0, key, "Concurrent send");
                if (res["status"].asString() == "success") {
                    successCount++;
                } else {
                    failureCount++;
                }
            } catch (...) {
                failureCount++;
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Assert that exactly 5 transfers succeeded and 1 failed due to overdraft protection
    EXPECT_EQ(successCount.load(), 5);
    EXPECT_EQ(failureCount.load(), 1);

    // Verify balances match expected values exactly
    auto balanceResA = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountIdA);
    auto balanceResB = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountIdB);
    EXPECT_DOUBLE_EQ(balanceResA[0]["balance"].as<double>(), 0.0);
    EXPECT_DOUBLE_EQ(balanceResB[0]["balance"].as<double>(), 5000.0);
}
