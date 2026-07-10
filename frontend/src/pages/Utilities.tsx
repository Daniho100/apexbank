import React, { useState } from 'react';
import { Phone, Globe, Zap } from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../mockBackend';

const inputClass = "w-full bg-slate-900/80 border border-slate-800 rounded-xl px-4 py-3 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500 transition-all";
const labelClass = "block text-xs font-semibold text-slate-400 uppercase tracking-wider mb-2";
const btnPrimary = "w-full bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-semibold py-3 px-4 rounded-xl transition-all shadow-lg shadow-blue-500/20 active:scale-[0.98] cursor-pointer";
const btnSecondary = "w-full bg-slate-800 hover:bg-slate-700 text-slate-200 font-semibold py-3 px-4 rounded-xl transition-all active:scale-[0.98] cursor-pointer";

export const Utilities: React.FC = () => {
  const { 
    activeAccount, 
    idempotencyKey, 
    regenerateIdempotencyKey, 
    reloadUserData, 
    showToast 
  } = useBank();

  const [utilityType, setUtilityType] = useState<'Airtime' | 'Data' | 'Electricity' | 'CableTV' | 'Betting'>('Airtime');
  const [utilityProvider, setUtilityProvider] = useState('MTN');
  const [utilityRecipient, setUtilityRecipient] = useState('');
  const [utilityAmount, setUtilityAmount] = useState('');
  const [utilityTokenResult, setUtilityTokenResult] = useState<string | null>(null);
  
  const [tokenToValidate, setTokenToValidate] = useState('');
  const [validationResult, setValidationResult] = useState<{ status: string; message: string } | null>(null);

  const handlePayUtility = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!activeAccount) {
      return showToast('error', 'No active account selected.');
    }
    if (!utilityRecipient || !utilityAmount) {
      return showToast('error', 'Please fill in the recipient and amount.');
    }
    try {
      const amt = parseFloat(utilityAmount);
      const res = await bank.payUtility(
        activeAccount.account_number, 
        utilityProvider, 
        utilityRecipient, 
        amt, 
        utilityType, 
        idempotencyKey
      );
      showToast('success', `${utilityType} payment processed.`);
      if (res && res.token) {
        setUtilityTokenResult(res.token);
      } else {
        setUtilityTokenResult(null);
      }
      setUtilityRecipient('');
      setUtilityAmount('');
      regenerateIdempotencyKey();
      await reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const handleValidateToken = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!tokenToValidate) return;
    try {
      const res = await bank.validatePrepaidToken(tokenToValidate);
      setValidationResult(res);
      if (res && res.status === 'success') {
        showToast('success', 'Prepaid units uploaded.');
      } else {
        showToast('error', res ? res.message : 'Validation failed.');
      }
    } catch (err: any) {
      showToast('error', err.message);
    }
    setTokenToValidate('');
  };

  return (
    <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
      {/* Utility portal */}
      <div className="lg:col-span-2 bg-slate-900 border border-slate-800 rounded-2xl p-6">
        <h3 className="font-bold text-white text-base mb-6">Purchase Utility Prepaid Vouchers</h3>
        
        {/* Tabs */}
        <div className="grid grid-cols-3 gap-3 mb-6">
          <button 
            onClick={() => { setUtilityType('Airtime'); setUtilityProvider('MTN'); }}
            className={`p-3 rounded-xl border flex flex-col items-center gap-2 text-xs font-semibold cursor-pointer ${
              utilityType === 'Airtime' ? 'bg-blue-600/10 border-blue-500/30 text-blue-400' : 'bg-slate-950/60 border-slate-850 text-slate-500'
            }`}
          >
            <Phone size={18} /> Airtime Voucher
          </button>
          <button 
            onClick={() => { setUtilityType('Data'); setUtilityProvider('MTN'); }}
            className={`p-3 rounded-xl border flex flex-col items-center gap-2 text-xs font-semibold cursor-pointer ${
              utilityType === 'Data' ? 'bg-blue-600/10 border-blue-500/30 text-blue-400' : 'bg-slate-950/60 border-slate-850 text-slate-500'
            }`}
          >
            <Globe size={18} /> Data Bundle
          </button>
          <button 
            onClick={() => { setUtilityType('Electricity'); setUtilityProvider('PrepaidMeter'); }}
            className={`p-3 rounded-xl border flex flex-col items-center gap-2 text-xs font-semibold cursor-pointer ${
              utilityType === 'Electricity' ? 'bg-blue-600/10 border-blue-500/30 text-blue-400' : 'bg-slate-950/60 border-slate-850 text-slate-500'
            }`}
          >
            <Zap size={18} /> Electric Prepaid
          </button>
        </div>

        <form onSubmit={handlePayUtility} className="flex flex-col gap-5">
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            <div>
              <label className={labelClass}>Vendor Provider</label>
              {utilityType === 'Electricity' ? (
                <input type="text" value="AEDC/EKEDC (Prepaid)" disabled className={`${inputClass} bg-slate-950/50`} />
              ) : (
                <select 
                  value={utilityProvider}
                  onChange={e => setUtilityProvider(e.target.value)}
                  className={inputClass}
                >
                  <option value="MTN">MTN Nigeria</option>
                  <option value="Airtel">Airtel Nigeria</option>
                  <option value="Glo">Glo Nigeria</option>
                  <option value="9mobile">9mobile Nigeria</option>
                </select>
              )}
            </div>
            <div>
              <label className={labelClass}>Recipient Number / Meter No</label>
              <input 
                type="text" 
                value={utilityRecipient}
                onChange={e => setUtilityRecipient(e.target.value)}
                className={inputClass}
                placeholder={utilityType === 'Electricity' ? "Prepaid Meter Number" : "Phone Number (e.g. 080...)"}
              />
            </div>
          </div>

          <div>
            <label className={labelClass}>Purchase Amount (NGN)</label>
            <input 
              type="number" 
              value={utilityAmount}
              onChange={e => setUtilityAmount(e.target.value)}
              className={inputClass}
              placeholder="e.g. 3,500"
            />
          </div>

          <button type="submit" className={btnPrimary}>Post Utility Payout</button>
        </form>

        {utilityTokenResult && (
          <div className="bg-slate-950 border border-blue-500/20 p-5 rounded-2xl mt-6 flex flex-col gap-3">
            <span className="text-[10px] text-blue-400 font-black tracking-widest uppercase block text-center">Generated Prepaid Meter Token</span>
            <span className="block text-2xl font-black text-white font-mono text-center tracking-wider bg-slate-900 py-3 rounded-xl border border-slate-800">{utilityTokenResult}</span>
            <p className="text-slate-500 text-center text-[10px] leading-normal">
              Copy this 20-digit prepaid meter token and load it in the meter charging simulator on the right.
            </p>
          </div>
        )}
      </div>

      {/* Meter simulator */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 h-fit">
        <h3 className="font-bold text-white text-base mb-6">Meter Charger Mock Simulator</h3>
        <form onSubmit={handleValidateToken} className="flex flex-col gap-4">
          <div>
            <label className={labelClass}>Prepaid Voucher Code</label>
            <input 
              type="text" 
              value={tokenToValidate}
              onChange={e => setTokenToValidate(e.target.value)}
              className={`${inputClass} font-mono text-center`}
              placeholder="xxxx-xxxx-xxxx-xxxx-xxxx"
            />
          </div>
          <button type="submit" className={btnSecondary}>Upload Voucher Token</button>
        </form>

        {validationResult && (
          <div className={`mt-5 p-4 rounded-xl border flex flex-col gap-2 ${
            validationResult.status === 'success' ? 'bg-emerald-950/40 border-emerald-500/25 text-emerald-450' : 'bg-rose-950/40 border-rose-500/25 text-rose-450'
          }`}>
            <span className="font-bold text-xs uppercase tracking-widest">{validationResult.status}</span>
            <p className="text-[11px] leading-relaxed text-slate-300">{validationResult.message}</p>
          </div>
        )}
      </div>
    </div>
  );
};

export default Utilities;
