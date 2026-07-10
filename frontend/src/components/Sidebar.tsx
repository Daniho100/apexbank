import React from 'react';
import { 
   Send, Shield, CreditCard, Percent, Zap, Globe, Award, LogOut 
} from 'lucide-react';
import { useBank } from '../context/BankContext';

const tabClassActive = "flex items-center gap-3 px-4 py-3.5 rounded-xl bg-blue-600 text-white font-semibold transition-all shadow-lg shadow-blue-600/10 border border-blue-500/20";
const tabClassInactive = "flex items-center gap-3 px-4 py-3.5 rounded-xl hover:bg-slate-900/60 text-slate-400 hover:text-slate-200 font-medium transition-all";

export const Sidebar: React.FC = () => {
  const { currentUser, activeTab, setActiveTab, merchantProfile, handleLogout } = useBank();

  if (!currentUser) return null;

  return (
    <aside className="w-full md:w-64 bg-slate-900 border-r border-slate-800 flex flex-col shrink-0">
      {/* Logo */}
      <div className="p-6 border-b border-slate-850 flex items-center gap-3">
        <span className="bg-blue-600 text-white font-mono font-black text-xs px-2.5 py-1.5 rounded-lg tracking-wider">APEX</span>
        <span className="font-black tracking-tight text-white text-base">TRUST BANK</span>
      </div>

      {/* Nav Menu */}
      <nav className="flex-1 p-4 flex flex-col gap-1.5">
        {currentUser.role !== 'administrator' && (
          <>
            <button 
              onClick={() => setActiveTab('overview')} 
              className={activeTab === 'overview' ? tabClassActive : tabClassInactive}
            >
              <CreditCard size={18} /> Overview
            </button>
            <button 
              onClick={() => setActiveTab('transfers')} 
              className={activeTab === 'transfers' ? tabClassActive : tabClassInactive}
            >
              <Send size={18} /> Payments
            </button>
            <button 
              onClick={() => setActiveTab('fixed-deposits')} 
              className={activeTab === 'fixed-deposits' ? tabClassActive : tabClassInactive}
            >
              <Percent size={18} /> Fixed Deposits
            </button>
            <button 
              onClick={() => setActiveTab('loans')} 
              className={activeTab === 'loans' ? tabClassActive : tabClassInactive}
            >
              <Award size={18} /> Loans Engine
            </button>
            <button 
              onClick={() => setActiveTab('utilities')} 
              className={activeTab === 'utilities' ? tabClassActive : tabClassInactive}
            >
              <Zap size={18} /> Utilities
            </button>
            
            {merchantProfile && (
              <button 
                onClick={() => setActiveTab('merchant')} 
                className={activeTab === 'merchant' ? tabClassActive : tabClassInactive}
              >
                <Globe size={18} /> Merchant console
              </button>
            )}
          </>
        )}
        
        {currentUser.role === 'administrator' && (
          <button 
            onClick={() => setActiveTab('admin')} 
            className={activeTab === 'admin' ? tabClassActive : tabClassInactive}
          >
            <Shield size={18} /> Admin Portal
          </button>
        )}
      </nav>

      {/* Profile footer */}
      <div className="p-4 border-t border-slate-850 flex flex-col gap-3 bg-slate-950/20">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 rounded-full bg-slate-800 border border-slate-700 flex items-center justify-center font-bold text-blue-400 uppercase">
            {currentUser.email[0]}
          </div>
          <div className="overflow-hidden">
            <span className="block text-sm font-semibold text-white truncate">{currentUser.email}</span>
            <span className="text-[10px] uppercase tracking-wider font-extrabold text-blue-500">{currentUser.role}</span>
          </div>
        </div>
        <button 
          onClick={handleLogout}
          className="w-full flex items-center justify-center gap-2 bg-slate-800 hover:bg-rose-950/30 hover:text-rose-400 py-2.5 rounded-xl border border-slate-700 text-slate-300 font-semibold text-xs transition-all cursor-pointer"
        >
          <LogOut size={14} /> Log Out
        </button>
      </div>
    </aside>
  );
};

export default Sidebar;
