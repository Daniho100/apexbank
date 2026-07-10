import React, { useState } from 'react';
import { useBank } from '../context/BankContext';
import * as bank from '../mockBackend';

const inputClass = "w-full bg-slate-900/80 border border-slate-800 rounded-xl px-4 py-3 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500 transition-all";
const labelClass = "block text-xs font-semibold text-slate-400 uppercase tracking-wider mb-2";
const btnPrimary = "w-full bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-semibold py-3 px-4 rounded-xl transition-all shadow-lg shadow-blue-500/20 active:scale-[0.98] cursor-pointer";

export const FixedDeposits: React.FC = () => {
  const { currentUser, fdList, reloadUserData, showToast } = useBank();
  const [fdAmount, setFdAmount] = useState('');
  const [fdDuration, setFdDuration] = useState('6');

  // Map duration to APR rate
  const getInterestRate = (durationStr: string) => {
    switch (durationStr) {
      case '3': return 12;
      case '6': return 15;
      case '12': return 18;
      default: return 15;
    }
  };

  const currentRate = getInterestRate(fdDuration);

  const estimatedInterest = fdAmount 
    ? parseFloat(fdAmount) * (currentRate / 100) * (parseInt(fdDuration) / 12) 
    : 0;

  const estimatedMaturityValue = fdAmount 
    ? parseFloat(fdAmount) + estimatedInterest 
    : 0;

  const handleCreateFD = (e: React.FormEvent) => {
    e.preventDefault();
    if (!currentUser || !fdAmount) return;
    try {
      const amt = parseFloat(fdAmount);
      const months = parseInt(fdDuration);
      const rate = getInterestRate(fdDuration);
      bank.createFixedDeposit(currentUser.id, amt, months, rate);
      showToast('success', 'Fixed Deposit Certificate generated successfully.');
      setFdAmount('');
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const handleLiquidateFD = (fdId: string) => {
    if (!currentUser) return;
    try {
      bank.earlyWithdrawFixedDeposit(fdId, currentUser.id);
      showToast('success', 'Certificate early liquidated.');
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  return (
    <div className="flex flex-col gap-8">
      {/* FD calculator layout */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
        <div className="lg:col-span-2 bg-slate-900 border border-slate-800 rounded-2xl p-6 flex flex-col justify-between">
          <div>
            <h3 className="font-bold text-white text-lg mb-2">High-Yield Interest Certificates</h3>
            <p className="text-slate-400 text-sm leading-relaxed">
              Open fixed-duration deposit certificates lock selected funds in a secure treasury account earning **up to 18.00% APR** compounding interest.
            </p>
            <p className="text-slate-500 text-xs mt-3 leading-relaxed">
              💡 *Liquidation policy: Early withdrawals before maturity date apply a 50% interest yield reduction penalty.*
            </p>
          </div>
          
          <div className="bg-slate-950/80 p-5 rounded-2xl border border-slate-850 mt-6 flex justify-between items-center">
            <div>
              <span className="text-[10px] text-slate-500 uppercase font-black tracking-wider block">Estimated Compound Interest</span>
              <span className="block text-2xl font-black text-emerald-400 mt-1">
                {estimatedInterest.toLocaleString(undefined, {minimumFractionDigits: 2, maximumFractionDigits: 2})} NGN
              </span>
            </div>
            <div className="text-right">
              <span className="text-[10px] text-slate-500 uppercase font-black tracking-wider block">Payout at Maturity</span>
              <span className="block text-base font-bold text-slate-300 mt-1">
                {estimatedMaturityValue.toLocaleString(undefined, {minimumFractionDigits: 2, maximumFractionDigits: 2})} NGN
              </span>
            </div>
          </div>
        </div>

        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
          <h3 className="font-bold text-white text-base mb-6">Open High-Yield Certificate</h3>
          <form onSubmit={handleCreateFD} className="flex flex-col gap-4">
            <div>
              <label className={labelClass}>Deposit Amount</label>
              <input 
                type="number" 
                value={fdAmount}
                onChange={e => setFdAmount(e.target.value)}
                className={inputClass}
                placeholder="Minimum 10,000 NGN"
              />
            </div>
            <div>
              <label className={labelClass}>Lock Duration</label>
              <select 
                value={fdDuration}
                onChange={e => setFdDuration(e.target.value)}
                className={inputClass}
              >
                <option value="3">3 Months (12.00% APR)</option>
                <option value="6">6 Months (15.00% APR)</option>
                <option value="12">12 Months (18.00% APR)</option>
              </select>
            </div>
            <button type="submit" className={btnPrimary}>Establish Fixed Deposit Lock</button>
          </form>
        </div>
      </div>

      {/* Active certificates */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
        <h3 className="font-bold text-white text-base mb-6">Active Certificates Overview</h3>
        {fdList.length === 0 ? (
          <div className="text-center py-12 text-slate-500 text-sm">No active fixed deposit certificates.</div>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            {fdList.map(fd => (
              <div key={fd.id} className="bg-slate-950/60 border border-slate-855 rounded-2xl p-6 flex flex-col justify-between gap-5 glass-card">
                <div>
                  <div className="flex justify-between items-start">
                    <div>
                      <span className="text-[10px] text-slate-500 font-mono tracking-widest uppercase font-black">{fd.certificate_number}</span>
                      <h4 className="text-xl font-black text-white mt-1">{fd.amount.toLocaleString()} NGN</h4>
                    </div>
                    <span className={`text-[10px] font-black uppercase px-2.5 py-0.5 rounded-full border ${
                      fd.status === 'active' 
                        ? 'bg-blue-950 text-blue-400 border-blue-500/20' 
                        : (fd.status === 'matured' ? 'bg-emerald-950 text-emerald-400 border-emerald-500/20' : 'bg-slate-900 text-slate-500 border-slate-800')
                    }`}>
                      {fd.status.replace('_', ' ')}
                    </span>
                  </div>
                  <div className="grid grid-cols-2 gap-4 mt-6 text-xs border-t border-slate-900 pt-4">
                    <div>
                      <span className="text-slate-500 block text-[9px] uppercase font-black tracking-wider">Interest Rate</span>
                      <span className="text-slate-300 font-semibold mt-0.5 block">{fd.interest_rate}.00% APR</span>
                    </div>
                    <div className="text-right">
                      <span className="text-slate-500 block text-[9px] uppercase font-black tracking-wider">Maturity Date</span>
                      <span className="text-slate-300 font-semibold mt-0.5 block">{new Date(fd.maturity_date).toLocaleDateString()}</span>
                    </div>
                  </div>
                </div>
                {fd.status === 'active' && (
                  <button 
                    onClick={() => handleLiquidateFD(fd.id)}
                    className="w-full py-2.5 text-xs font-bold rounded-xl bg-rose-600/10 hover:bg-rose-600/20 text-rose-400 border border-rose-500/10 transition-colors cursor-pointer"
                  >
                    Liquidate Certificate Early
                  </button>
                )}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

export default FixedDeposits;
