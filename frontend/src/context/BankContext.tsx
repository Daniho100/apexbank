import React, { createContext, useContext, useState, useEffect, useMemo } from 'react';
import * as bank from '../api/bankApi';

const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080';

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

  const reloadUserData = async () => {
    if (!currentUser) return;
    const token = sessionStorage.getItem('auth_token');
    if (!token) return;

    const headers = {
      'Authorization': `Bearer ${token}`,
      'Content-Type': 'application/json'
    };

    try {
      // 1. Fetch accounts
      const accountsRes = await fetch(`${API_URL}/api/accounts`, { headers });
      if (!accountsRes.ok) throw new Error('Failed to fetch accounts');
      const userAccs = await accountsRes.json();
      setUserAccounts(userAccs);

      // Set active account and fetch transactions if account exists
      if (userAccs.length > 0) {
        const currentSelected =
          userAccs.find((a: any) => a.account_number === activeAccount?.account_number) || userAccs[0];
        setActiveAccount(currentSelected);

        const txnsRes = await fetch(`${API_URL}/api/transactions?account_id=${currentSelected.id}`, { headers });
        if (txnsRes.ok) {
          const userTxns = await txnsRes.json();
          setTransactions(userTxns);
        }
        
        // If merchant, set merchant wallet
        if (currentUser.role === 'customer' || currentUser.role === 'merchant') {
          const mWallet = userAccs.find((a: any) => a.type === 'merchant');
          setMerchantWallet(mWallet || null);
        }
      }

      // 2. Fetch notifications
      const notesRes = await fetch(`${API_URL}/api/notifications`, { headers });
      if (notesRes.ok) {
        const notes = await notesRes.json();
        const readKey = `read_notifications_${currentUser.id}`;
        const readIds = JSON.parse(localStorage.getItem(readKey) || '[]');
        const updatedNotes = notes.map((n: any) => ({
          ...n,
          is_read: n.is_read || readIds.includes(n.id)
        }));
        setNotifications(updatedNotes);
      }

      // 3. Fetch loans
      const loansRes = await fetch(`${API_URL}/api/loans`, { headers });
      if (loansRes.ok) {
        const loans = await loansRes.json();
        setLoansList(loans);
      }

      // 4. Fetch loan schedules
      const schedulesRes = await fetch(`${API_URL}/api/loans/schedules`, { headers });
      if (schedulesRes.ok) {
        const schedules = await schedulesRes.json();
        setLoanSchedules(schedules);
      }

      // 5. Fetch fixed deposits
      const fdsRes = await fetch(`${API_URL}/api/fixed-deposits`, { headers });
      if (fdsRes.ok) {
        const fds = await fdsRes.json();
        setFdList(fds);
      }

      // 6. Fetch merchant profile & invoices
      const profileRes = await fetch(`${API_URL}/api/merchants/profile`, { headers });
      if (profileRes.ok) {
        const profile = await profileRes.json();
        setMerchantProfile(profile);
        if (profile) {
          const invoicesRes = await fetch(`${API_URL}/api/merchants/invoices`, { headers });
          if (invoicesRes.ok) {
            const invoices = await invoicesRes.json();
            setMerchantInvoices(invoices);
          }
        }
      }

      // 7. Fetch audit logs (admin only)
      if (currentUser.role === 'administrator') {
        const auditRes = await fetch(`${API_URL}/api/admin/audit-logs`, { headers });
        if (auditRes.ok) {
          const logs = await auditRes.json();
          setAuditLogs(logs);
        }
      }
    } catch (e) {
      console.error('Error reloading user data:', e);
    }
  };

  useEffect(() => {
    reloadUserData();
    if (currentUser) {
      if (currentUser.role === 'administrator') {
        setActiveTab('admin');
      } else {
        setActiveTab('overview');
      }
    }
  }, [currentUser]);

  // Load transactions when activeAccount changes
  useEffect(() => {
    if (activeAccount && currentUser) {
      const token = sessionStorage.getItem('auth_token');
      if (token) {
        const headers = {
          'Authorization': `Bearer ${token}`,
          'Content-Type': 'application/json'
        };
        fetch(`${API_URL}/api/transactions?account_id=${activeAccount.id}`, { headers })
          .then(res => res.json())
          .then(userTxns => {
            if (Array.isArray(userTxns)) {
              setTransactions(userTxns);
            }
          })
          .catch(err => console.error(err));
      }
    }
  }, [activeAccount?.account_number, currentUser]);

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
