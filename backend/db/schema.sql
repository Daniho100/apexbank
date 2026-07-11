-- PostgreSQL Schema for Banking Application MVP
-- Setup UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- 1. Users Table
CREATE TABLE IF NOT EXISTS users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    salt VARCHAR(255) NOT NULL,
    role VARCHAR(50) NOT NULL DEFAULT 'customer', -- 'customer', 'support', 'loan_officer', 'auditor', 'administrator', 'super_administrator', 'merchant', 'api_client'
    two_factor_secret VARCHAR(255),
    email_verified BOOLEAN DEFAULT FALSE,
    status VARCHAR(50) DEFAULT 'active', -- 'active', 'frozen', 'locked'
    failed_login_attempts INT DEFAULT 0,
    locked_until TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);

-- 2. Sessions Table
CREATE TABLE IF NOT EXISTS sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE CASCADE,
    refresh_token VARCHAR(255) UNIQUE NOT NULL,
    token_expires_at TIMESTAMP NOT NULL,
    device_fingerprint VARCHAR(255),
    ip_address VARCHAR(45),
    last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    revoked BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_sessions_refresh_token ON sessions(refresh_token);
CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);

-- 3. Accounts Table
CREATE TABLE IF NOT EXISTS accounts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE RESTRICT,
    account_number VARCHAR(20) UNIQUE NOT NULL,
    type VARCHAR(50) NOT NULL DEFAULT 'savings', -- 'savings', 'checking', 'merchant'
    balance NUMERIC(18, 4) NOT NULL DEFAULT 0.0000, -- cached balance, ledger is source of truth
    status VARCHAR(50) DEFAULT 'active', -- 'active', 'frozen', 'closed'
    currency VARCHAR(3) DEFAULT 'NGN',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_accounts_number ON accounts(account_number);
CREATE INDEX IF NOT EXISTS idx_accounts_user_id ON accounts(user_id);

-- 4. Transactions Table
CREATE TABLE IF NOT EXISTS transactions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type VARCHAR(50) NOT NULL, -- 'deposit', 'withdrawal', 'transfer', 'loan_disbursement', 'loan_repayment', 'fixed_deposit_creation', 'fixed_deposit_maturity', 'bill_payment', 'merchant_payment', 'reversal', 'adjustment'
    amount NUMERIC(18, 4) NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'pending', -- 'pending', 'completed', 'failed', 'reversed'
    idempotency_key VARCHAR(255) UNIQUE NOT NULL,
    sender_account_id UUID REFERENCES accounts(id) ON DELETE RESTRICT,
    receiver_account_id UUID REFERENCES accounts(id) ON DELETE RESTRICT,
    reference_number VARCHAR(100) UNIQUE NOT NULL,
    description TEXT,
    metadata JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_transactions_idempotency ON transactions(idempotency_key);
CREATE INDEX IF NOT EXISTS idx_transactions_reference ON transactions(reference_number);
CREATE INDEX IF NOT EXISTS idx_transactions_created_at ON transactions(created_at);

