#include <gtest/gtest.h>
#include "services/LedgerService.h"
#include "services/LoanEngine.h"
#include <drogon/drogon.h>
#include <random>

using namespace banking::services;

class LoanRuleTest : public ::testing::Test {
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
            testUserId, "test_user_loan_" + accountNo + "@test.com"
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
            // Clean up dependencies
            db->execSqlSync("DELETE FROM ledger_entries WHERE account_id = $1 OR account_id IN (SELECT id FROM accounts WHERE user_id = $2)", accountId, testUserId);
            db->execSqlSync("DELETE FROM transactions WHERE sender_account_id = $1 OR receiver_account_id = $1 OR sender_account_id IN (SELECT id FROM accounts WHERE user_id = $2) OR receiver_account_id IN (SELECT id FROM accounts WHERE user_id = $2)", accountId, testUserId);
            db->execSqlSync("DELETE FROM loan_schedules WHERE loan_id IN (SELECT id FROM loans WHERE user_id = $1)", testUserId);
            db->execSqlSync("DELETE FROM loans WHERE user_id = $1", testUserId);
            db->execSqlSync("DELETE FROM accounts WHERE user_id = $1", testUserId);
            db->execSqlSync("DELETE FROM users WHERE id = $1", testUserId);
        }
    }
};

TEST_F(LoanRuleTest, AutoDeductOverdueOnIncomingDeposit) {
    auto db = drogon::app().getDbClient();

    // 1. User applies for a loan of 60,000 NGN over 12 months at 10% interest rate
    auto applyResult = LoanEngine::applyForLoan(testUserId, 60000.0, 12, 10.0);
    EXPECT_EQ(applyResult["status"].asString(), "pending");
    std::string loanId = applyResult["loan_id"].asString();
    double monthlyRepayment = applyResult["monthly_repayment"].asDouble();

    // 2. Admin approves and disburses the loan
    // This credits the user account with 60,000 NGN principal and creates 12 monthly schedules
    auto disburseResult = LoanEngine::approveAndDisburse(loanId, "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(disburseResult["status"].asString(), "success");

    // Verify balance is now 60,000 NGN
    auto balanceRes1 = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountId);
    EXPECT_DOUBLE_EQ(balanceRes1[0]["balance"].as<double>(), 60000.0);

    // 3. User spends all the disbursed funds (withdraws 60,000 NGN)
    // This reduces user savings balance to 0.0
    std::string wthKey = drogon::utils::getUuid();
    auto wthResult = LedgerService::processWithdrawal(accountNo, 60000.0, wthKey, "ATM cashout");
    EXPECT_EQ(wthResult["status"].asString(), "success");

    auto balanceRes2 = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountId);
    EXPECT_DOUBLE_EQ(balanceRes2[0]["balance"].as<double>(), 0.0);

    // 4. Manually force the first installment to be OVERDUE
    // Change its due_date to a past timestamp and status to 'overdue'
    db->execSqlSync(
        "UPDATE loan_schedules SET due_date = CURRENT_TIMESTAMP - INTERVAL '2 days', status = 'overdue' "
        "WHERE loan_id = $1 AND installment_number = 1",
        loanId
    );

    // 5. User receives a deposit of 20,000 NGN (e.g. payroll)
    // This incoming deposit must automatically trigger the overdue loan deduction!
    std::string depKey = drogon::utils::getUuid();
    auto depResult = LedgerService::processDeposit(accountNo, 20000.0, depKey, "Payroll Deposit");
    EXPECT_EQ(depResult["status"].asString(), "success");

    // Balance should be: deposit amount (20,000) - monthlyRepayment, because balance was 0.0 initially
    double expectedBalance = 20000.0 - monthlyRepayment;
    
    auto balanceRes3 = db->execSqlSync("SELECT balance FROM accounts WHERE id = $1", accountId);
    EXPECT_NEAR(balanceRes3[0]["balance"].as<double>(), expectedBalance, 0.01);

    // Verify that the schedule's status changed from 'overdue' to 'paid'
    auto schedResult = db->execSqlSync(
        "SELECT status, amount_paid FROM loan_schedules WHERE loan_id = $1 AND installment_number = 1",
        loanId
    );
    EXPECT_EQ(schedResult[0]["status"].as<std::string>(), "paid");
    EXPECT_NEAR(schedResult[0]["amount_paid"].as<double>(), monthlyRepayment, 0.01);
}
