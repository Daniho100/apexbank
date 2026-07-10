import React, { useState, useMemo } from 'react';
import { useBank } from '../context/BankContext';
import * as bank from '../mockBackend';
import { motion, AnimatePresence } from 'framer-motion';
import { ShieldAlert, ArrowRight, X, User, Copy } from 'lucide-react';

const inputClass = "w-full bg-slate-900 border border-slate-800 rounded-xl px-4 py-3 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500 transition-all text-sm";
const labelClass = "block text-xs font-semibold text-slate-400 uppercase tracking-wider mb-2";
const btnPrimary = "w-full bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-semibold py-3.5 px-4 rounded-xl transition-all shadow-lg shadow-blue-500/20 active:scale-[0.98] flex items-center justify-center gap-2 cursor-pointer text-sm font-bold border border-blue-500/10";
const btnSecondary = "w-full bg-slate-800 hover:bg-slate-700 text-slate-200 font-semibold py-3.5 px-4 rounded-xl transition-all active:scale-[0.98] cursor-pointer text-sm";

export const Transfers: React.FC = () => {
  const { 
    currentUser,
    activeAccount, 
    idempotencyKey, 
    regenerateIdempotencyKey, 
    reloadUserData, 
    showToast,
    isIdentityVerified,
    verifyIdentity,
    transactions 
  } = useBank();

  const [transferTarget, setTransferTarget] = useState('');
  const [transferAmount, setTransferAmount] = useState('');
  const [transferDesc, setTransferDesc] = useState('');

  // Gating verification modal states
  const [showVerifyModal, setShowVerifyModal] = useState(false);
  const [modalBvnOrNin, setModalBvnOrNin] = useState('');
  const [modalError, setModalError] = useState<string | null>(null);

  // Directory of other same-bank users who are saved beneficiaries
  const directoryEntries = useMemo(() => {
    if (!currentUser) return [];
    try {
      const users = bank.getStorage<bank.User[]>('users', []);
      const accounts = bank.getStorage<bank.Account[]>('accounts', []);
      
      const storageKey = `beneficiaries_${currentUser.id}`;
      const beneficiaryAccs = JSON.parse(localStorage.getItem(storageKey) || '[]');
      
      // Filter only accounts whose numbers exist in the user's saved beneficiaries list
      return accounts
        .filter(acc => beneficiaryAccs.includes(acc.account_number))
        .map(acc => {
          const user = users.find(u => u.id === acc.user_id);
          return {
            accountNumber: acc.account_number,
            email: user ? user.email : 'External Account',
            type: acc.type,
            status: acc.status
          };
        });
    } catch (e) {
      return [];
    }
  }, [currentUser, transactions]);

  const executeTransferAction = () => {
    if (!currentUser || !activeAccount || !transferTarget || !transferAmount) return;
    try {
      const amt = parseFloat(transferAmount);
      bank.processTransfer(
        activeAccount.account_number, 
        transferTarget, 
        amt, 
        idempotencyKey, 
        transferDesc || "Internal Transfer"
      );
      
      // Auto-save recipient as beneficiary after successful transfer
      const storageKey = `beneficiaries_${currentUser.id}`;
      const beneficiaries = JSON.parse(localStorage.getItem(storageKey) || '[]');
      if (!beneficiaries.includes(transferTarget)) {
        beneficiaries.push(transferTarget);
        localStorage.setItem(storageKey, JSON.stringify(beneficiaries));
      }

      showToast('success', `Transferred ${amt.toLocaleString()} NGN successfully.`);
      setTransferAmount('');
      setTransferTarget('');
      setTransferDesc('');
      regenerateIdempotencyKey();
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const handleTransferSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!activeAccount) {
      return showToast('error', 'No active account selected. Go to Overview to select an account.');
    }
    if (!transferTarget || !transferAmount) {
      return showToast('error', 'Please enter a recipient account and transfer amount.');
    }
    
    const amt = parseFloat(transferAmount);
    if (isNaN(amt) || amt <= 0) {
      return showToast('error', 'Please enter a valid transfer amount.');
    }

    // Gating trigger: if transfer > 50,000 NGN and identity is unverified, trigger BVN/NIN prompt
    if (amt > 50000 && !isIdentityVerified) {
      setModalBvnOrNin('');
      setModalError(null);
      setShowVerifyModal(true);
      return;
    }

    executeTransferAction();
  };

  const handleModalVerifySubmit = (e: React.FormEvent) => {
    e.preventDefault();
    setModalError(null);
    const cleanNum = modalBvnOrNin.replace(/\D/g, '');
    if (cleanNum.length !== 11) {
      setModalError('BVN or NIN must be exactly 11 numeric digits.');
      return;
    }

    const verified = verifyIdentity(cleanNum);
    if (verified) {
      showToast('success', 'Identity authenticated successfully. Daily transfer limits updated.');
      setShowVerifyModal(false);
      // Automatically proceed to finish the transfer
      executeTransferAction();
    } else {
      setModalError('Verification failed. Please check the input.');
    }
  };

  const handleAutofillRecipient = (accNo: string) => {
    setTransferTarget(accNo);
    showToast('success', `Autofilled beneficiary: ${accNo}`);
  };

  return (
    <div className="relative">
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
        
        {/* Left Form Column */}
        <div className="lg:col-span-2 bg-slate-900 border border-slate-800 rounded-2xl p-8 w-full">
          <div className="mb-6 border-b border-slate-850 pb-4">
            <h3 className="font-bold text-white text-lg">Direct Internal Fund Transfers</h3>
            <p className="text-slate-500 text-xs mt-1">Transfers post dual credit/debit records to database journal sheets under transaction locks.</p>
          </div>

          <form onSubmit={handleTransferSubmit} className="flex flex-col gap-6">
            <div>
              <label className={labelClass}>Recipient Account Number</label>
              <input 
                type="text" 
                value={transferTarget}
                onChange={e => setTransferTarget(e.target.value)}
                className={inputClass}
                placeholder="e.g. 104829103"
              />
            </div>

            <div>
              <label className={labelClass}>Transfer Amount (NGN)</label>
              <input 
                type="number" 
                value={transferAmount}
                onChange={e => setTransferAmount(e.target.value)}
                className={inputClass}
                placeholder="e.g. 20,000"
              />
            </div>

            <div>
              <label className={labelClass}>Description / Memo</label>
              <input 
                type="text" 
                value={transferDesc}
                onChange={e => setTransferDesc(e.target.value)}
                className={inputClass}
                placeholder="Reason for payment"
              />
            </div>

            <button type="submit" className={btnPrimary}>
              Post Internal Transfer <ArrowRight size={16} />
            </button>
          </form>
        </div>

        {/* Right Directory Column: shows beneficiaries only */}
        <div className="lg:col-span-1 bg-slate-900 border border-slate-800 rounded-2xl p-6 h-fit">
          <div className="border-b border-slate-850 pb-3 mb-4">
            <h4 className="font-bold text-white text-sm">My Saved Beneficiaries</h4>
            <p className="text-[10px] text-slate-500 mt-1 leading-normal">
              Accounts will be automatically saved here as beneficiaries after your first successful transfer to them.
            </p>
          </div>

          <div className="flex flex-col gap-3 max-h-[350px] overflow-y-auto pr-1">
            {directoryEntries.length === 0 ? (
              <div className="text-center py-8 text-slate-600 text-xs">
                No saved beneficiaries. Complete a transfer to any user to save them here automatically.
              </div>
            ) : (
              directoryEntries.map((entry) => (
                <div 
                  key={entry.accountNumber}
                  onClick={() => handleAutofillRecipient(entry.accountNumber)}
                  className="bg-slate-950/50 hover:bg-slate-950 border border-slate-850 hover:border-blue-500/35 p-3 rounded-xl flex items-center justify-between cursor-pointer transition-all hover:scale-[1.01] group"
                >
                  <div className="flex items-center gap-3 overflow-hidden">
                    <div className="w-8 h-8 rounded-lg bg-blue-500/10 border border-blue-500/20 flex items-center justify-center text-blue-400 shrink-0">
                      <User size={14} />
                    </div>
                    <div className="overflow-hidden">
                      <span className="block text-[11px] font-bold text-slate-300 truncate">{entry.email}</span>
                      <span className="block text-[10px] font-mono text-slate-500 mt-0.5 tracking-wider">{entry.accountNumber}</span>
                    </div>
                  </div>

                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      navigator.clipboard.writeText(entry.accountNumber);
                      showToast('success', `Copied account number: ${entry.accountNumber}`);
                    }}
                    className="p-1.5 rounded-lg bg-slate-900 border border-slate-850 hover:border-slate-700 text-slate-500 hover:text-white transition-colors cursor-pointer"
                    title="Copy Account Number"
                  >
                    <Copy size={12} />
                  </button>
                </div>
              ))
            )}
          </div>
        </div>

      </div>

      {/* Identity Verification Gating Overlay Modal */}
      <AnimatePresence>
        {showVerifyModal && (
          <div className="fixed inset-0 z-50 flex items-center justify-center p-4">
            {/* Backdrop overlay */}
            <motion.div 
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              onClick={() => setShowVerifyModal(false)}
              className="absolute inset-0 bg-black/70 backdrop-blur-sm"
            ></motion.div>

            {/* Modal Card */}
            <motion.div 
              initial={{ opacity: 0, scale: 0.95, y: 20 }}
              animate={{ opacity: 1, scale: 1, y: 0 }}
              exit={{ opacity: 0, scale: 0.95, y: 20 }}
              className="glass-card max-w-sm w-full p-6 rounded-3xl border border-slate-800 shadow-2xl relative z-50 text-center flex flex-col gap-5"
            >
              <div className="flex justify-between items-center border-b border-slate-850 pb-3">
                <span className="text-[10px] text-slate-500 font-mono uppercase tracking-widest block text-left">Security Verification</span>
                <button 
                  onClick={() => setShowVerifyModal(false)}
                  className="p-1 rounded-lg hover:bg-slate-800 text-slate-500 hover:text-white transition-colors cursor-pointer"
                >
                  <X size={16} />
                </button>
              </div>

              <div className="flex justify-center text-rose-455">
                <ShieldAlert size={40} className="animate-pulse" />
              </div>

              <div>
                <h4 className="font-bold text-white text-sm">High-Value Transfer Limit Gate</h4>
                <p className="text-slate-400 text-[11px] leading-relaxed mt-2">
                  You are attempting to transfer **NGN {parseFloat(transferAmount).toLocaleString()}**. Transactions above 50,000 NGN require account authentication.
                </p>
              </div>

              <form onSubmit={handleModalVerifySubmit} className="flex flex-col gap-4 text-left">
                <div>
                  <label className="block text-[10px] font-semibold text-slate-400 uppercase mb-2">BVN or NIN (11 digits)</label>
                  <input 
                    type="text" 
                    value={modalBvnOrNin}
                    onChange={e => setModalBvnOrNin(e.target.value.slice(0, 11))}
                    placeholder="Enter 11-digit BVN or NIN"
                    className="w-full bg-slate-950/60 border border-slate-800 rounded-xl px-4 py-2.5 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500 transition-all text-xs text-center font-mono tracking-widest"
                    maxLength={11}
                  />
                  {modalError && (
                    <p className="text-[10px] text-rose-450 mt-1 font-medium">{modalError}</p>
                  )}
                </div>

                <div className="flex gap-3 mt-2">
                  <button 
                    type="button"
                    onClick={() => setShowVerifyModal(false)}
                    className={btnSecondary}
                  >
                    Cancel
                  </button>
                  <button 
                    type="submit"
                    className={btnPrimary}
                  >
                    Verify & Authorize
                  </button>
                </div>
              </form>
            </motion.div>
          </div>
        )}
      </AnimatePresence>
    </div>
  );
};

export default Transfers;