-- 5. Ledger Entries Table
CREATE TABLE IF NOT EXISTS ledger_entries (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    transaction_id UUID REFERENCES transactions(id) ON DELETE RESTRICT,
    account_id UUID REFERENCES accounts(id) ON DELETE RESTRICT,
    type VARCHAR(10) NOT NULL, -- 'DEBIT', 'CREDIT'
    amount NUMERIC(18, 4) NOT NULL, -- Always positive
    balance_after NUMERIC(18, 4) NOT NULL,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_ledger_account_created ON ledger_entries(account_id, created_at);
CREATE INDEX IF NOT EXISTS idx_ledger_transaction ON ledger_entries(transaction_id);

-- 6. Loans Table
CREATE TABLE IF NOT EXISTS loans (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE RESTRICT,
    name VARCHAR(255) NOT NULL DEFAULT 'Personal Loan',
    reference_number VARCHAR(100) UNIQUE NOT NULL,
    amount NUMERIC(18, 4) NOT NULL,
    interest_rate NUMERIC(5, 2) NOT NULL, -- e.g. 10.00%
    duration_months INT NOT NULL,
    monthly_repayment NUMERIC(18, 4) NOT NULL,
    outstanding_balance NUMERIC(18, 4) NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'pending', -- 'pending', 'approved', 'rejected', 'disbursed', 'active', 'completed', 'defaulted', 'written_off', 'closed'
    approved_at TIMESTAMP,
    disbursed_at TIMESTAMP,
    due_date TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_loans_user ON loans(user_id);
CREATE INDEX IF NOT EXISTS idx_loans_status ON loans(status);
CREATE UNIQUE INDEX IF NOT EXISTS idx_loans_reference ON loans(reference_number);

-- 7. Loan Schedules Table
CREATE TABLE IF NOT EXISTS loan_schedules (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    loan_id UUID REFERENCES loans(id) ON DELETE CASCADE,
    installment_number INT NOT NULL,
    amount_due NUMERIC(18, 4) NOT NULL,
    principal_due NUMERIC(18, 4) NOT NULL,
    interest_due NUMERIC(18, 4) NOT NULL,
    amount_paid NUMERIC(18, 4) DEFAULT 0.0000,
    due_date TIMESTAMP NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'unpaid', -- 'unpaid', 'partial', 'paid', 'overdue'
    paid_at TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_schedules_loan ON loan_schedules(loan_id);
CREATE INDEX IF NOT EXISTS idx_schedules_due_date_status ON loan_schedules(due_date, status);

-- 8. Fixed Deposits Table
CREATE TABLE IF NOT EXISTS fixed_deposits (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE RESTRICT,
    amount NUMERIC(18, 4) NOT NULL,
    duration_months INT NOT NULL,
    interest_rate NUMERIC(5, 2) NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'active', -- 'active', 'matured', 'early_withdrawn'
    start_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    maturity_date TIMESTAMP NOT NULL,
    certificate_number VARCHAR(100) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_fixed_user ON fixed_deposits(user_id);

-- 9. Electricity Tokens Table
CREATE TABLE IF NOT EXISTS electricity_tokens (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    token VARCHAR(20) UNIQUE NOT NULL, -- 20-digit unique token string
    meter_number VARCHAR(50) NOT NULL,
    units NUMERIC(10, 2) NOT NULL,
    amount NUMERIC(18, 4) NOT NULL,
    status VARCHAR(20) NOT NULL DEFAULT 'unused', -- 'unused', 'used', 'expired'
    generated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    used_at TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_electricity_token ON electricity_tokens(token);

-- 10. Merchant Accounts Table
CREATE TABLE IF NOT EXISTS merchant_accounts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE CASCADE,
    business_name VARCHAR(255) NOT NULL,
    webhook_url VARCHAR(255),
    api_key_hash VARCHAR(255),
    api_key_salt VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_merchant_user ON merchant_accounts(user_id);

-- 11. Merchant Invoices Table
CREATE TABLE IF NOT EXISTS merchant_invoices (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    merchant_id UUID REFERENCES merchant_accounts(id) ON DELETE CASCADE,
    customer_account_number VARCHAR(20),
    amount NUMERIC(18, 4) NOT NULL,
    description TEXT,
    status VARCHAR(50) DEFAULT 'pending', -- 'pending', 'paid', 'expired'
    digital_signature VARCHAR(512),
    webhook_sent BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_invoices_merchant ON merchant_invoices(merchant_id);

-- 12. Audit Logs Table
CREATE TABLE IF NOT EXISTS audit_logs (
    id BIGSERIAL PRIMARY KEY,
    user_id UUID REFERENCES users(id) ON DELETE SET NULL,
    action VARCHAR(100) NOT NULL,
    description TEXT,
    ip_address VARCHAR(45),
    device_fingerprint VARCHAR(255),
    old_values JSONB,
    new_values JSONB,
    correlation_id UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_audit_logs_user_created ON audit_logs(user_id, created_at);
CREATE INDEX IF NOT EXISTS idx_audit_logs_correlation ON audit_logs(correlation_id);

-- 13. Notifications Table
CREATE TABLE IF NOT EXISTS notifications (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id) ON DELETE CASCADE,
    title VARCHAR(255) NOT NULL,
    content TEXT NOT NULL,
    type VARCHAR(50) NOT NULL, -- 'in_app', 'email', 'websocket'
    is_read BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_notifications_user ON notifications(user_id, is_read);

-- 14. Seed System Treasury User and Account
INSERT INTO users (id, email, password_hash, salt, role, status)
VALUES ('00000000-0000-0000-0000-000000000000', 'system_treasury@banking.com', 'SYSTEM_LOCKED', 'SYSTEM_LOCKED', 'administrator', 'active')
ON CONFLICT (id) DO NOTHING;

INSERT INTO accounts (id, user_id, account_number, type, balance, status, currency)
VALUES ('00000000-0000-0000-0000-000000000001', '00000000-0000-0000-0000-000000000000', 'SYSTEM_TREASURY_001', 'checking', 1000000000.0000, 'active', 'NGN')
ON CONFLICT (account_number) DO NOTHING;
