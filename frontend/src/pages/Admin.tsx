import React, { useState, useMemo, useEffect } from 'react';
import { 
  Shield, Terminal, Search, UserCheck, UserX, AlertTriangle, Info, BookOpen, Layers, 
  Users, TrendingUp, Trash2, Sliders, DollarSign, Activity, ChevronRight
} from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../api/bankApi';

const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080';

const btnDanger = "bg-rose-600 hover:bg-rose-500 text-white font-semibold py-2.5 px-4 rounded-xl transition-all active:scale-[0.98] cursor-pointer text-xs flex items-center gap-1.5 border border-rose-500/10";
const btnSuccess = "bg-emerald-600 hover:bg-emerald-500 text-white font-semibold py-2.5 px-4 rounded-xl transition-all active:scale-[0.98] cursor-pointer text-xs flex items-center gap-1.5 border border-emerald-500/10";

interface AdminProps {
  initialTab?: 'dashboard' | 'directory' | 'approvals' | 'audits';
}

export const Admin: React.FC<AdminProps> = ({ initialTab = 'dashboard' }) => {
  const { loansList, loanSchedules, auditLogs, reloadUserData, showToast, setActiveTab } = useBank();
  
  // Tab states for inner admin sidebar
  const [adminTab, setAdminTab] = useState<'dashboard' | 'directory' | 'approvals' | 'audits'>(initialTab);

  useEffect(() => {
    setAdminTab(initialTab);
  }, [initialTab]);

  // Selection states
  const [selectedUserId, setSelectedUserId] = useState<string | null>(null);
  const [userSearchQuery, setUserSearchQuery] = useState('');
  
  // Custom loan limit input state
  const [customLimit, setCustomLimit] = useState('');
  
  // Double check delete user modal
  const [showDeleteConfirm, setShowDeleteConfirm] = useState(false);
  const [userToDelete, setUserToDelete] = useState<string | null>(null);

  // Dynamic user list from REST API
  const [allUsers, setAllUsers] = useState<bank.User[]>([]);
  
  // System stats aggregates
  const [stats, setStats] = useState({
    total_users: 0,
    total_savings: 0,
    total_loans: 0,
    treasury_balance: 0
  });
  
  // Dynamic activity details from REST API for inspector
  const [selectedUserActivity, setSelectedUserActivity] = useState<{
    accounts: bank.Account[];
    transactions: bank.Transaction[];
    audit_logs: bank.AuditLog[];
  } | null>(null);

  const fetchUsers = () => {
    const token = sessionStorage.getItem('auth_token');
    fetch(`${API_URL}/api/admin/users`, {
      headers: { 'Authorization': `Bearer ${token}` }
    })
      .then(res => res.json())
      .then(data => {
        if (Array.isArray(data)) {
          setAllUsers(data.filter(u => u.email !== 'system_treasury@banking.com'));
        }
      })
      .catch(err => console.error(err));
  };

  const fetchStats = async () => {
    try {
      const data = await bank.fetchSystemStats();
      if (data && typeof data === 'object') {
        setStats({
          total_users: data.total_users || 0,
          total_savings: data.total_savings || 0,
          total_loans: data.total_loans || 0,
          treasury_balance: data.treasury_balance || 0
        });
      }
    } catch (err) {
      console.error("Failed to load statistics", err);
    }
  };

  useEffect(() => {
    fetchUsers();
    fetchStats();
  }, [loansList]);

  useEffect(() => {
    if (!selectedUserId) {
      setSelectedUserActivity(null);
      return;
    }
    const token = sessionStorage.getItem('auth_token');
    fetch(`${API_URL}/api/admin/users/${selectedUserId}/activity`, {
      headers: { 'Authorization': `Bearer ${token}` }
    })
      .then(res => res.json())
      .then(data => setSelectedUserActivity(data))
      .catch(err => console.error(err));
  }, [selectedUserId, loansList]);

  // 1. Pending Loan Approvals list (globally across all users)
  const pendingLoans = useMemo(() => {
    return loansList.filter(l => l.status === 'pending');
  }, [loansList]);

  // Filter user list based on search query
  const filteredUsers = useMemo(() => {
    return allUsers.filter(u => 
      u.email.toLowerCase().includes(userSearchQuery.toLowerCase()) || 
      u.id.toLowerCase().includes(userSearchQuery.toLowerCase())
    );
  }, [allUsers, userSearchQuery]);

  // Scan for Overdue Loans that are Unpaid
  const overdueAlertUsers = useMemo(() => {
    const now = new Date();
    const overdueLoanIds = loanSchedules
      .filter(s => s.status === 'unpaid' && new Date(s.due_date) < now)
      .map(s => s.loan_id);
      
    const userIdsWithOverdue = loansList
      .filter(l => overdueLoanIds.includes(l.id) && l.status === 'active')
      .map(l => l.user_id);
      
    return allUsers.filter(u => userIdsWithOverdue.includes(u.id));
  }, [allUsers, loansList, loanSchedules]);

  // Selected User's Details
  const selectedUser = useMemo(() => {
    const userObj = allUsers.find(u => u.id === selectedUserId) || null;
    if (userObj && (!customLimit || selectedUserId !== userObj.id)) {
      setCustomLimit(userObj.loan_limit.toString());
    }
    return userObj;
  }, [allUsers, selectedUserId]);

  const selectedUserAccounts = selectedUserActivity?.accounts || [];
  const selectedUserTransactions = selectedUserActivity?.transactions || [];
  const selectedUserAuditLogs = selectedUserActivity?.audit_logs || [];

  // Action: Approve Loan Request
  const handleApproveLoan = async (loanId: string) => {
    try {
      await bank.approveLoan(loanId);
      showToast('success', 'Loan approved and funds disbursed successfully.');
      await reloadUserData();
      fetchStats();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  // Action: Freeze / Unfreeze a User
  const handleToggleStatus = async (userId: string, currentStatus: string) => {
    try {
      const nextStatus = currentStatus === 'active' ? 'frozen' : 'active';
      await bank.setUserStatus(userId, nextStatus);
      showToast('success', `User status updated to: ${nextStatus.toUpperCase()}`);
      fetchUsers();
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  // Action: Update User Loan Credit Limit
  const handleUpdateLimitSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!selectedUserId || !customLimit) return;
    try {
      const amt = parseFloat(customLimit);
      if (isNaN(amt) || amt <= 0) {
        return showToast('error', 'Please enter a valid credit limit amount.');
      }
      await bank.updateLoanLimit(selectedUserId, amt);
      showToast('success', `User credit limit increased to ${amt.toLocaleString()} NGN.`);
      fetchUsers();
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  // Action: Delete User cascading flow
  const handleDeleteUserClick = (userId: string) => {
    setUserToDelete(userId);
    setShowDeleteConfirm(true);
  };

  const handleConfirmDeleteUser = async () => {
    if (!userToDelete) return;
    try {
      const res = await bank.deleteUser(userToDelete);
      showToast('success', res.message || 'User deleted successfully.');
      setSelectedUserId(null);
      setUserToDelete(null);
      setShowDeleteConfirm(false);
      fetchUsers();
      fetchStats();
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message || 'Failed to delete user.');
      setShowDeleteConfirm(false);
    }
  };

  return (
    <div className="pb-12 min-h-[600px]">
      {/* RIGHT CONTENT PANE */}
      <div className="min-w-0">
        
        {/* TABS SWITCHING CONTENT */}
        {adminTab === 'dashboard' && (
          <div className="flex flex-col gap-8 animate-in fade-in slide-in-from-bottom-2 duration-300">
            
            {/* STATS OVERVIEW CARDS */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
              
              <div className="bg-slate-900 border border-slate-850 p-6 rounded-3xl flex items-center justify-between shadow-xl">
                <div>
                  <span className="text-[10px] text-slate-500 uppercase tracking-widest font-black block">Active Users</span>
                  <span className="text-2xl font-black text-white block mt-1.5">{stats.total_users}</span>
                  <span className="text-[9px] text-emerald-450 block mt-1.5 font-bold">100% postgres storage</span>
                </div>
                <div className="w-12 h-12 bg-blue-500/10 border border-blue-500/25 rounded-2xl flex items-center justify-center text-blue-400">
                  <Users size={20} />
                </div>
              </div>

              <div className="bg-slate-900 border border-slate-850 p-6 rounded-3xl flex items-center justify-between shadow-xl">
                <div>
                  <span className="text-[10px] text-slate-500 uppercase tracking-widest font-black block">Savings Deposits</span>
                  <span className="text-lg font-black text-white block mt-1.5 truncate max-w-[150px]" title={`${stats.total_savings.toLocaleString()} NGN`}>
                    {stats.total_savings.toLocaleString()} NGN
                  </span>
                  <span className="text-[9px] text-blue-400 block mt-1.5 font-bold">Total Savings Wallets</span>
                </div>
                <div className="w-12 h-12 bg-indigo-500/10 border border-indigo-500/25 rounded-2xl flex items-center justify-center text-indigo-400">
                  <DollarSign size={20} />
                </div>
              </div>

              <div className="bg-slate-900 border border-slate-850 p-6 rounded-3xl flex items-center justify-between shadow-xl">
                <div>
                  <span className="text-[10px] text-slate-500 uppercase tracking-widest font-black block">Active Credit Issued</span>
                  <span className="text-lg font-black text-white block mt-1.5 truncate max-w-[150px]" title={`${stats.total_loans.toLocaleString()} NGN`}>
                    {stats.total_loans.toLocaleString()} NGN
                  </span>
                  <span className="text-[9px] text-orange-400 block mt-1.5 font-bold">Disbursed Loan Balance</span>
                </div>
                <div className="w-12 h-12 bg-orange-500/10 border border-orange-500/25 rounded-2xl flex items-center justify-center text-orange-400">
                  <Activity size={20} />
                </div>
              </div>

              <div className="bg-slate-900 border border-slate-850 p-6 rounded-3xl flex items-center justify-between shadow-xl">
                <div>
                  <span className="text-[10px] text-slate-500 uppercase tracking-widest font-black block">System Treasury</span>
                  <span className="text-lg font-black text-white block mt-1.5 truncate max-w-[150px]" title={`${stats.treasury_balance.toLocaleString()} NGN`}>
                    {stats.treasury_balance.toLocaleString()} NGN
                  </span>
                  {stats.treasury_balance < 10000000 ? (
                    <span className="text-[9px] text-rose-450 block mt-1.5 font-bold flex items-center gap-0.5 animate-pulse">
                      <AlertTriangle size={10} /> Reserves Critical
                    </span>
                  ) : (
                    <span className="text-[9px] text-emerald-450 block mt-1.5 font-bold">Reserves Secured</span>
                  )}
                </div>
                <div className={`w-12 h-12 rounded-2xl flex items-center justify-center border ${
                  stats.treasury_balance < 10000000 
                    ? 'bg-rose-500/10 border-rose-500/25 text-rose-400' 
                    : 'bg-emerald-500/10 border-emerald-500/25 text-emerald-400'
                }`}>
                  <Shield size={20} />
                </div>
              </div>

            </div>

            {/* CENTRAL SYSTEM OVERVIEW */}
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
              
              <div className="bg-slate-900 border border-slate-850 p-6 rounded-3xl lg:col-span-2 shadow-xl flex flex-col gap-4">
                <h3 className="text-sm font-extrabold text-white uppercase tracking-wider flex items-center gap-2">
                  <Activity size={16} className="text-blue-500" /> Administrative Operations Dashboard
                </h3>
                <p className="text-xs text-slate-400 leading-relaxed">
                  Welcome to the Administrative Shell of Apex Trust Bank. You have high-privileged ledger access to configure credit thresholds, execute risk audits, freeze accounts, disburse loans, and terminate registry databases. Ensure compliance rules are followed when performing modifications.
                </p>
                <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mt-2">
                  <div className="bg-slate-955/40 border border-slate-850 p-4 rounded-2xl">
                    <span className="text-[10px] text-slate-500 font-bold uppercase tracking-wider block">Database Integrity</span>
                    <span className="text-xs text-emerald-450 font-bold block mt-1">POSTGRESQL v16 Healthy</span>
                    <span className="text-[9px] text-slate-500 block mt-0.5 font-mono">Host: postgres // Default Cascades</span>
                  </div>
                  <div className="bg-slate-955/40 border border-slate-850 p-4 rounded-2xl">
                    <span className="text-[10px] text-slate-500 font-bold uppercase tracking-wider block">Redis Cache System</span>
                    <span className="text-xs text-emerald-450 font-bold block mt-1">REDIS MEMORY ACTIVE</span>
                    <span className="text-[9px] text-slate-500 block mt-0.5 font-mono">Rate limiting logs cleared in real-time</span>
                  </div>
                </div>
              </div>

              <div className="bg-slate-900 border border-slate-850 p-6 rounded-3xl shadow-xl flex flex-col gap-4">
                <h3 className="text-sm font-extrabold text-white uppercase tracking-wider flex items-center gap-2">
                  <AlertTriangle size={16} className="text-rose-450" /> System Alerts & Notices
                </h3>
                <div className="flex flex-col gap-3.5 max-h-[220px] overflow-y-auto pr-1">
                  {pendingLoans.length > 0 && (
                    <div className="bg-orange-955/20 border border-orange-500/20 p-3.5 rounded-2xl flex flex-col gap-1">
                      <span className="text-[10px] font-black text-orange-400 uppercase">Pending Approvals</span>
                      <p className="text-[10px] text-orange-300 leading-normal">{pendingLoans.length} loan applications are waiting for admin disburse approvals.</p>
                      <button onClick={() => setActiveTab('admin-approvals')} className="text-left text-[9px] font-extrabold text-orange-400 hover:text-white mt-1 uppercase flex items-center gap-0.5">
                        Process applications <ChevronRight size={10} />
                      </button>
                    </div>
                  )}
                  {overdueAlertUsers.length > 0 && (
                    <div className="bg-rose-955/20 border border-rose-500/20 p-3.5 rounded-2xl flex flex-col gap-1">
                      <span className="text-[10px] font-black text-rose-400 uppercase">Overdue Loan Repayments</span>
                      <p className="text-[10px] text-rose-300 leading-normal">{overdueAlertUsers.length} user accounts have active loans with outstanding overdue installments.</p>
                      <button onClick={() => setActiveTab('admin-directory')} className="text-left text-[9px] font-extrabold text-rose-400 hover:text-white mt-1 uppercase flex items-center gap-0.5">
                        Inspect delinquent profiles <ChevronRight size={10} />
                      </button>
                    </div>
                  )}
                  {pendingLoans.length === 0 && overdueAlertUsers.length === 0 && (
                    <div className="text-center py-12 text-slate-500 text-xs flex flex-col gap-2 justify-center items-center h-full">
                      <UserCheck size={28} className="text-slate-650" />
                      All system risk thresholds are currently clear.
                    </div>
                  )}
                </div>
              </div>

            </div>

          </div>
        )}

        {adminTab === 'directory' && (
          <div className="flex flex-col gap-8 animate-in fade-in slide-in-from-bottom-2 duration-300">
            
            {/* GRID: Directory & Selected User Inspector */}
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
              
              {/* Directory Listing */}
              <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 lg:col-span-2 shadow-xl flex flex-col">
                <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 mb-6 border-b border-slate-850 pb-5">
                  <div>
                    <h3 className="font-bold text-white text-base flex items-center gap-2">
                      <BookOpen size={18} className="text-blue-500" /> System Users Directory
                    </h3>
                    <p className="text-xs text-slate-500 mt-1">Freeze profiles, adjust credit lines, and delete records.</p>
                  </div>
                  <div className="relative max-w-xs w-full">
                    <Search className="absolute left-3.5 top-1/2 -translate-y-1/2 text-slate-500" size={14} />
                    <input 
                      type="text"
                      value={userSearchQuery}
                      onChange={e => setUserSearchQuery(e.target.value)}
                      placeholder="Search email or ID..."
                      className="w-full bg-slate-950/80 border border-slate-800 rounded-xl pl-9 pr-4 py-2 text-xs text-white placeholder-slate-600 focus:outline-none focus:ring-1 focus:ring-blue-500"
                    />
                  </div>
                </div>

                <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 max-h-[500px] overflow-y-auto pr-1">
                  {filteredUsers.length === 0 ? (
                    <div className="col-span-2 text-center py-16 text-slate-500 text-sm">No matched users found.</div>
                  ) : (
                    filteredUsers.map(u => (
                      <div 
                        key={u.id}
                        onClick={() => setSelectedUserId(u.id === selectedUserId ? null : u.id)}
                        className={`border rounded-2xl p-4 flex flex-col justify-between gap-4 transition-all cursor-pointer ${
                          u.id === selectedUserId 
                            ? 'bg-blue-950/20 border-blue-500 scale-[1.01] shadow-lg shadow-blue-500/5' 
                            : 'bg-slate-950/20 border-slate-850 hover:bg-slate-950/30'
                        }`}
                      >
                        <div className="flex justify-between items-start">
                          <div className="overflow-hidden">
                            <span className="block text-sm font-bold text-white truncate">{u.email}</span>
                            <span className="text-[10px] text-slate-500 font-mono truncate block mt-0.5">ID: {u.id}</span>
                            <span className="text-[10px] font-extrabold text-blue-400 block mt-1">Limit: {u.loan_limit?.toLocaleString() || '1,000,000'} NGN</span>
                          </div>
                          <span className={`text-[9px] font-black uppercase tracking-wider px-2 py-0.5 rounded-full border ${
                            u.status === 'active' 
                              ? 'bg-emerald-950/40 border-emerald-500/20 text-emerald-400' 
                              : 'bg-rose-950/40 border-rose-500/20 text-rose-455'
                          }`}>
                            {u.status}
                          </span>
                        </div>

                        <div className="flex justify-between items-center gap-2 border-t border-slate-850/60 pt-3">
                          <span className="text-[9px] text-slate-400 bg-slate-850 px-2 py-0.5 rounded uppercase font-bold tracking-wider">{u.role}</span>
                          <div className="flex items-center gap-2">
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                handleToggleStatus(u.id, u.status);
                              }}
                              className={u.status === 'active' ? btnDanger : btnSuccess}
                            >
                              {u.status === 'active' ? (
                                <><UserX size={12} /> Freeze</>
                              ) : (
                                <><UserCheck size={12} /> Activate</>
                              )}
                            </button>
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                handleDeleteUserClick(u.id);
                              }}
                              className="bg-slate-800 hover:bg-rose-955/40 hover:text-rose-400 text-slate-450 p-2.5 rounded-xl transition-all border border-slate-700/50 active:scale-95 cursor-pointer"
                              title="Delete Account permanently"
                            >
                              <Trash2 size={12} />
                            </button>
                          </div>
                        </div>
                      </div>
                    ))
                  )}
                </div>
              </div>

              {/* Inspector Pane */}
              <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 xl:col-span-1 shadow-xl">
                {!selectedUser ? (
                  <div className="text-center py-24 text-slate-500 text-sm flex flex-col gap-3 justify-center items-center h-full">
                    <Info size={32} className="text-slate-650 mb-1" />
                    Select a profile from the directory to inspect wallets, configure limits, or view transaction trails.
                  </div>
                ) : (
                  <div className="flex flex-col gap-6 animate-in fade-in slide-in-from-right-2 duration-300">
                    <div className="border-b border-slate-850 pb-4">
                      <span className="text-[9px] bg-blue-500/10 text-blue-400 border border-blue-500/20 px-2.5 py-0.5 rounded-full font-bold uppercase tracking-wider">User Ledger Audit Inspector</span>
                      <h4 className="text-base font-black text-white mt-2.5 truncate" title={selectedUser.email}>{selectedUser.email}</h4>
                      <span className="text-[10px] text-slate-500 font-mono block truncate mt-0.5">Wallet ID: {selectedUser.id}</span>
                    </div>

                    {/* CREDIT LIMIT CONFIGURATOR */}
                    <form onSubmit={handleUpdateLimitSubmit} className="bg-slate-955/40 border border-slate-850 p-4 rounded-2xl flex flex-col gap-3">
                      <span className="text-[10px] text-slate-450 font-black uppercase tracking-wider flex items-center gap-1.5">
                        <Sliders size={12} className="text-blue-500" /> Credit Limit Controls
                      </span>
                      <div className="flex gap-2">
                        <input 
                          type="number"
                          value={customLimit}
                          onChange={e => setCustomLimit(e.target.value)}
                          placeholder="Limit (NGN)..."
                          className="flex-1 bg-slate-900 border border-slate-800 rounded-xl px-3 py-2 text-xs text-white placeholder-slate-600 focus:outline-none"
                        />
                        <button 
                          type="submit"
                          className="bg-blue-600 hover:bg-blue-500 text-white font-bold text-xs px-4 py-2 rounded-xl transition-all active:scale-95 cursor-pointer flex items-center gap-1"
                        >
                          Update Limit
                        </button>
                      </div>
                      <p className="text-[9px] text-slate-500 leading-normal">
                        Sets the maximum accumulated outstanding loan debt that this customer is allowed to borrow. Default is 1,000,000 NGN.
                      </p>
                    </form>

                    {/* ACCOUNTS WALLETS */}
                    <div className="flex flex-col gap-3">
                      <span className="text-[10px] text-slate-450 font-black uppercase tracking-wider flex items-center gap-1.5">
                        <Layers size={12} className="text-blue-500" /> Account Wallets ({selectedUserAccounts.length})
                      </span>
                      <div className="flex flex-col gap-2 max-h-[180px] overflow-y-auto pr-1">
                        {selectedUserAccounts.length === 0 ? (
                          <div className="text-center py-4 text-slate-500 text-[10px] bg-slate-955/10 border border-slate-850 rounded-2xl">No accounts linked.</div>
                        ) : (
                          selectedUserAccounts.map(acc => (
                            <div key={acc.id} className="bg-slate-955/30 border border-slate-850/80 rounded-2xl p-3 flex justify-between items-center gap-4">
                              <div>
                                <span className="text-[9px] text-slate-500 uppercase font-black tracking-wider block">{acc.type}</span>
                                <span className="font-mono text-[10px] text-slate-350 block mt-0.5">{acc.account_number}</span>
                              </div>
                              <span className="text-xs font-black text-white shrink-0">{acc.balance.toLocaleString()} NGN</span>
                            </div>
                          ))
                        )}
                      </div>
                    </div>

                    {/* TRANSACTIONS HISTORIES */}
                    <div className="flex flex-col gap-3">
                      <span className="text-[10px] text-slate-450 font-black uppercase tracking-wider flex items-center gap-1.5">
                        <Activity size={12} className="text-blue-500" /> Recent Transactions ({selectedUserTransactions.length})
                      </span>
                      <div className="flex flex-col gap-2 max-h-[180px] overflow-y-auto pr-1">
                        {selectedUserTransactions.length === 0 ? (
                          <div className="text-center py-4 text-slate-500 text-[10px] bg-slate-955/10 border border-slate-850 rounded-2xl">No transaction records.</div>
                        ) : (
                          selectedUserTransactions.map(t => {
                            const isCredit = selectedUserAccounts.some(a => a.id === t.receiver_account_id);
                            return (
                              <div key={t.id} className="bg-slate-955/30 border border-slate-850/80 rounded-2xl p-2.5 flex flex-col gap-1 text-[10px]">
                                <div className="flex justify-between items-start gap-2">
                                  <span className="text-slate-450 truncate font-semibold uppercase">{t.type.replace('_', ' ')}</span>
                                  <span className={`font-black shrink-0 ${isCredit ? 'text-emerald-450' : 'text-rose-455'}`}>
                                    {isCredit ? '+' : '-'} {t.amount.toLocaleString()} NGN
                                  </span>
                                </div>
                                <div className="flex justify-between items-center text-[9px] text-slate-500 font-mono">
                                  <span>{t.reference_number || t.id.slice(0, 8)}</span>
                                  <span>{new Date(t.created_at).toLocaleDateString()}</span>
                                </div>
                              </div>
                            );
                          })
                        )}
                      </div>
                    </div>
                    {/* USER AUDIT LOGS */}
                    <div className="flex flex-col gap-3">
                      <span className="text-[10px] text-slate-450 font-black uppercase tracking-wider flex items-center gap-1.5">
                        <Terminal size={12} className="text-blue-500" /> Recent Security Logs ({selectedUserAuditLogs.length})
                      </span>
                      <div className="flex flex-col gap-2 max-h-[150px] overflow-y-auto pr-1">
                        {selectedUserAuditLogs.length === 0 ? (
                          <div className="text-center py-4 text-slate-500 text-[10px] bg-slate-955/10 border border-slate-850 rounded-2xl">No security logs.</div>
                        ) : (
                          selectedUserAuditLogs.map(log => (
                            <div key={log.id} className="bg-slate-955/40 border border-slate-850/60 rounded-2xl p-2.5 flex flex-col gap-1 text-[10px]">
                              <div className="flex justify-between items-center text-[9px] font-mono text-slate-500">
                                <span>{new Date(log.created_at).toLocaleTimeString()}</span>
                                <span>IP: {log.ip_address}</span>
                              </div>
                              <span className="font-extrabold text-blue-400 uppercase tracking-widest">{log.action}</span>
                              <p className="text-slate-450 leading-normal">{log.description}</p>
                            </div>
                          ))
                        )}
                      </div>
                    </div>

                  </div>
                )}
              </div>

            </div>

          </div>
        )}

        {adminTab === 'approvals' && (
          <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 shadow-xl animate-in fade-in slide-in-from-bottom-2 duration-300">
            <h3 className="font-bold text-white text-base mb-6 flex items-center gap-2">
              <Shield size={18} className="text-blue-500" /> Pending Loan Approvals
            </h3>
            
            {pendingLoans.length === 0 ? (
              <div className="text-center py-24 text-slate-500 text-sm flex flex-col gap-2 justify-center items-center">
                <UserCheck size={36} className="text-slate-655" />
                No credit applications pending approval.
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-6">
                {pendingLoans.map(loan => {
                  const loanUser = allUsers.find(u => u.id === loan.user_id);
                  return (
                    <div key={loan.id} className="bg-slate-955/40 border border-slate-850 rounded-2xl p-5 flex flex-col justify-between gap-5">
                      <div>
                        <span className="text-[9px] text-slate-500 font-mono block truncate">USER: {loanUser ? loanUser.email : loan.user_id}</span>
                        <span className="text-[10px] text-slate-400 block mt-0.5">LOAN: {loan.name || 'Personal Loan'} ({loan.reference_number || loan.id.slice(0,8)})</span>
                        <span className="text-base font-black text-white block mt-2">{loan.amount.toLocaleString()} NGN</span>
                        <span className="text-[10px] text-slate-455 block mt-0.5">Term: {loan.duration_months} M | APR: {loan.interest_rate}%</span>
                      </div>
                      <button 
                        onClick={() => handleApproveLoan(loan.id)}
                        className="w-full py-2.5 rounded-xl bg-emerald-600 hover:bg-emerald-500 text-white font-bold text-xs transition-all shadow-md active:scale-95 cursor-pointer flex items-center justify-center gap-1.5"
                      >
                        Approve & Disburse Cash
                      </button>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        )}

        {adminTab === 'audits' && (
          <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 shadow-xl animate-in fade-in slide-in-from-bottom-2 duration-300">
            <h3 className="font-bold text-white text-base mb-6 flex items-center gap-2">
              <Terminal size={18} className="text-blue-500" /> System-Wide Security Audit Trails
            </h3>
            
            {auditLogs.length === 0 ? (
              <div className="text-center py-24 text-slate-500 text-sm">No audit logs registered in system.</div>
            ) : (
              <div className="overflow-x-auto max-h-[500px]">
                <table className="w-full text-xs text-left text-slate-400">
                  <thead>
                    <tr className="border-b border-slate-800 text-slate-500 uppercase tracking-wider">
                      <th className="pb-3 pr-4">Timestamp</th>
                      <th className="pb-3 pr-4">Event Action</th>
                      <th className="pb-3 pr-4">User Identifier</th>
                      <th className="pb-3 pr-4">Detailed Description</th>
                      <th className="pb-3">IP Address</th>
                    </tr>
                  </thead>
                  <tbody>
                    {auditLogs.map(log => (
                      <tr key={log.id} className="border-b border-slate-850/50 text-slate-355 hover:bg-slate-855/10 transition-colors">
                        <td className="py-3.5 pr-4 font-mono text-[10px] text-slate-500">{new Date(log.created_at).toLocaleString()}</td>
                        <td className="py-3.5 pr-4 uppercase font-black text-[9px] tracking-widest text-blue-400">{log.action}</td>
                        <td className="py-3.5 pr-4 font-mono text-[10px] text-slate-400 truncate max-w-[120px]" title={log.user_email || log.user_id}>
                          {log.user_email || log.user_id}
                        </td>
                        <td className="py-3.5 pr-4 text-slate-300 text-xs">{log.description}</td>
                        <td className="py-3.5 font-mono text-[10px] text-slate-500">{log.ip_address}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </div>
        )}

      </div>

      {/* DOUBLE CONFIRM DELETE ACCOUNT MODAL */}
      {showDeleteConfirm && (
        <div className="fixed inset-0 z-50 bg-slate-950/80 backdrop-blur-sm flex items-center justify-center p-4">
          <div className="bg-slate-900 border border-slate-800 max-w-md w-full p-6 rounded-3xl shadow-2xl flex flex-col gap-5 animate-in zoom-in-95 duration-200">
            <div className="flex items-center gap-3 border-b border-slate-800 pb-4">
              <AlertTriangle className="text-rose-500 animate-bounce shrink-0" size={24} />
              <h3 className="font-extrabold text-white text-base">Terminate Customer Profile?</h3>
            </div>
            <p className="text-xs text-slate-400 leading-normal">
              Warning! Deleting this customer is a destructive cascade action that permanently deletes their database record, all savings accounts, active or pending credit records, and ledger transaction histories. This cannot be undone.
            </p>
            <div className="flex justify-end gap-3 pt-2">
              <button 
                onClick={() => {
                  setShowDeleteConfirm(false);
                  setUserToDelete(null);
                }}
                className="bg-slate-800 hover:bg-slate-700 text-slate-300 text-xs font-bold px-4 py-2.5 rounded-xl transition-all cursor-pointer"
              >
                Keep Account
              </button>
              <button 
                onClick={handleConfirmDeleteUser}
                className="bg-rose-600 hover:bg-rose-500 text-white text-xs font-bold px-4 py-2.5 rounded-xl transition-all shadow-lg shadow-rose-600/15 active:scale-95 cursor-pointer"
              >
                Yes, Delete Permanently
              </button>
            </div>
          </div>
        </div>
      )}

    </div>
  );
};

export default Admin;
