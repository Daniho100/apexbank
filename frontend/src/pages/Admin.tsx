import React, { useState, useMemo } from 'react';
import { 
  Shield, Terminal, Search, UserCheck, UserX, AlertTriangle, Info, BookOpen, Layers
} from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../mockBackend';

const btnPrimary = "bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-semibold py-2.5 px-4 rounded-xl transition-all shadow-lg shadow-blue-500/10 active:scale-[0.98] cursor-pointer text-xs flex items-center gap-1.5 border border-blue-500/10";
const btnDanger = "bg-rose-600 hover:bg-rose-500 text-white font-semibold py-2.5 px-4 rounded-xl transition-all active:scale-[0.98] cursor-pointer text-xs flex items-center gap-1.5 border border-rose-500/10";
const btnSuccess = "bg-emerald-600 hover:bg-emerald-500 text-white font-semibold py-2.5 px-4 rounded-xl transition-all active:scale-[0.98] cursor-pointer text-xs flex items-center gap-1.5 border border-emerald-500/10";

export const Admin: React.FC = () => {
  const { loansList, auditLogs, reloadUserData, showToast } = useBank();
  
  // Selection states
  const [selectedUserId, setSelectedUserId] = useState<string | null>(null);
  const [userSearchQuery, setUserSearchQuery] = useState('');

  // 1. Pending Loan Approvals list (globally across all users)
  const pendingLoans = useMemo(() => {
    return loansList.filter(l => l.status === 'pending');
  }, [loansList]);

  // 2. Fetch all system users (excluding treasury system users)
  const allUsers = useMemo(() => {
    const users = bank.getStorage<bank.User[]>('users', []);
    return users.filter(u => u.email !== 'system_treasury@banking.com');
  }, [loansList]);

  // Filter user list based on search query
  const filteredUsers = useMemo(() => {
    return allUsers.filter(u => 
      u.email.toLowerCase().includes(userSearchQuery.toLowerCase()) || 
      u.id.toLowerCase().includes(userSearchQuery.toLowerCase())
    );
  }, [allUsers, userSearchQuery]);

  // 3. Scan for Overdue Loans that are Unpaid
  const overdueAlertUsers = useMemo(() => {
    const loans = bank.getStorage<bank.Loan[]>('loans', []);
    const schedules = bank.getStorage<bank.LoanSchedule[]>('loan_schedules', []);
    const now = new Date();
    
    // Installments that are overdue and unpaid
    const overdueLoanIds = schedules
      .filter(s => s.status === 'unpaid' && new Date(s.due_date) < now)
      .map(s => s.loan_id);
      
    const userIdsWithOverdue = loans
      .filter(l => overdueLoanIds.includes(l.id) && l.status === 'active')
      .map(l => l.user_id);
      
    return allUsers.filter(u => userIdsWithOverdue.includes(u.id));
  }, [allUsers, loansList]);

  // Selected User's Details
  const selectedUser = useMemo(() => {
    return allUsers.find(u => u.id === selectedUserId) || null;
  }, [allUsers, selectedUserId]);

  const selectedUserAccounts = useMemo(() => {
    if (!selectedUserId) return [];
    const accounts = bank.getStorage<bank.Account[]>('accounts', []);
    return accounts.filter(a => a.user_id === selectedUserId);
  }, [selectedUserId, loansList]);

  const selectedUserTransactions = useMemo(() => {
    if (!selectedUserId) return [];
    const accIds = selectedUserAccounts.map(a => a.id);
    const txns = bank.getStorage<bank.Transaction[]>('transactions', []);
    return txns.filter(t => accIds.includes(t.sender_account_id) || accIds.includes(t.receiver_account_id));
  }, [selectedUserId, selectedUserAccounts, loansList]);

  const selectedUserAuditLogs = useMemo(() => {
    if (!selectedUserId) return [];
    const logs = bank.getStorage<bank.AuditLog[]>('audit_logs', []);
    return logs.filter(l => l.user_id === selectedUserId);
  }, [selectedUserId, loansList]);

  // Action: Approve Loan Request
  const handleApproveLoan = (loanId: string) => {
    try {
      bank.approveLoan(loanId);
      showToast('success', 'Loan approved and funds disbursed successfully.');
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  // Action: Freeze / Unfreeze a User (Blacklist)
  const handleToggleStatus = (userId: string, currentStatus: string) => {
    try {
      const nextStatus = currentStatus === 'active' ? 'frozen' : 'active';
      bank.setUserStatus(userId, nextStatus);
      showToast('success', `User status updated to: ${nextStatus.toUpperCase()}`);
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  return (
    <div className="flex flex-col gap-8 pb-12">
      
      {/* ⚠️ BLACKLIST ALERTS FOR OVERDUE LOANS */}
      {overdueAlertUsers.length > 0 && (
        <div className="bg-rose-955/40 border border-rose-800/40 rounded-3xl p-6 shadow-2xl">
          <div className="flex items-center gap-3 mb-4">
            <AlertTriangle className="text-rose-400 animate-pulse shrink-0" size={24} />
            <div>
              <h3 className="text-rose-200 font-bold text-base">Overdue Loan Repayment Alerts</h3>
              <p className="text-rose-450/80 text-xs">The following users have unpaid loan schedules that are past their due date. Please blacklist or contact them.</p>
            </div>
          </div>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {overdueAlertUsers.map(u => (
              <div key={u.id} className="bg-rose-950/20 border border-rose-900/30 rounded-2xl p-4 flex justify-between items-center gap-4">
                <div className="overflow-hidden">
                  <span className="block text-white font-bold text-sm truncate">{u.email}</span>
                  <span className="text-[10px] text-rose-350 uppercase tracking-wider font-mono">STATUS: {u.status}</span>
                </div>
                {u.status === 'active' ? (
                  <button 
                    onClick={() => handleToggleStatus(u.id, u.status)}
                    className="px-4 py-2 bg-rose-600 hover:bg-rose-500 text-white text-xs font-bold rounded-xl transition-all shadow-md active:scale-95 cursor-pointer"
                  >
                    Blacklist / Freeze User
                  </button>
                ) : (
                  <span className="px-3 py-1.5 bg-rose-900/30 text-rose-450 border border-rose-500/20 text-xs font-bold rounded-xl uppercase tracking-wider">
                    Blacklisted
                  </span>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Grid: Pending Loans & User Directory */}
      <div className="grid grid-cols-1 xl:grid-cols-3 gap-8">
        
        {/* PANEL: PENDING LOAN APPROVALS */}
        <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 xl:col-span-1 shadow-xl">
          <h3 className="font-bold text-white text-base mb-6 flex items-center gap-2">
            <Shield size={18} className="text-blue-500" /> Pending Loan Approvals
          </h3>
          
          {pendingLoans.length === 0 ? (
            <div className="text-center py-16 text-slate-500 text-sm flex flex-col gap-2 justify-center items-center">
              <UserCheck size={32} className="text-slate-600 mb-1" />
              No credit applications pending approval.
            </div>
          ) : (
            <div className="flex flex-col gap-4 max-h-[480px] overflow-y-auto pr-1">
              {pendingLoans.map(loan => {
                const loanUser = allUsers.find(u => u.id === loan.user_id);
                return (
                  <div key={loan.id} className="bg-slate-950/40 border border-slate-850 rounded-2xl p-4 flex flex-col gap-4">
                    <div>
                      <span className="text-[9px] text-slate-500 font-mono block truncate">USER: {loanUser ? loanUser.email : loan.user_id}</span>
                      <span className="text-sm font-black text-white block mt-1">{loan.amount.toLocaleString()} NGN</span>
                      <span className="text-[10px] text-slate-400 block mt-0.5">Term: {loan.duration_months} M | APR: {loan.interest_rate}%</span>
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

        {/* PANEL: USER DIRECTORY */}
        <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 xl:col-span-2 shadow-xl flex flex-col">
          <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 mb-6 border-b border-slate-850 pb-5">
            <div>
              <h3 className="font-bold text-white text-base flex items-center gap-2">
                <BookOpen size={18} className="text-blue-500" /> System Users Directory
              </h3>
              <p className="text-xs text-slate-500 mt-1">Audit user activities, freeze wallets, and configure security statuses.</p>
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

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4 max-h-[440px] overflow-y-auto pr-1">
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
                    </div>
                    <span className={`text-[9px] font-black uppercase tracking-wider px-2 py-0.5 rounded-full border ${
                      u.status === 'active' 
                        ? 'bg-emerald-950/40 border-emerald-500/20 text-emerald-400' 
                        : 'bg-rose-950/40 border-rose-500/20 text-rose-450'
                    }`}>
                      {u.status}
                    </span>
                  </div>

                  <div className="flex justify-between items-center gap-2 border-t border-slate-850/60 pt-3">
                    <span className="text-[9px] text-slate-400 bg-slate-850 px-2 py-0.5 rounded uppercase font-bold tracking-wider">{u.role}</span>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        handleToggleStatus(u.id, u.status);
                      }}
                      className={u.status === 'active' ? btnDanger : btnSuccess}
                    >
                      {u.status === 'active' ? (
                        <><UserX size={12} /> Freeze Wallet</>
                      ) : (
                        <><UserCheck size={12} /> Activate Wallet</>
                      )}
                    </button>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>
      </div>

      {/* USER DETAIL INSPECTOR VIEW */}
      {selectedUser && (
        <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 shadow-2xl animate-in fade-in slide-in-from-bottom-2 duration-300">
          <div className="flex justify-between items-start border-b border-slate-850 pb-5 mb-6">
            <div>
              <span className="text-[9px] bg-blue-500/10 text-blue-400 border border-blue-500/20 px-2.5 py-0.5 rounded-full font-bold uppercase tracking-wider">User Ledger Audit Inspector</span>
              <h3 className="text-lg font-black text-white mt-1.5">{selectedUser.email}</h3>
              <p className="text-xs text-slate-500 mt-0.5">Wallet ID: {selectedUser.id}</p>
            </div>
            <button 
              onClick={() => setSelectedUserId(null)}
              className="text-xs font-bold text-slate-500 hover:text-slate-300 bg-slate-950/50 hover:bg-slate-850 border border-slate-800 px-3 py-1.5 rounded-xl cursor-pointer"
            >
              Close Inspector
            </button>
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
            
            {/* COLUMN 1: User Accounts & Details */}
            <div className="flex flex-col gap-6">
              <h4 className="font-extrabold text-slate-300 text-xs uppercase tracking-wider flex items-center gap-1.5">
                <Layers size={14} className="text-blue-500" /> Account Wallets ({selectedUserAccounts.length})
              </h4>
              <div className="flex flex-col gap-4">
                {selectedUserAccounts.length === 0 ? (
                  <div className="text-center py-6 text-slate-500 text-xs bg-slate-950/10 border border-slate-850 rounded-2xl">No accounts linked to this profile.</div>
                ) : (
                  selectedUserAccounts.map(acc => (
                    <div key={acc.id} className="bg-slate-950/30 border border-slate-850 rounded-2xl p-4">
                      <div className="flex justify-between items-start">
                        <div>
                          <span className="text-[10px] text-slate-500 uppercase font-black tracking-wider">{acc.type} account</span>
                          <span className="block font-mono text-xs text-slate-300 mt-0.5">{acc.account_number}</span>
                        </div>
                        <span className={`text-[9px] px-2 py-0.5 rounded font-extrabold uppercase ${
                          acc.status === 'active' ? 'bg-emerald-950 text-emerald-400' : 'bg-slate-850 text-slate-500'
                        }`}>
                          {acc.status}
                        </span>
                      </div>
                      <div className="mt-4 pt-3 border-t border-slate-850/50 flex justify-between items-baseline">
                        <span className="text-[9px] text-slate-500 font-bold uppercase tracking-wider">Book balance</span>
                        <span className="text-sm font-black text-white">{acc.balance.toLocaleString()} NGN</span>
                      </div>
                    </div>
                  ))
                )}
              </div>
            </div>

            {/* COLUMN 2: User Transactions */}
            <div className="lg:col-span-1 flex flex-col gap-6">
              <h4 className="font-extrabold text-slate-300 text-xs uppercase tracking-wider flex items-center gap-1.5">
                <Info size={14} className="text-blue-500" /> Ledger Transactions ({selectedUserTransactions.length})
              </h4>
              <div className="flex flex-col gap-3 max-h-[350px] overflow-y-auto pr-1">
                {selectedUserTransactions.length === 0 ? (
                  <div className="text-center py-12 text-slate-500 text-xs bg-slate-950/10 border border-slate-850 rounded-2xl">No transaction history.</div>
                ) : (
                  selectedUserTransactions.map(t => {
                    const isCredit = selectedUserAccounts.some(a => a.id === t.receiver_account_id);
                    return (
                      <div key={t.id} className="bg-slate-950/30 border border-slate-850/80 rounded-2xl p-3.5 flex flex-col gap-2">
                        <div className="flex justify-between items-start gap-2">
                          <span className="text-[9px] text-slate-500 font-mono">{t.reference_number}</span>
                          <span className={`text-xs font-black shrink-0 ${isCredit ? 'text-emerald-400' : 'text-rose-450'}`}>
                            {isCredit ? '+' : '-'} {t.amount.toLocaleString()} NGN
                          </span>
                        </div>
                        <div className="flex justify-between items-center gap-2 text-[10px]">
                          <span className="text-slate-450 truncate font-semibold uppercase">{t.type.replace('_', ' ')}</span>
                          <span className="text-slate-500">{new Date(t.created_at).toLocaleDateString()}</span>
                        </div>
                        <p className="text-[10px] text-slate-500 leading-normal border-t border-slate-950/50 pt-1.5 truncate">{t.description}</p>
                      </div>
                    );
                  })
                )}
              </div>
            </div>

            {/* COLUMN 3: User Audit Logs */}
            <div className="lg:col-span-1 flex flex-col gap-6">
              <h4 className="font-extrabold text-slate-300 text-xs uppercase tracking-wider flex items-center gap-1.5">
                <Terminal size={14} className="text-blue-500" /> User Audit Trails ({selectedUserAuditLogs.length})
              </h4>
              <div className="flex flex-col gap-3 max-h-[350px] overflow-y-auto pr-1">
                {selectedUserAuditLogs.length === 0 ? (
                  <div className="text-center py-12 text-slate-500 text-xs bg-slate-950/10 border border-slate-850 rounded-2xl">No audit log records for this user.</div>
                ) : (
                  selectedUserAuditLogs.map(log => (
                    <div key={log.id} className="bg-slate-950/40 border border-slate-850/60 rounded-2xl p-3 flex flex-col gap-1.5">
                      <div className="flex justify-between items-center text-[9px] font-mono text-slate-500">
                        <span>{new Date(log.created_at).toLocaleTimeString()}</span>
                        <span>IP: {log.ip_address}</span>
                      </div>
                      <span className="text-[10px] font-extrabold text-blue-400 uppercase tracking-widest">{log.action}</span>
                      <p className="text-[10px] text-slate-400 leading-relaxed">{log.description}</p>
                    </div>
                  ))
                )}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* FULL SYSTEM AUDIT LOG Explorer */}
      <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 shadow-xl">
        <h3 className="font-bold text-white text-base mb-6 flex items-center gap-2">
          <Terminal size={18} className="text-blue-500" /> System-Wide Security Audit Trails
        </h3>
        
        {auditLogs.length === 0 ? (
          <div className="text-center py-16 text-slate-500 text-sm">No audit logs registered in system.</div>
        ) : (
          <div className="overflow-x-auto max-h-[400px]">
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
                  <tr key={log.id} className="border-b border-slate-850/50 text-slate-350 hover:bg-slate-855/10 transition-colors">
                    <td className="py-3.5 pr-4 font-mono text-[10px] text-slate-500">{new Date(log.created_at).toLocaleString()}</td>
                    <td className="py-3.5 pr-4 uppercase font-black text-[9px] tracking-widest text-blue-400">{log.action}</td>
                    <td className="py-3.5 pr-4 font-mono text-[10px] text-slate-400 truncate max-w-[120px]">{log.user_id}</td>
                    <td className="py-3.5 pr-4 text-slate-300 text-xs">{log.description}</td>
                    <td className="py-3.5 font-mono text-[10px] text-slate-500">{log.ip_address}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
};

export default Admin;
