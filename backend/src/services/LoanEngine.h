#pragma once
#include <string>
#include <vector>
#include <drogon/drogon.h>
#include <json/json.h>

namespace banking::services {

struct Installment {
    int number;
    double principal;
    double interest;
    double total;
    std::string dueDate;
};

class LoanEngine {
public:
    // Calculate Equated Monthly Installment (EMI) using amortization formula
    static double calculateEMI(double principal, double annualRate, int durationMonths);

    // Generate monthly payment schedule details
    static std::vector<Installment> generateSchedule(double principal, double annualRate, int durationMonths);

    // Check if user is eligible for a new loan (no defaulted loans, no overdue schedules)
    static bool checkEligibility(const std::string& userId, std::string& outReason);

    // Process a loan application
    static Json::Value applyForLoan(const std::string& userId, double amount, int durationMonths, double annualRate);

    // Approve and disburse a loan (admin action)
    static Json::Value approveAndDisburse(const std::string& loanId, const std::string& adminId);

    // Automatically check and deduct outstanding overdue repayments from customer account
    // This is run inside transactions when customer receives deposits or transfers
    static double checkAndAutoDeductOverdue(
        const std::shared_ptr<drogon::orm::Transaction>& trans,
        const std::string& accountId,
        double incomingAmount,
        const std::string& correlationId
    );

    // Process manual loan repayment
    static Json::Value processRepayment(const std::string& loanId, const std::string& accountNo, double amount, const std::string& idempotencyKey);
};

} // namespace banking::services
