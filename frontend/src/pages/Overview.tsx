import React, { useState, useMemo } from 'react';
import { 
  DollarSign, FileText, Plus, AlertCircle, CheckCircle, ShieldAlert, ShieldCheck 
} from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../api/bankApi';

const inputClass = "w-full bg-slate-950/60 border border-slate-800 rounded-xl px-4 py-2.5 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500 transition-all text-xs";
const labelClass = "block text-xs font-semibold text-slate-400 uppercase tracking-wider mb-2";
const btnPrimary = "w-full bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-semibold py-3 px-4 rounded-xl transition-all shadow-lg shadow-blue-500/20 active:scale-[0.98] cursor-pointer";

export const Overview: React.FC = () => {
  const { 
    userAccounts, 
    activeAccount, 
    setActiveAccount, 
    transactions, 
    loansList, 
    idempotencyKey, 
    regenerateIdempotencyKey, 
    reloadUserData, 
    showToast,
    isIdentityVerified,
    verifyIdentity
  } = useBank();

  const [depositAmount, setDepositAmount] = useState('');
  const [withdrawAmount, setWithdrawAmount] = useState('');
  
  // Local verification form states
  const [bvnOrNin, setBvnOrNin] = useState('');
  const [verificationError, setVerificationError] = useState<string | null>(null);

  const hasDefaultedLoan = useMemo(() => {
    return loansList.some(l => l.status === 'defaulted');
  }, [loansList]);

  const handleDeposit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!activeAccount || !depositAmount) return;
    try {
      const amt = parseFloat(depositAmount);
      await bank.processDeposit(activeAccount.account_number, amt, idempotencyKey, "Cash deposit clearance");
      showToast('success', `Cash deposit of ${amt.toLocaleString()} NGN processed.`);
      setDepositAmount('');
      regenerateIdempotencyKey();
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const handleWithdraw = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!activeAccount || !withdrawAmount) return;
    try {
      const amt = parseFloat(withdrawAmount);
      await bank.processWithdrawal(activeAccount.account_number, amt, idempotencyKey, "ATM Withdrawal");
      showToast('success', `Withdrawal of ${amt.toLocaleString()} NGN completed.`);
      setWithdrawAmount('');
      regenerateIdempotencyKey();
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const handleVerifySubmit = (e: React.FormEvent) => {
    e.preventDefault();
    setVerificationError(null);
    const cleanNum = bvnOrNin.replace(/\D/g, '');
    if (cleanNum.length !== 11) {
      setVerificationError('Identification must be exactly 11 numeric digits.');
      return;
    }
    const success = verifyIdentity(cleanNum);
    if (success) {
      showToast('success', 'Profile identity verification succeeded.');
      setBvnOrNin('');
    } else {
      setVerificationError('Failed to process verification. Please retry.');
    }
  };

  return (
    <div className="flex flex-col gap-8">
      {/* Top dashboard widgets */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        
        {/* Ledger Account Card */}
        {userAccounts.slice(0, 2).map((acc, index) => (
          <div 
            key={acc.id}
            onClick={() => setActiveAccount(acc)}
            className={`rounded-3xl p-6 relative overflow-hidden flex flex-col justify-between aspect-[1.6/1] border shadow-2xl transition-all cursor-pointer ${
              activeAccount?.id === acc.id 
                ? 'ring-2 ring-blue-500 scale-[1.02]' 
                : 'hover:scale-[1.01]'
            } ${
              index === 0 
                ? 'bg-gradient-to-br from-blue-700 via-indigo-800 to-slate-900 border-blue-500/20' 
                : 'bg-gradient-to-br from-slate-800 via-slate-900 to-indigo-950 border-slate-700/20'
            }`}
          >
            <div className="absolute top-0 right-0 w-32 h-32 bg-white/5 rounded-full blur-2xl"></div>
            
            <div className="flex justify-between items-start z-10">
              <div>
                <span className="text-[10px] font-bold text-slate-300 uppercase tracking-widest">{acc.type} card</span>
                <span className="block text-white font-mono text-sm tracking-widest mt-1">
                  **** **** **** {acc.account_number.slice(-4)}
                </span>
              </div>
              <span className="bg-white/10 text-white text-[9px] font-black uppercase px-2 py-0.5 rounded-lg tracking-wider border border-white/10">
                {acc.currency}
              </span>
            </div>

            {/* Display full account number with copy option */}
            <div className="z-10 flex items-center justify-between bg-slate-950/50 px-3 py-1.5 rounded-xl border border-white/5">
              <span className="font-mono text-[11px] text-slate-300 tracking-wider">
                ACC: {acc.account_number}
              </span>
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  navigator.clipboard.writeText(acc.account_number);
                  showToast('success', `Account number ${acc.account_number} copied to clipboard!`);
                }}
                className="text-[9px] text-blue-450 hover:text-blue-300 font-bold bg-blue-500/10 hover:bg-blue-500/20 px-2 py-1 rounded-lg border border-blue-500/20 transition-all cursor-pointer"
              >
                Copy
              </button>
            </div>

            <div className="z-10">
              <span className="text-slate-300 text-[10px] uppercase font-bold tracking-wider">Available Balance</span>
              <h3 className="text-2xl font-black text-white mt-1 leading-none">
                {acc.balance.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
              </h3>
            </div>
          </div>
        ))}

        {/* Identity Verification Dashboard widget */}
        <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 flex flex-col justify-between min-h-[170px]">
          <div className="flex items-center justify-between border-b border-slate-850 pb-3 mb-2">
            <span className="text-[10px] text-slate-500 uppercase font-black tracking-wider">Identity Profile Status</span>
            {isIdentityVerified ? (
              <span className="px-2.5 py-0.5 rounded-full bg-emerald-500/10 border border-emerald-500/30 text-emerald-400 font-extrabold text-[9px] uppercase tracking-wider flex items-center gap-1">
                <ShieldCheck size={12} /> Verified
              </span>
            ) : (
              <span className="px-2.5 py-0.5 rounded-full bg-rose-500/10 border border-rose-500/30 text-rose-450 font-extrabold text-[9px] uppercase tracking-wider flex items-center gap-1">
                <ShieldAlert size={12} /> Unverified
              </span>
            )}
          </div>

          {isIdentityVerified ? (
            <div className="flex-1 flex flex-col justify-center text-center py-4">
              <CheckCircle size={24} className="text-emerald-400 mx-auto mb-2" />
              <p className="text-xs text-slate-300 font-bold">BVN/NIN Authenticated</p>
              <p className="text-[10px] text-slate-500 mt-1 leading-relaxed">
                All daily transfer limits and credit allocations unlocked.
              </p>
            </div>
          ) : (
            <form onSubmit={handleVerifySubmit} className="flex-1 flex flex-col justify-between gap-3">
              <div>
                <p className="text-[10px] text-slate-400 leading-normal">
                  Verify your 11-digit BVN or NIN to authorize transfers over N50,000.
                </p>
                {verificationError && (
                  <p className="text-[10px] text-rose-450 mt-1">{verificationError}</p>
                )}
              </div>
              <div className="flex gap-2 items-center">
                <input 
                  type="text" 
                  value={bvnOrNin}
                  onChange={e => setBvnOrNin(e.target.value.slice(0, 11))}
                  className={inputClass}
                  placeholder="Enter 11-digit BVN/NIN"
                  maxLength={11}
                />
                <button 
                  type="submit" 
                  className="bg-blue-600 hover:bg-blue-500 text-white text-[10px] font-bold py-2.5 px-3.5 rounded-xl cursor-pointer transition-colors shrink-0"
                >
                  Verify
                </button>
              </div>
            </form>
          )}
        </div>

        {/* Small Quick-Stats Card */}
        {userAccounts.length < 2 && (
          <div className="bg-slate-900 border border-slate-800 rounded-3xl p-6 flex flex-col justify-between">
            <div>
              <span className="text-[10px] text-slate-500 uppercase font-black tracking-wider block">Eligible credit lines</span>
              {hasDefaultedLoan ? (
                <span className="text-rose-500 text-xs font-bold mt-2 flex items-center gap-1.5"><AlertCircle size={14} /> Blocked - Overdue balance</span>
              ) : (
                <span className="text-emerald-400 text-xs font-bold mt-2 flex items-center gap-1.5"><CheckCircle size={14} /> Clear - Verified Account</span>
              )}
            </div>
            <div className="text-xs text-slate-400 mt-4 leading-relaxed">
              Your account status is monitored by security algorithms. Prompt loan payments ensure high eligibility indices.
            </div>
          </div>
        )}
      </div>

      {/* Desktop layout for cash deposits/withdrawals */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        {/* Deposit Card */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
          <div className="flex items-center gap-3 mb-6">
            <div className="w-10 h-10 rounded-xl bg-blue-600/10 border border-blue-500/20 flex items-center justify-center text-blue-400">
              <Plus size={20} />
            </div>
            <div>
              <h3 className="font-bold text-white text-base">Clear Cash Deposit</h3>
              <p className="text-slate-500 text-xs mt-0.5">Post funds directly to your ledger wallet</p>
            </div>
          </div>
          <form onSubmit={handleDeposit} className="flex flex-col gap-4">
            <div>
              <label className={labelClass}>Amount (NGN)</label>
              <input 
                type="number" 
                value={depositAmount}
                onChange={e => setDepositAmount(e.target.value)}
                className={inputClass}
                placeholder="e.g. 50,000"
              />
            </div>
            <button type="submit" className={btnPrimary}>Clear Cash Deposit</button>
          </form>
        </div>

        {/* Withdrawal Card */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
          <div className="flex items-center gap-3 mb-6">
            <div className="w-10 h-10 rounded-xl bg-rose-600/10 border border-rose-500/20 flex items-center justify-center text-rose-400">
              <DollarSign size={20} />
            </div>
            <div>
              <h3 className="font-bold text-white text-base">ATM Cashout Withdrawal</h3>
              <p className="text-slate-500 text-xs mt-0.5">Deduct cash instantly from your ledger balance</p>
            </div>
          </div>
          <form onSubmit={handleWithdraw} className="flex flex-col gap-4">
            <div>
              <label className={labelClass}>Amount (NGN)</label>
              <input 
                type="number" 
                value={withdrawAmount}
                onChange={e => setWithdrawAmount(e.target.value)}
                className={inputClass}
                placeholder="e.g. 10,000"
              />
            </div>
            <button type="submit" className="w-full bg-rose-600 hover:bg-rose-500 text-white font-semibold py-3 px-4 rounded-xl transition-all shadow-lg active:scale-[0.98] cursor-pointer text-sm">
              Execute ATM Cashout
            </button>
          </form>
        </div>
      </div>

      {/* Table View Transaction statement */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
        <div className="flex justify-between items-center mb-6">
          <h3 className="font-bold text-white text-base flex items-center gap-2">
            <FileText size={18} className="text-blue-500" /> Transaction Ledger Statements
          </h3>
          <span className="text-[10px] text-slate-500 font-mono tracking-widest uppercase">Real-time ledger data</span>
        </div>

        {transactions.length === 0 ? (
          <div className="text-center py-12 text-slate-500 text-sm">No transaction ledger records found.</div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm text-left text-slate-300">
              <thead>
                <tr className="border-b border-slate-800 text-xs text-slate-500 uppercase tracking-wider">
                  <th className="pb-3 pr-4">Reference No</th>
                  <th className="pb-3 pr-4">Transaction Type</th>
                  <th className="pb-3 pr-4">Description</th>
                  <th className="pb-3 pr-4 text-right">Amount</th>
                  <th className="pb-3 text-center">Status</th>
                </tr>
              </thead>
              <tbody>
                {transactions.map(t => {
                  const isIncoming = activeAccount 
                    ? t.receiver_account_id === activeAccount.id 
                    : (t.type === 'deposit' || t.type === 'loan_disbursement');
                  return (
                    <tr key={t.id} className="border-b border-slate-850 hover:bg-slate-855/20 transition-colors">
                      <td className="py-4 pr-4 font-mono text-xs text-slate-400">{t.reference_number}</td>
                      <td className="py-4 pr-4 uppercase font-bold text-[10px] tracking-wider">
                        <span className={`px-2 py-0.5 rounded-full border ${
                          isIncoming 
                            ? 'bg-emerald-950/40 border-emerald-500/20 text-emerald-400' 
                            : 'bg-slate-950/40 border-slate-800 text-slate-400'
                        }`}>
                          {t.type.replace('_', ' ')}
                        </span>
                      </td>
                      <td className="py-4 pr-4 text-slate-400 text-xs">{t.description}</td>
                      <td className={`py-4 pr-4 text-right font-black ${
                        isIncoming ? 'text-emerald-400' : 'text-rose-400'
                      }`}>
                        {isIncoming ? '+' : '-'} {t.amount.toLocaleString()} NGN
                      </td>
                      <td className="py-4 text-center">
                        <span className="text-[10px] bg-blue-950 text-blue-400 border border-blue-500/10 px-2 py-0.5 rounded-full font-bold">
                          {t.status}
                        </span>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
};

export default Overview;
