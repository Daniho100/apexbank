import React, { createContext, useContext, useState, useEffect, useMemo } from 'react';
import * as bank from '../mockBackend';

interface BankContextType {
  currentUser: bank.User | null;
  setCurrentUser: (user: bank.User | null) => void;
  activeTab: 'overview' | 'transfers' | 'loans' | 'fixed-deposits' | 'utilities' | 'merchant' | 'admin';
  setActiveTab: (tab: 'overview' | 'transfers' | 'loans' | 'fixed-deposits' | 'utilities' | 'merchant' | 'admin') => void;
  devHudOpen: boolean;
  setDevHudOpen: (open: boolean) => void;
  notificationOpen: boolean;
  setNotificationOpen: (open: boolean) => void;
  userAccounts: bank.Account[];
  activeAccount: bank.Account | null;
  setActiveAccount: (account: bank.Account | null) => void;
  notifications: bank.Notification[];
  transactions: bank.Transaction[];
  auditLogs: bank.AuditLog[];
  fdList: bank.FixedDeposit[];
  loansList: bank.Loan[];
  loanSchedules: bank.LoanSchedule[];
  merchantProfile: bank.MerchantAccount | null;
  merchantInvoices: bank.MerchantInvoice[];
  merchantWallet: bank.Account | null;
  securityLogs: Array<{ time: string; event: string; detail: string }>;
  toast: { type: 'success' | 'error'; message: string } | null;
  idempotencyKey: string;
  isIdentityVerified: boolean;
  verifyIdentity: (bvnOrNin: string) => boolean;
  showToast: (type: 'success' | 'error', message: string) => void;
  logSecurity: (event: string, detail: string) => void;
  reloadUserData: () => void;
  regenerateIdempotencyKey: () => void;
  handleLogout: () => void;
}

const BankContext = createContext<BankContextType | undefined>(undefined);

