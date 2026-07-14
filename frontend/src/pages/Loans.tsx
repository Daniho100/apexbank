import React, { useState, useMemo } from 'react';
import { AlertCircle } from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../api/bankApi';

const inputClass = "w-full bg-slate-900/80 border border-slate-800 rounded-xl px-4 py-3 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500 transition-all";
const labelClass = "block text-xs font-semibold text-slate-400 uppercase tracking-wider mb-2";
const btnPrimary = "w-full bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-semibold py-3 px-4 rounded-xl transition-all shadow-lg shadow-blue-500/20 active:scale-[0.98] cursor-pointer";

export const Loans: React.FC = () => {
  const { 
    currentUser, 
    activeAccount, 
    loansList, 
    loanSchedules, 
    idempotencyKey, 
    regenerateIdempotencyKey, 
    reloadUserData, 
    showToast 
  } = useBank();

  const [loanAmount, setLoanAmount] = useState('');
  const [loanDuration, setLoanDuration] = useState('12');
  const [loanName, setLoanName] = useState('');
  const [activeLoanId, setActiveLoanId] = useState('');
  const [loanRepayAmount, setLoanRepayAmount] = useState('');

  const hasDefaultedLoan = useMemo(() => {
    return loansList.some(l => l.status === 'defaulted');
  }, [loansList]);

  const getLoanRate = (durationStr: string) => {
    switch (durationStr) {
      case '6': return 10;
      case '12': return 12;
      case '24': return 15;
      default: return 12;
    }
  };

  const handleApplyLoan = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!currentUser || !loanAmount) return;
    if (hasDefaultedLoan) {
      return showToast('error', 'Loan application blocked due to overdue default.');
    }
    try {
      const amt = parseFloat(loanAmount);
      const months = parseInt(loanDuration);
      const rate = getLoanRate(loanDuration);
      await bank.applyLoan(currentUser.id, amt, months, rate, loanName || 'Personal Loan');
      showToast('success', 'Loan request submitted for processing.');
      setLoanAmount('');
      setLoanName('');
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const handleRepayLoan = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!activeAccount || !activeLoanId || !loanRepayAmount) {
      return showToast('error', 'Please select a loan and enter a repayment amount.');
    }
    try {
      const amt = parseFloat(loanRepayAmount);
      await bank.manualRepayLoan(activeLoanId, activeAccount.account_number, amt, idempotencyKey);
      showToast('success', `Loan repayment of ${amt.toLocaleString()} NGN posted.`);
      setLoanRepayAmount('');
      regenerateIdempotencyKey();
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  return (
    <div className="flex flex-col gap-8">
      {/* Loan input forms */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        {/* Apply */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
          <h3 className="font-bold text-white text-base mb-6">Request Loan</h3>
          {hasDefaultedLoan && (
            <div className="mb-4 p-4 rounded-xl border border-rose-500/20 bg-rose-950/20 text-rose-400 text-xs flex items-center gap-2">
              <AlertCircle size={16} />
              <span>Credit operations are locked until all overdue loan schedules are settled.</span>
            </div>
          )}
          <form onSubmit={handleApplyLoan} className="flex flex-col gap-4">
            <div>
              <label className={labelClass}>Principal Borrow Amount</label>
              <input 
                type="number" 
                value={loanAmount}
                onChange={e => setLoanAmount(e.target.value)}
                className={inputClass}
                placeholder="e.g. 250,000 NGN"
                disabled={hasDefaultedLoan}
              />
            </div>
            <div>
              <label className={labelClass}>Loan Purpose / Name</label>
              <input 
                type="text" 
                value={loanName}
                onChange={e => setLoanName(e.target.value)}
                className={inputClass}
                placeholder="e.g. Home Reno, Business Expansion"
                disabled={hasDefaultedLoan}
              />
            </div>
            <div>
              <label className={labelClass}>Loan Duration</label>
              <select 
                value={loanDuration}
                onChange={e => setLoanDuration(e.target.value)}
                className={inputClass}
                disabled={hasDefaultedLoan}
              >
                <option value="6">6 Months (10% Interest)</option>
                <option value="12">12 Months (12% Interest)</option>
                <option value="24">24 Months (15% Interest)</option>
              </select>
            </div>
            <button 
              type="submit" 
              className={btnPrimary} 
              disabled={hasDefaultedLoan || !loanAmount}
            >
              Submit Credit Request
            </button>
          </form>
        </div>

        {/* Repay */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
          <h3 className="font-bold text-white text-base mb-6">Installment Payment</h3>
          <form onSubmit={handleRepayLoan} className="flex flex-col gap-4">
            <div>
              <label className={labelClass}>Select Credit Profile</label>
              <select 
                value={activeLoanId}
                onChange={e => setActiveLoanId(e.target.value)}
                className={inputClass}
              >
                <option value="">Select loan to pay</option>
                {loansList.filter(l => l.status === 'active' || l.status === 'defaulted').map(l => (
                  <option key={l.id} value={l.id}>
                    {l.name || 'Personal Loan'} ({l.reference_number || l.id.slice(0,8)}) | Outstanding: {l.outstanding_balance.toLocaleString()} NGN
                  </option>
                ))}
              </select>
            </div>
            <div>
              <label className={labelClass}>Repayment Amount (NGN)</label>
              <input 
                type="number" 
                value={loanRepayAmount}
                onChange={e => setLoanRepayAmount(e.target.value)}
                className={inputClass}
                placeholder="e.g. 10,000"
              />
            </div>
            <button type="submit" className={btnPrimary} disabled={!activeLoanId || !loanRepayAmount}>
              Repay
            </button>
          </form>
        </div>
      </div>

      {/* Schedules lists */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
        <h3 className="font-bold text-white text-base mb-6">Amortization Payment Schedules</h3>
        {loansList.length === 0 ? (
          <div className="text-center py-12 text-slate-500 text-sm">No credit lines registered.</div>
        ) : (
          <div className="flex flex-col gap-8">
            {loansList.map(loan => (
              <div key={loan.id} className="border border-slate-800 rounded-2xl p-5 bg-slate-950/40 flex flex-col gap-4">
                <div className="flex justify-between items-center border-b border-slate-850 pb-4">
                  <div>
                    <span className="text-[10px] text-slate-500 font-mono block">
                      CREDIT LINE: {loan.name || 'Personal Loan'} ({loan.reference_number || loan.id})
                    </span>
                    <span className="text-base font-bold text-slate-200 mt-1 block">
                      Remaining Balance: {loan.outstanding_balance.toLocaleString()} NGN (Principal: {loan.amount.toLocaleString()} NGN)
                    </span>
                  </div>
                  <span className={`text-[10px] uppercase font-black px-2.5 py-0.5 rounded-full border ${
                    loan.status === 'active' 
                      ? 'bg-blue-950 text-blue-400 border-blue-500/20' 
                      : (loan.status === 'completed' 
                        ? 'bg-emerald-950 text-emerald-400 border-emerald-500/20' 
                        : (loan.status === 'defaulted' ? 'bg-rose-950 text-rose-450 border-rose-500/20 border-dashed animate-pulse' : 'bg-slate-900 text-slate-550 border-slate-800'))
                  }`}>
                    {loan.status}
                  </span>
                </div>
                <div className="overflow-x-auto">
                  <table className="w-full text-xs text-left text-slate-400">
                    <thead>
                      <tr className="border-b border-slate-900 uppercase text-slate-500 tracking-wider">
                        <th className="pb-2">Installment No</th>
                        <th className="pb-2">Scheduled Amount</th>
                        <th className="pb-2">Principal Due</th>
                        <th className="pb-2">Interest Portion</th>
                        <th className="pb-2">Paid to Date</th>
                        <th className="pb-2">Due Date</th>
                        <th className="pb-2 text-center">Status</th>
                      </tr>
                    </thead>
                    <tbody>
                      {loanSchedules.filter(s => s.loan_id === loan.id).map(s => (
                        <tr key={s.id} className="border-b border-slate-900/60 text-slate-300">
                          <td className="py-3 font-semibold">Month {s.installment_number}</td>
                          <td className="py-3">{s.amount_due.toLocaleString()} NGN</td>
                          <td className="py-3">{s.principal_due.toLocaleString()} NGN</td>
                          <td className="py-3">{s.interest_due.toLocaleString()} NGN</td>
                          <td className="py-3 font-bold text-emerald-400">{s.amount_paid.toLocaleString()} NGN</td>
                          <td className="py-3 text-slate-500">{new Date(s.due_date).toLocaleDateString()}</td>
                          <td className="py-3 text-center">
                            <span className={`px-2 py-0.5 rounded-full text-[9px] font-black uppercase ${
                              s.status === 'paid' 
                                ? 'bg-emerald-950 text-emerald-450' 
                                : (s.status === 'overdue' ? 'bg-rose-950 text-rose-450 animate-pulse border border-rose-500/20' : 'bg-slate-800 text-slate-400')
                            }`}>
                              {s.status}
                            </span>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

export default Loans;
