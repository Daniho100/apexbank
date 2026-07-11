// Production C++ Backend REST API client and type mappings

export interface User {
  id: string;
  email: string;
  role: string;
  status: 'active' | 'frozen' | 'locked';
  created_at: string;
}

export interface Account {
  id: string;
  user_id: string;
  account_number: string;
  type: 'savings' | 'checking' | 'merchant';
  balance: number;
  status: 'active' | 'frozen' | 'closed';
  currency: string;
  created_at: string;
}

export interface Transaction {
  id: string;
  type: string;
  amount: number;
  status: 'pending' | 'completed' | 'failed' | 'reversed';
  idempotency_key?: string | null;
  sender_account_id?: string | null;
  receiver_account_id?: string | null;
  reference_number?: string | null;
  description?: string | null;
  created_at: string;
}

export interface LedgerEntry {
  id: string;
  transaction_id: string;
  account_id: string;
  type: 'DEBIT' | 'CREDIT';
  amount: number;
  balance_after: number;
  description: string;
  created_at: string;
}

export interface Loan {
  id: string;
  user_id: string;
  amount: number;
  interest_rate: number;
  duration_months: number;
  monthly_repayment: number;
  outstanding_balance: number;
  status: 'pending' | 'approved' | 'active' | 'completed' | 'defaulted';
  name?: string;
  reference_number?: string;
  created_at: string;
}

export interface LoanSchedule {
  id: string;
  loan_id: string;
  installment_number: number;
  amount_due: number;
  principal_due: number;
  interest_due: number;
  amount_paid: number;
  due_date: string;
  status: 'unpaid' | 'paid' | 'overdue';
}

export interface FixedDeposit {
  id: string;
  user_id: string;
  amount: number;
  duration_months: number;
  interest_rate: number;
  status: 'active' | 'matured' | 'early_withdrawn';
  start_date: string;
  maturity_date: string;
  certificate_number: string;
}

export interface ElectricityToken {
  token: string;
  meter_number: string;
  units: number;
  amount: number;
  status: 'unused' | 'used';
  generated_at: string;
}

export interface MerchantAccount {
  id: string;
  user_id: string;
  business_name: string;
  webhook_url: string;
  api_key: string;
}

export interface MerchantInvoice {
  id: string;
  merchant_id: string;
  customer_account_number: string;
  amount: number;
  description: string;
  status: 'pending' | 'paid';
  digital_signature: string;
  created_at: string;
}

export interface AuditLog {
  id: string;
  user_id: string;
  user_email?: string;
  action: string;
  description: string;
  ip_address: string;
  created_at: string;
}

export interface Notification {
  id: string;
  user_id: string;
  title: string;
  content: string;
  is_read: boolean;
  created_at: string;
}

// -------------------------------------------------------------------------
// Storage Helper Functions (Maintains local cache for autocomplete directories)
// -------------------------------------------------------------------------

export function getStorage<T>(key: string, defaultValue: T): T {
  const item = localStorage.getItem(key);
  return item ? JSON.parse(item) : defaultValue;
}

export function setStorage<T>(key: string, value: T): void {
  localStorage.setItem(key, JSON.stringify(value));
}

const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080';

async function apiRequest(path: string, options: RequestInit = {}): Promise<any> {
  const token = sessionStorage.getItem('auth_token');
  const headers = new Headers(options.headers || {});
  if (token) {
    headers.set('Authorization', `Bearer ${token}`);
  }
  if (!headers.has('Content-Type') && !(options.body instanceof FormData)) {
    headers.set('Content-Type', 'application/json');
  }
  const response = await fetch(`${API_URL}${path}`, {
    ...options,
    headers
  });
  if (!response.ok) {
    const errorData = await response.json().catch(() => ({}));
    throw new Error(errorData.message || errorData.error || `HTTP error! status: ${response.status}`);
  }
  return response.json().catch(() => null);
}

// -------------------------------------------------------------------------
// REST Client API Mappings
// -------------------------------------------------------------------------

