// Production-quality client-side Banking Engine MVP
// Implements complete banking rules, double-entry ledger, loan schedules, fixed deposits, utility payments, and auditing.
// Persists state in localStorage.

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
  balance: number; // cached balance, ledger is source of truth
  status: 'active' | 'frozen' | 'closed';
  currency: string;
  created_at: string;
}

export interface Transaction {
  id: string;
  type: string;
  amount: number;
  status: 'pending' | 'completed' | 'failed' | 'reversed';
  idempotency_key: string;
  sender_account_id: string;
  receiver_account_id: string;
  reference_number: string;
  description: string;
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
// Engine Helper Functions
// -------------------------------------------------------------------------

const SYS_USER_ID = '00000000-0000-0000-0000-000000000000';
const TREASURY_ACC_ID = '00000000-0000-0000-0000-000000000001';

export function getStorage<T>(key: string, defaultValue: T): T {
  const item = localStorage.getItem(key);
  return item ? JSON.parse(item) : defaultValue;
}

export function setStorage<T>(key: string, value: T): void {
  localStorage.setItem(key, JSON.stringify(value));
}

// Initialize Database State
export function initializeDb() {
  const users = getStorage<User[]>('users', []);
  const accounts = getStorage<Account[]>('accounts', []);

  // Seed Treasury if missing
  const hasTreasuryUser = users.some(u => u.id === SYS_USER_ID);
  if (!hasTreasuryUser) {
    users.push({
      id: SYS_USER_ID,
      email: 'system_treasury@banking.com',
      role: 'administrator',
      status: 'active',
      created_at: new Date().toISOString()
    });
    setStorage('users', users);
  }

  const hasTreasuryAcc = accounts.some(a => a.id === TREASURY_ACC_ID);
  if (!hasTreasuryAcc) {
    accounts.push({
      id: TREASURY_ACC_ID,
      user_id: SYS_USER_ID,
      account_number: 'SYSTEM_TREASURY_001',
      type: 'checking',
      balance: 1000000000.0,
      status: 'active',
      currency: 'NGN',
      created_at: new Date().toISOString()
    });
    setStorage('accounts', accounts);
  }
}

// Initialize
initializeDb();

// Generate Random Reference Numbers
function makeRef(prefix: string): string {
  const dateStr = new Date().toISOString().slice(0, 10).replace(/-/g, '');
  const rand = Math.floor(100000 + Math.random() * 900000);
  return `${prefix}_${dateStr}_${rand}`;
}

// Log Audit Trail
function addAudit(userId: string, action: string, description: string) {
  const logs = getStorage<AuditLog[]>('audit_logs', []);
  logs.unshift({
    id: crypto.randomUUID(),
    user_id: userId,
    action,
    description,
    ip_address: '127.0.0.1',
    created_at: new Date().toISOString()
  });
  setStorage('audit_logs', logs);
}

// Trigger Notifications
export function addNotification(userId: string, title: string, content: string) {
  const notifications = getStorage<Notification[]>('notifications', []);
  notifications.unshift({
    id: crypto.randomUUID(),
    user_id: userId,
    title,
    content,
    is_read: false,
    created_at: new Date().toISOString()
  });
  setStorage('notifications', notifications);
}

// Post Ledger Entries
function addLedgerEntry(txnId: string, accountId: string, type: 'DEBIT' | 'CREDIT', amount: number, balanceAfter: number, desc: string) {
  const entries = getStorage<LedgerEntry[]>('ledger_entries', []);
  entries.unshift({
    id: crypto.randomUUID(),
    transaction_id: txnId,
    account_id: accountId,
    type,
    amount,
    balance_after: balanceAfter,
    description: desc,
    created_at: new Date().toISOString()
  });
  setStorage('ledger_entries', entries);
}

// -------------------------------------------------------------------------
// Core Banking Business Operations
// -------------------------------------------------------------------------

// Check and Auto-Deduct Overdue Loan schedules from incoming amounts
function checkAndAutoDeduct(accountId: string, incomingAmount: number): number {
  const accounts = getStorage<Account[]>('accounts', []);
  const acc = accounts.find(a => a.id === accountId);
  if (!acc) return 0;

  const loans = getStorage<Loan[]>('loans', []);
  const schedules = getStorage<LoanSchedule[]>('loan_schedules', []);
  const treasury = accounts.find(a => a.id === TREASURY_ACC_ID)!;

  // Fetch overdue installments for this user
  const userLoans = loans.filter(l => l.user_id === acc.user_id && l.status === 'active');
  const userLoanIds = userLoans.map(l => l.id);
  const now = new Date();
  
  const overdueSchedules = schedules
    .filter(s => userLoanIds.includes(s.loan_id) && s.status === 'unpaid' && new Date(s.due_date) < now)
    .sort((a, b) => new Date(a.due_date).getTime() - new Date(b.due_date).getTime());

  if (overdueSchedules.length === 0) return 0;

  let totalDeducted = 0;
  let remainingIncoming = incomingAmount;

  for (const sched of overdueSchedules) {
    if (remainingIncoming <= 0) break;
    
    const unpaid = sched.amount_due - sched.amount_paid;
    const payment = Math.min(remainingIncoming, unpaid);

    sched.amount_paid += payment;
    if (sched.amount_paid >= sched.amount_due) {
      sched.status = 'paid';
    }

    // Update loan outstanding balance
    const loan = loans.find(l => l.id === sched.loan_id)!;
    loan.outstanding_balance -= payment;
    if (loan.outstanding_balance <= 0) {
      loan.outstanding_balance = 0;
      loan.status = 'completed';
    }

    acc.balance -= payment;
    treasury.balance += payment;
    remainingIncoming -= payment;
    totalDeducted += payment;

    // Create journal entries for auto deduction
    const txnId = crypto.randomUUID();
    const ref = makeRef('LNP');
    const desc = `Auto-deduction for overdue installment #${sched.installment_number} of Loan ${loan.id}`;
    
    const txns = getStorage<Transaction[]>('transactions', []);
    txns.unshift({
      id: txnId,
      type: 'loan_repayment',
      amount: payment,
      status: 'completed',
      idempotency_key: crypto.randomUUID(),
      sender_account_id: acc.id,
      receiver_account_id: treasury.id,
      reference_number: ref,
      description: desc,
      created_at: new Date().toISOString()
    });
    setStorage('transactions', txns);

    addLedgerEntry(txnId, acc.id, 'DEBIT', payment, acc.balance, desc);
    addLedgerEntry(txnId, treasury.id, 'CREDIT', payment, treasury.balance, `Overdue auto-payment from account ${acc.account_number}`);
  }

  setStorage('accounts', accounts);
  setStorage('loans', loans);
  setStorage('loan_schedules', schedules);

  if (totalDeducted > 0) {
    addNotification(
      acc.user_id,
      'Overdue Repayment Auto-Deducted',
      `An overdue loan repayment of ${totalDeducted.toLocaleString()} NGN was automatically deducted from your incoming deposit.`
    );
  }

  return totalDeducted;
}

// Deposit
export function processDeposit(accountNo: string, amount: number, idempotencyKey: string, desc = 'Cash Deposit') {
  const txns = getStorage<Transaction[]>('transactions', []);
  const cached = txns.find(t => t.idempotency_key === idempotencyKey);
  if (cached) return cached;

  const accounts = getStorage<Account[]>('accounts', []);
  const custAcc = accounts.find(a => a.account_number === accountNo);
  if (!custAcc) throw new Error('Account not found.');
  if (custAcc.status !== 'active') throw new Error('Account is frozen or closed.');

  const treasury = accounts.find(a => a.id === TREASURY_ACC_ID)!;

  custAcc.balance += amount;
  treasury.balance -= amount;

  const txnId = crypto.randomUUID();
  const ref = makeRef('DEP');

  const newTxn: Transaction = {
    id: txnId,
    type: 'deposit',
    amount,
    status: 'completed',
    idempotency_key: idempotencyKey,
    sender_account_id: treasury.id,
    receiver_account_id: custAcc.id,
    reference_number: ref,
    description: desc,
    created_at: new Date().toISOString()
  };

  txns.unshift(newTxn);
  setStorage('transactions', txns);
  setStorage('accounts', accounts);

  addLedgerEntry(txnId, treasury.id, 'DEBIT', amount, treasury.balance, `System payout for Deposit to ${accountNo}`);
  addLedgerEntry(txnId, custAcc.id, 'CREDIT', amount, custAcc.balance, desc);

  // Check auto deduct rules
  checkAndAutoDeduct(custAcc.id, amount);

  addAudit(custAcc.user_id, 'deposit', `Deposited ${amount} NGN into account ${accountNo}`);
  addNotification(custAcc.user_id, 'Funds Deposited', `${amount.toLocaleString()} NGN has been deposited into your account.`);

  return newTxn;
}

// Withdrawal
export function processWithdrawal(accountNo: string, amount: number, idempotencyKey: string, desc = 'ATM Withdrawal') {
  const txns = getStorage<Transaction[]>('transactions', []);
  const cached = txns.find(t => t.idempotency_key === idempotencyKey);
  if (cached) return cached;

  const accounts = getStorage<Account[]>('accounts', []);
  const custAcc = accounts.find(a => a.account_number === accountNo);
  if (!custAcc) throw new Error('Account not found.');
  if (custAcc.status !== 'active') throw new Error('Account is frozen or closed.');
  if (custAcc.balance < amount) throw new Error('Insufficient funds.');

  const treasury = accounts.find(a => a.id === TREASURY_ACC_ID)!;

  custAcc.balance -= amount;
  treasury.balance += amount;

  const txnId = crypto.randomUUID();
  const ref = makeRef('WTH');

  const newTxn: Transaction = {
    id: txnId,
    type: 'withdrawal',
    amount,
    status: 'completed',
    idempotency_key: idempotencyKey,
    sender_account_id: custAcc.id,
    receiver_account_id: treasury.id,
    reference_number: ref,
    description: desc,
    created_at: new Date().toISOString()
  };

  txns.unshift(newTxn);
  setStorage('transactions', txns);
  setStorage('accounts', accounts);

  addLedgerEntry(txnId, custAcc.id, 'DEBIT', amount, custAcc.balance, desc);
  addLedgerEntry(txnId, treasury.id, 'CREDIT', amount, treasury.balance, `System collection for withdrawal from ${accountNo}`);

  addAudit(custAcc.user_id, 'withdrawal', `Withdrew ${amount} NGN from account ${accountNo}`);
  addNotification(custAcc.user_id, 'Withdrawal Executed', `${amount.toLocaleString()} NGN has been withdrawn from your account.`);

  return newTxn;
}

// Transfer
export function processTransfer(senderNo: string, receiverNo: string, amount: number, idempotencyKey: string, desc = 'Transfer') {
  const txns = getStorage<Transaction[]>('transactions', []);
  const cached = txns.find(t => t.idempotency_key === idempotencyKey);
  if (cached) return cached;

  const accounts = getStorage<Account[]>('accounts', []);
  const sender = accounts.find(a => a.account_number === senderNo);
  const receiver = accounts.find(a => a.account_number === receiverNo);

  if (!sender || !receiver) throw new Error('Sender or Receiver account not found.');
  if (sender.status !== 'active' || receiver.status !== 'active') throw new Error('One or both accounts are inactive.');
  if (sender.balance < amount) throw new Error('Insufficient funds.');

  sender.balance -= amount;
  receiver.balance += amount;

  const txnId = crypto.randomUUID();
  const ref = makeRef('TXF');

  const newTxn: Transaction = {
    id: txnId,
    type: 'transfer',
    amount,
    status: 'completed',
    idempotency_key: idempotencyKey,
    sender_account_id: sender.id,
    receiver_account_id: receiver.id,
    reference_number: ref,
    description: desc,
    created_at: new Date().toISOString()
  };

  txns.unshift(newTxn);
  setStorage('transactions', txns);
  setStorage('accounts', accounts);

  addLedgerEntry(txnId, sender.id, 'DEBIT', amount, sender.balance, `Transfer to ${receiverNo}: ${desc}`);
  addLedgerEntry(txnId, receiver.id, 'CREDIT', amount, receiver.balance, `Transfer from ${senderNo}: ${desc}`);

  // Check auto-deduct rules for receiver
  checkAndAutoDeduct(receiver.id, amount);

  addAudit(sender.user_id, 'transfer', `Transferred ${amount} NGN to account ${receiverNo}`);
  addNotification(sender.user_id, 'Transfer Sent', `Sent ${amount.toLocaleString()} NGN to ${receiverNo}`);
  addNotification(receiver.user_id, 'Transfer Received', `Received ${amount.toLocaleString()} NGN from ${senderNo}`);

  return newTxn;
}

// -------------------------------------------------------------------------
// Loans System
// -------------------------------------------------------------------------

export function applyLoan(userId: string, amount: number, duration: number, rate = 10) {
  const loans = getStorage<Loan[]>('loans', []);
  const schedules = getStorage<LoanSchedule[]>('loan_schedules', []);

  // Calculate monthly repayment
  const monthlyRate = (rate / 12) / 100;
  const emi = amount * (monthlyRate * Math.pow(1 + monthlyRate, duration)) / (Math.pow(1 + monthlyRate, duration) - 1);

  const loanId = crypto.randomUUID();

  const newLoan: Loan = {
    id: loanId,
    user_id: userId,
    amount,
    interest_rate: rate,
    duration_months: duration,
    monthly_repayment: emi,
    outstanding_balance: amount,
    status: 'pending',
    created_at: new Date().toISOString()
  };

  loans.push(newLoan);
  setStorage('loans', loans);

  addAudit(userId, 'loan_application', `Applied for loan of ${amount} NGN`);
  return newLoan;
}

export function approveLoan(loanId: string) {
  const loans = getStorage<Loan[]>('loans', []);
  const loan = loans.find(l => l.id === loanId);
  if (!loan) throw new Error('Loan not found');
  if (loan.status !== 'pending') throw new Error('Loan not in pending status');

  const accounts = getStorage<Account[]>('accounts', []);
  const userAcc = accounts.find(a => a.user_id === loan.user_id && a.type === 'savings');
  if (!userAcc) throw new Error('User savings account not found');

  const treasury = accounts.find(a => a.id === TREASURY_ACC_ID)!;

  // Disburse funds
  loan.status = 'active';
  userAcc.balance += loan.amount;
  treasury.balance -= loan.amount;

  // Generate Schedules
  const schedules = getStorage<LoanSchedule[]>('loan_schedules', []);
  for (let i = 1; i <= loan.duration_months; i++) {
    const dueDate = new Date();
    dueDate.setMonth(dueDate.getMonth() + i);

    schedules.push({
      id: crypto.randomUUID(),
      loan_id: loan.id,
      installment_number: i,
      amount_due: loan.monthly_repayment,
      principal_due: loan.amount / loan.duration_months,
      interest_due: (loan.monthly_repayment - (loan.amount / loan.duration_months)),
      amount_paid: 0,
      due_date: dueDate.toISOString(),
      status: 'unpaid'
    });
  }

  // Double entry records
  const txnId = crypto.randomUUID();
  const ref = makeRef('LND');
  const txns = getStorage<Transaction[]>('transactions', []);
  txns.unshift({
    id: txnId,
    type: 'loan_disbursement',
    amount: loan.amount,
    status: 'completed',
    idempotency_key: crypto.randomUUID(),
    sender_account_id: treasury.id,
    receiver_account_id: userAcc.id,
    reference_number: ref,
    description: 'Loan disbursement payout',
    created_at: new Date().toISOString()
  });

  setStorage('loans', loans);
  setStorage('accounts', accounts);
  setStorage('loan_schedules', schedules);
  setStorage('transactions', txns);

  addLedgerEntry(txnId, treasury.id, 'DEBIT', loan.amount, treasury.balance, `Disbursement debit for loan ${loan.id}`);
  addLedgerEntry(txnId, userAcc.id, 'CREDIT', loan.amount, userAcc.balance, 'Loan disbursement credit');

  addNotification(loan.user_id, 'Loan Disbursed', `Your loan of ${loan.amount.toLocaleString()} NGN has been approved and disbursed.`);
  addAudit(loan.user_id, 'loan_disbursement', `Disbursed loan ${loan.id} of ${loan.amount} NGN`);

  return loan;
}

export function manualRepayLoan(loanId: string, accountNo: string, amount: number, idempotencyKey: string) {
  const txns = getStorage<Transaction[]>('transactions', []);
  const cached = txns.find(t => t.idempotency_key === idempotencyKey);
  if (cached) return cached;

  const accounts = getStorage<Account[]>('accounts', []);
  const custAcc = accounts.find(a => a.account_number === accountNo);
  const loans = getStorage<Loan[]>('loans', []);
  const loan = loans.find(l => l.id === loanId);

  if (!custAcc || !loan) throw new Error('Account or Loan not found.');
  if (custAcc.balance < amount) throw new Error('Insufficient funds.');

  const payment = Math.min(amount, loan.outstanding_balance);
  const treasury = accounts.find(a => a.id === TREASURY_ACC_ID)!;

  custAcc.balance -= payment;
  treasury.balance += payment;
  loan.outstanding_balance -= payment;
  if (loan.outstanding_balance <= 0) {
    loan.status = 'completed';
  }

  // Allocate payment to schedules
  const schedules = getStorage<LoanSchedule[]>('loan_schedules', []);
  const loanScheds = schedules
    .filter(s => s.loan_id === loanId && s.status === 'unpaid')
    .sort((a, b) => new Date(a.due_date).getTime() - new Date(b.due_date).getTime());

  let remPayment = payment;
  for (const s of loanScheds) {
    if (remPayment <= 0) break;
    const unpaid = s.amount_due - s.amount_paid;
    const alloc = Math.min(remPayment, unpaid);
    s.amount_paid += alloc;
    if (s.amount_paid >= s.amount_due) {
      s.status = 'paid';
    }
    remPayment -= alloc;
  }

  const txnId = crypto.randomUUID();
  const ref = makeRef('LNP');
  const desc = `Manual repayment for loan ${loanId}`;

  const newTxn: Transaction = {
    id: txnId,
    type: 'loan_repayment',
    amount: payment,
    status: 'completed',
    idempotency_key: idempotencyKey,
    sender_account_id: custAcc.id,
    receiver_account_id: treasury.id,
    reference_number: ref,
    description: desc,
    created_at: new Date().toISOString()
  };

  txns.unshift(newTxn);
  setStorage('transactions', txns);
  setStorage('accounts', accounts);
  setStorage('loans', loans);
  setStorage('loan_schedules', schedules);

  addLedgerEntry(txnId, custAcc.id, 'DEBIT', payment, custAcc.balance, desc);
  addLedgerEntry(txnId, treasury.id, 'CREDIT', payment, treasury.balance, `Manual loan repayment from ${accountNo}`);

  addNotification(custAcc.user_id, 'Loan Repayed', `Manual loan payment of ${payment.toLocaleString()} NGN processed successfully.`);
  addAudit(custAcc.user_id, 'loan_repayment', `Repayed ${payment} NGN on loan ${loanId}`);

  return newTxn;
}

// -------------------------------------------------------------------------
// Fixed Deposits
// -------------------------------------------------------------------------

export function createFixedDeposit(userId: string, amount: number, duration: number, rate = 12) {
  const accounts = getStorage<Account[]>('accounts', []);
  const custAcc = accounts.find(a => a.user_id === userId && a.type === 'savings');
  if (!custAcc) throw new Error('Savings account not found');
  if (custAcc.balance < amount) throw new Error('Insufficient funds');

  const treasury = accounts.find(a => a.id === TREASURY_ACC_ID)!;

  custAcc.balance -= amount;
  treasury.balance += amount;

  const certNo = `FD_CERT_${duration}M_${Math.floor(100000 + Math.random() * 900000)}`;
  const fdId = crypto.randomUUID();

  const start = new Date();
  const maturity = new Date();
  maturity.setMonth(maturity.getMonth() + duration);

  const fixedDeps = getStorage<FixedDeposit[]>('fixed_deposits', []);
  const newFd: FixedDeposit = {
    id: fdId,
    user_id: userId,
    amount,
    duration_months: duration,
    interest_rate: rate,
    status: 'active',
    start_date: start.toISOString(),
    maturity_date: maturity.toISOString(),
    certificate_number: certNo
  };

  fixedDeps.push(newFd);
  setStorage('fixed_deposits', fixedDeps);
  setStorage('accounts', accounts);

  // Journal entry
  const txnId = crypto.randomUUID();
  const ref = makeRef('FDC');
  const txns = getStorage<Transaction[]>('transactions', []);
  txns.unshift({
    id: txnId,
    type: 'fixed_deposit_creation',
    amount,
    status: 'completed',
    idempotency_key: crypto.randomUUID(),
    sender_account_id: custAcc.id,
    receiver_account_id: treasury.id,
    reference_number: ref,
    description: `Fixed Deposit Certificate ${certNo}`,
    created_at: new Date().toISOString()
  });
  setStorage('transactions', txns);

  addLedgerEntry(txnId, custAcc.id, 'DEBIT', amount, custAcc.balance, `Opened Fixed Deposit: Cert ${certNo}`);
  addLedgerEntry(txnId, treasury.id, 'CREDIT', amount, treasury.balance, `Fixed deposit intake from savings`);

  addNotification(userId, 'Fixed Deposit Opened', `Your fixed deposit of ${amount.toLocaleString()} NGN has been opened. Cert: ${certNo}`);
  addAudit(userId, 'fixed_deposit_creation', `Opened fixed deposit ${certNo} of ${amount} NGN`);

  return newFd;
}

export function earlyWithdrawFixedDeposit(fdId: string, userId: string) {
  const fixedDeps = getStorage<FixedDeposit[]>('fixed_deposits', []);
  const fd = fixedDeps.find(f => f.id === fdId && f.user_id === userId);
  if (!fd) throw new Error('Fixed deposit not found');
  if (fd.status !== 'active') throw new Error('Fixed deposit not active');

  const start = new Date(fd.start_date);
  const now = new Date();
  const daysDiff = Math.max(0, Math.floor((now.getTime() - start.getTime()) / (1000 * 60 * 60 * 24)));

  const annualRate = fd.interest_rate / 100;
  const accruedInterest = fd.amount * annualRate * (daysDiff / 365);
  
  // 50% interest penalty
  const finalInterest = accruedInterest * 0.5;
  const totalPayout = fd.amount + finalInterest;

  const accounts = getStorage<Account[]>('accounts', []);
  const custAcc = accounts.find(a => a.user_id === userId && a.type === 'savings')!;
  const treasury = accounts.find(a => a.id === TREASURY_ACC_ID)!;

  fd.status = 'early_withdrawn';
  custAcc.balance += totalPayout;
  treasury.balance -= totalPayout;

  const txnId = crypto.randomUUID();
  const ref = makeRef('FDL');
  const txns = getStorage<Transaction[]>('transactions', []);
  txns.unshift({
    id: txnId,
    type: 'fixed_deposit_maturity',
    amount: totalPayout,
    status: 'completed',
    idempotency_key: crypto.randomUUID(),
    sender_account_id: treasury.id,
    receiver_account_id: custAcc.id,
    reference_number: ref,
    description: `Early liquidation of Fixed Deposit Cert ${fd.certificate_number}`,
    created_at: new Date().toISOString()
  });

  setStorage('fixed_deposits', fixedDeps);
  setStorage('accounts', accounts);
  setStorage('transactions', txns);

  addLedgerEntry(txnId, treasury.id, 'DEBIT', totalPayout, treasury.balance, `Early liquidation payout for Cert ${fd.certificate_number}`);
  addLedgerEntry(txnId, custAcc.id, 'CREDIT', totalPayout, custAcc.balance, `Accrued payout for Cert ${fd.certificate_number} with early withdrawal penalty`);

  addNotification(userId, 'Fixed Deposit Liquidated', `Certificate ${fd.certificate_number} was early liquidated. Paid ${totalPayout.toLocaleString()} NGN.`);
  addAudit(userId, 'fixed_deposit_early_withdrawal', `Early liquidated Fixed Deposit ${fd.certificate_number}`);

  return fd;
}

// -------------------------------------------------------------------------
// Nigerian Utility Payouts
// -------------------------------------------------------------------------

export function payUtility(accountNo: string, provider: string, recipient: string, amount: number, type: string, idempotencyKey: string) {
  // Utility maps to cash withdrawal
  const txns = getStorage<Transaction[]>('transactions', []);
  const cached = txns.find(t => t.idempotency_key === idempotencyKey);
  if (cached) return cached;

  const desc = `${type} purchase (${provider}): to ${recipient}`;
  const debitTxn = processWithdrawal(accountNo, amount, idempotencyKey, desc);

  // Generate tokens for electricity
  let token = '';
  if (type === 'Electricity') {
    const parts = [];
    for (let i = 0; i < 5; i++) {
      parts.push(Math.floor(1000 + Math.random() * 9000));
    }
    token = parts.join('-');
    const tokens = getStorage<ElectricityToken[]>('electricity_tokens', []);
    tokens.push({
      token,
      meter_number: recipient,
      units: amount / 85.00,
      amount,
      status: 'unused',
      generated_at: new Date().toISOString()
    });
    setStorage('electricity_tokens', tokens);
  }

  const response = {
    status: 'success',
    provider,
    recipient,
    amount,
    token: token || undefined,
    message: token ? 'Meter token generated successfully' : 'Payment confirmed',
    transaction: debitTxn
  };

  return response;
}

export function validatePrepaidToken(token: string) {
  const tokens = getStorage<ElectricityToken[]>('electricity_tokens', []);
  const t = tokens.find(tk => tk.token === token);
  if (!t) return { status: 'failed', message: 'Token not found.' };
  if (t.status === 'used') return { status: 'failed', message: 'Token already used.' };

  t.status = 'used';
  setStorage('electricity_tokens', tokens);

  return {
    status: 'success',
    meter_number: t.meter_number,
    units_added: t.units,
    message: `Prepaid meter charged successfully with ${t.units.toFixed(2)} kWh.`
  };
}

// -------------------------------------------------------------------------
// Merchant Payments
// -------------------------------------------------------------------------

export function registerMerchant(userId: string, businessName: string, webhookUrl: string) {
  const merchants = getStorage<MerchantAccount[]>('merchants', []);
  const mId = crypto.randomUUID();
  const apiKey = `mk_live_${crypto.randomUUID().replace(/-/g, '')}`;

  const newM: MerchantAccount = {
    id: mId,
    user_id: userId,
    business_name: businessName,
    webhook_url: webhookUrl,
    api_key: apiKey
  };
  merchants.push(newM);
  setStorage('merchants', merchants);

  // Create merchant wallet
  const accounts = getStorage<Account[]>('accounts', []);
  accounts.push({
    id: crypto.randomUUID(),
    user_id: userId,
    account_number: `2${Math.floor(100000000 + Math.random() * 900000000)}`,
    type: 'merchant',
    balance: 0.0,
    status: 'active',
    currency: 'NGN',
    created_at: new Date().toISOString()
  });
  setStorage('accounts', accounts);

  addAudit(userId, 'merchant_registration', `Registered business ${businessName}`);
  return newM;
}

export function createInvoice(merchantUserId: string, customerAcc: string, amount: number, description: string) {
  const merchants = getStorage<MerchantAccount[]>('merchants', []);
  const m = merchants.find(me => me.user_id === merchantUserId);
  if (!m) throw new Error('Merchant profile not found');

  const invoices = getStorage<MerchantInvoice[]>('invoices', []);
  const invId = crypto.randomUUID();
  const sig = hmacSha256Signature(invId + ":" + m.id + ":" + amount, m.api_key);

  const newInv: MerchantInvoice = {
    id: invId,
    merchant_id: m.id,
    customer_account_number: customerAcc,
    amount,
    description,
    status: 'pending',
    digital_signature: sig,
    created_at: new Date().toISOString()
  };

  invoices.push(newInv);
  setStorage('invoices', invoices);

  return newInv;
}

export function payInvoice(invoiceId: string, customerAccountNo: string, idempotencyKey: string) {
  const invoices = getStorage<MerchantInvoice[]>('invoices', []);
  const inv = invoices.find(i => i.id === invoiceId);
  if (!inv) throw new Error('Invoice not found');
  if (inv.status === 'paid') throw new Error('Invoice already paid');

  const merchants = getStorage<MerchantAccount[]>('merchants', []);
  const m = merchants.find(me => me.id === inv.merchant_id)!;

  const accounts = getStorage<Account[]>('accounts', []);
  const mWallet = accounts.find(a => a.user_id === m.user_id && a.type === 'merchant')!;

  const transferTxn = processTransfer(customerAccountNo, mWallet.account_number, inv.amount, idempotencyKey, `Invoice Settlement: ${inv.description}`);
  
  inv.status = 'paid';
  setStorage('invoices', invoices);

  // Simulate Webhook dispatch
  console.log(`[WEBHOOK DISPATCH] URL: ${m.webhook_url} | Event: invoice.paid | InvoiceID: ${invoiceId} | Signature: ${inv.digital_signature}`);

  return {
    status: 'success',
    invoice: inv,
    transfer: transferTxn
  };
}