export const BankProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [currentUser, setCurrentUser] = useState<bank.User | null>(null);
  const [activeTab, setActiveTab] = useState<'overview' | 'transfers' | 'loans' | 'fixed-deposits' | 'utilities' | 'merchant' | 'admin'>('overview');
  const [devHudOpen, setDevHudOpen] = useState(false);
  const [notificationOpen, setNotificationOpen] = useState(false);

  const [userAccounts, setUserAccounts] = useState<bank.Account[]>([]);
  const [activeAccount, setActiveAccount] = useState<bank.Account | null>(null);
  const [notifications, setNotifications] = useState<bank.Notification[]>([]);
  const [transactions, setTransactions] = useState<bank.Transaction[]>([]);
  const [auditLogs, setAuditLogs] = useState<bank.AuditLog[]>([]);

  const [fdList, setFdList] = useState<bank.FixedDeposit[]>([]);
  const [loansList, setLoansList] = useState<bank.Loan[]>([]);
  const [loanSchedules, setLoanSchedules] = useState<bank.LoanSchedule[]>([]);

  const [merchantProfile, setMerchantProfile] = useState<bank.MerchantAccount | null>(null);
  const [merchantInvoices, setMerchantInvoices] = useState<bank.MerchantInvoice[]>([]);
  const [merchantWallet, setMerchantWallet] = useState<bank.Account | null>(null);

  const [securityLogs, setSecurityLogs] = useState<Array<{ time: string; event: string; detail: string }>>([]);
  const [toast, setToast] = useState<{ type: 'success' | 'error'; message: string } | null>(null);
  const [idempotencyKey, setIdempotencyKey] = useState(crypto.randomUUID());
  
  // Identity verification states
  const [isIdentityVerified, setIsIdentityVerified] = useState(false);

  // Sync identity verification state when user logs in/out
  useEffect(() => {
    if (currentUser) {
      const verified = localStorage.getItem(`verified_${currentUser.id}`) === 'true';
      setIsIdentityVerified(verified);
    } else {
      setIsIdentityVerified(false);
    }
  }, [currentUser]);

  const verifyIdentity = (bvnOrNin: string): boolean => {
    if (!currentUser) return false;
    const cleanNum = bvnOrNin.replace(/\D/g, '');
    if (cleanNum.length === 11) {
      localStorage.setItem(`verified_${currentUser.id}`, 'true');
      setIsIdentityVerified(true);
      logSecurity('Identity Verified', `User verified profile via BVN/NIN: ${cleanNum.slice(0, 3)}*******${cleanNum.slice(-2)}`);
      
      // Seed audit log and system notification
      bank.addNotification(
        currentUser.id, 
        'Identity Verified Successfully', 
        'Your identity profile (BVN/NIN Verification status) has been successfully verified. High-value transfer limits have been unlocked.'
      );
      reloadUserData();
      return true;
    }
    return false;
  };

  // Show status popup toast
  const showToast = (type: 'success' | 'error', message: string) => {
    setToast({ type, message });
    setTimeout(() => setToast(null), 5000);
  };

  // Add developer security trace
  const logSecurity = (event: string, detail: string) => {
    setSecurityLogs((prev) => [
      {
        time: new Date().toLocaleTimeString(),
        event,
        detail,
      },
      ...prev.slice(0, 19),
    ]);
  };

  const regenerateIdempotencyKey = () => {
    setIdempotencyKey(crypto.randomUUID());
  };

  const reloadUserData = () => {
    if (!currentUser) return;
    const accounts = bank.getStorage<bank.Account[]>('accounts', []);
    const userAccs = accounts.filter((a) => a.user_id === currentUser.id);
    setUserAccounts(userAccs);
    if (userAccs.length > 0) {
      const currentSelected =
        userAccs.find((a) => a.account_number === activeAccount?.account_number) || userAccs[0];
      setActiveAccount(currentSelected);

      const txns = bank.getStorage<bank.Transaction[]>('transactions', []);
      const userTxns = txns.filter(
        (t) => t.sender_account_id === currentSelected.id || t.receiver_account_id === currentSelected.id
      );
      setTransactions(userTxns);
    }

    const notes = bank
      .getStorage<bank.Notification[]>('notifications', [])
      .filter((n) => n.user_id === currentUser.id);
    setNotifications(notes);

    const loans = bank.getStorage<bank.Loan[]>('loans', []).filter((l) => l.user_id === currentUser.id);
    setLoansList(loans);

    const schedules = bank.getStorage<bank.LoanSchedule[]>('loan_schedules', []);
    setLoanSchedules(schedules);

    const fds = bank.getStorage<bank.FixedDeposit[]>('fixed_deposits', []).filter((f) => f.user_id === currentUser.id);
    setFdList(fds);

    const merchants = bank.getStorage<bank.MerchantAccount[]>('merchants', []);
    const merc = merchants.find((m) => m.user_id === currentUser.id);
    setMerchantProfile(merc || null);
    if (merc) {
      const mercInvs = bank.getStorage<bank.MerchantInvoice[]>('invoices', []).filter((i) => i.merchant_id === merc.id);
      setMerchantInvoices(mercInvs);
      const mWallet = accounts.find((a) => a.user_id === currentUser.id && a.type === 'merchant');
      setMerchantWallet(mWallet || null);
    }

    setAuditLogs(bank.getStorage<bank.AuditLog[]>('audit_logs', []));
  };

  useEffect(() => {
    reloadUserData();
  }, [currentUser]);

  // Handle auto load of token if exists in session storage
  useEffect(() => {
    const token = sessionStorage.getItem('auth_token');
    if (token) {
      try {
        const payloadBase64 = token.split('.')[1];
        if (payloadBase64) {
          const payload = JSON.parse(atob(payloadBase64));
          const users = bank.getStorage<bank.User[]>('users', []);
          const searchId = payload.user_id || payload.userId;
          let matchedUser = users.find((u) => u.id === searchId);
          if (!matchedUser && payload.email) {
            // Restore from JWT payload directly if missing in local list
            matchedUser = {
              id: searchId,
              email: payload.email,
              role: payload.role,
              status: 'active',
              created_at: new Date().toISOString()
            };
            users.push(matchedUser);
            bank.setStorage('users', users);
          }
          if (matchedUser) {
            setCurrentUser(matchedUser);
            logSecurity('Session Restored', `JWT session loaded for user: ${matchedUser.email}`);
          }
        }
      } catch (e) {
        sessionStorage.removeItem('auth_token');
      }
    }
  }, []);

  const handleLogout = () => {
    sessionStorage.removeItem('auth_token');
    setCurrentUser(null);
    logSecurity('Session Destroyed', 'Authentication token invalidated.');
    showToast('success', 'Logged out.');
    setActiveTab('overview');
  };

  const value = useMemo(
    () => ({
      currentUser,
      setCurrentUser,
      activeTab,
      setActiveTab,
      devHudOpen,
      setDevHudOpen,
      notificationOpen,
      setNotificationOpen,
      userAccounts,
      activeAccount,
      setActiveAccount,
      notifications,
      transactions,
      auditLogs,
      fdList,
      loansList,
      loanSchedules,
      merchantProfile,
      merchantInvoices,
      merchantWallet,
      securityLogs,
      toast,
      idempotencyKey,
      isIdentityVerified,
      verifyIdentity,
      showToast,
      logSecurity,
      reloadUserData,
      regenerateIdempotencyKey,
      handleLogout,
    }),
    [
      currentUser,
      activeTab,
      devHudOpen,
      notificationOpen,
      userAccounts,
      activeAccount,
      notifications,
      transactions,
      auditLogs,
      fdList,
      loansList,
      loanSchedules,
      merchantProfile,
      merchantInvoices,
      merchantWallet,
      securityLogs,
      toast,
      idempotencyKey,
      isIdentityVerified,
    ]
  );

  return <BankContext.Provider value={value}>{children}</BankContext.Provider>;
};

export const useBank = () => {
  const context = useContext(BankContext);
  if (context === undefined) {
    throw new Error('useBank must be used within a BankProvider');
  }
  return context;
};