export async function processDeposit(accountNo: string, amount: number, idempotencyKey: string, desc = 'Cash Deposit') {
  return apiRequest('/api/accounts/deposit', {
    method: 'POST',
    headers: { 'Idempotency-Key': idempotencyKey },
    body: JSON.stringify({ account_number: accountNo, amount, description: desc })
  });
}

export async function processWithdrawal(accountNo: string, amount: number, idempotencyKey: string, desc = 'ATM Withdrawal') {
  return apiRequest('/api/accounts/withdraw', {
    method: 'POST',
    headers: { 'Idempotency-Key': idempotencyKey },
    body: JSON.stringify({ account_number: accountNo, amount, description: desc })
  });
}

export async function processTransfer(senderNo: string, receiverNo: string, amount: number, idempotencyKey: string, desc = 'Transfer') {
  return apiRequest('/api/accounts/transfer', {
    method: 'POST',
    headers: { 'Idempotency-Key': idempotencyKey },
    body: JSON.stringify({ sender_account_number: senderNo, receiver_account_number: receiverNo, amount, description: desc })
  });
}

export async function applyLoan(_userId: string, amount: number, duration: number, rate = 10, name = 'Personal Loan') {
  return apiRequest('/api/loans/apply', {
    method: 'POST',
    body: JSON.stringify({ amount, duration_months: duration, interest_rate: rate, name })
  });
}

export async function approveLoan(loanId: string) {
  return apiRequest('/api/admin/loans/approve', {
    method: 'POST',
    body: JSON.stringify({ loan_id: loanId })
  });
}

export async function manualRepayLoan(loanId: string, accountNo: string, amount: number, idempotencyKey: string) {
  return apiRequest('/api/loans/repay', {
    method: 'POST',
    headers: { 'Idempotency-Key': idempotencyKey },
    body: JSON.stringify({ loan_id: loanId, account_number: accountNo, amount })
  });
}

export async function createFixedDeposit(_userId: string, amount: number, duration: number, rate = 12) {
  return apiRequest('/api/fixed-deposits/create', {
    method: 'POST',
    body: JSON.stringify({ amount, duration_months: duration, interest_rate: rate })
  });
}

export async function earlyWithdrawFixedDeposit(fdId: string, _userId: string) {
  return apiRequest('/api/fixed-deposits/liquidate', {
    method: 'POST',
    body: JSON.stringify({ fixed_deposit_id: fdId })
  });
}

export async function payUtility(accountNo: string, provider: string, recipient: string, amount: number, type: string, idempotencyKey: string) {
  return apiRequest('/api/utilities/pay', {
    method: 'POST',
    headers: { 'Idempotency-Key': idempotencyKey },
    body: JSON.stringify({ account_number: accountNo, provider, meter_number: recipient, phone_number: recipient, amount, type })
  });
}

export async function validatePrepaidToken(token: string) {
  return apiRequest('/api/utilities/validate', {
    method: 'POST',
    body: JSON.stringify({ token })
  });
}

export async function registerMerchant(_userId: string, businessName: string, webhookUrl: string) {
  return apiRequest('/api/merchants/register', {
    method: 'POST',
    body: JSON.stringify({ business_name: businessName, webhook_url: webhookUrl })
  });
}

export async function createInvoice(_merchantUserId: string, customerAcc: string, amount: number, description: string) {
  return apiRequest('/api/merchants/invoices', {
    method: 'POST',
    body: JSON.stringify({ customer_account_number: customerAcc, amount, description })
  });
}

export async function payInvoice(invoiceId: string, customerAccountNo: string, idempotencyKey: string) {
  return apiRequest('/api/merchants/invoices/pay', {
    method: 'POST',
    headers: { 'Idempotency-Key': idempotencyKey },
    body: JSON.stringify({ invoice_id: invoiceId, customer_account_number: customerAccountNo })
  });
}

export async function setUserStatus(userId: string, status: 'active' | 'frozen' | 'locked') {
  return apiRequest('/api/admin/users/status', {
    method: 'POST',
    body: JSON.stringify({ user_id: userId, status })
  });
}
