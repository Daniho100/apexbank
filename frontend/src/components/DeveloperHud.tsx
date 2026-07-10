import React from 'react';
import { Terminal, Code, X } from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../api/bankApi';
import { motion, AnimatePresence } from 'framer-motion';

const btnSecondary = "w-full bg-slate-800 hover:bg-slate-700 text-slate-200 font-semibold py-3 px-4 rounded-xl transition-all active:scale-[0.98] cursor-pointer";

export const DeveloperHud: React.FC = () => {
  const { 
    currentUser, 
    devHudOpen, 
    setDevHudOpen, 
    securityLogs, 
    showToast, 
    reloadUserData 
  } = useBank();

  if (!currentUser) return null;

  return (
    <>
      {/* Floating Developer Drawer HUD Button */}
      <button 
        onClick={() => setDevHudOpen(true)}
        className="fixed bottom-6 right-6 z-40 bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-bold p-4 rounded-full shadow-2xl border border-blue-400/20 active:scale-95 transition-all flex items-center gap-2 group cursor-pointer"
        title="Open Developer HUD Console"
      >
        <Code size={20} />
        <span className="text-xs font-semibold uppercase tracking-wider max-w-0 overflow-hidden group-hover:max-w-xs transition-all duration-300 ease-in-out">
          Developer HUD
        </span>
      </button>

      {/* Sliding side-drawer for Developer HUD */}
      <AnimatePresence>
        {devHudOpen && (
          <div className="fixed inset-0 z-50 flex justify-end">
            {/* Backdrop overlay */}
            <motion.div 
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              onClick={() => setDevHudOpen(false)} 
              className="absolute inset-0 bg-black/60 backdrop-blur-sm transition-opacity"
            ></motion.div>
            
            {/* Drawer content */}
            <motion.aside 
              initial={{ x: '100%' }}
              animate={{ x: 0 }}
              exit={{ x: '100%' }}
              transition={{ type: 'spring', damping: 25, stiffness: 220 }}
              className="w-full max-w-md bg-slate-900 border-l border-slate-800 p-6 shadow-2xl relative flex flex-col justify-between h-full z-10"
            >
              <div className="flex flex-col gap-6 min-h-0">
                <div className="flex justify-between items-center border-b border-slate-800 pb-4">
                  <div>
                    <h3 className="font-black text-white text-sm flex items-center gap-2">
                      <Terminal size={18} className="text-blue-500" /> CRYPTOGRAPHIC MONITOR HUD
                    </h3>
                    <p className="text-[10px] text-slate-500 mt-1 leading-normal">
                      Secure JWT state assertion and cryptographic hashing tracer.
                    </p>
                  </div>
                  <button 
                    onClick={() => setDevHudOpen(false)}
                    className="p-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-slate-400 hover:text-white border border-slate-750 transition-colors cursor-pointer"
                  >
                    <X size={16} />
                  </button>
                </div>

                {/* JWT Decoded token Claims */}
                <div className="flex flex-col gap-3">
                  <span className="text-[9px] font-black text-slate-400 uppercase tracking-widest block">Session Claims (JWT)</span>
                  <div className="bg-slate-950 border border-slate-850 rounded-2xl p-4 font-mono text-[10px] leading-relaxed flex flex-col gap-3">
                    <div>
                      <span className="text-slate-600 block uppercase font-bold text-[8px] tracking-wider">Alg/Type Headers</span>
                      <span className="text-rose-400 block mt-0.5">{"{\"alg\": \"HS256\", \"typ\": \"JWT\"}"}</span>
                    </div>
                    <div>
                      <span className="text-slate-600 block uppercase font-bold text-[8px] tracking-wider">Claims Signature payload</span>
                      <span className="text-blue-400 block mt-0.5 break-all">
                        {JSON.stringify({ sub: currentUser.id, email: currentUser.email, role: currentUser.role, exp: Math.floor(Date.now()/1000) + 3600 })}
                      </span>
                    </div>
                  </div>
                </div>

                {/* Seeding tools */}
                <div className="flex flex-col gap-3">
                  <span className="text-[9px] font-black text-slate-400 uppercase tracking-widest block">Sandbox State Seeds</span>
                  <div className="grid grid-cols-1 gap-3 text-xs font-semibold">
                    <button 
                      onClick={async () => {
                        try {
                          await bank.registerMerchant(currentUser.id, "Apex Services Ltd", "https://api.merchant.com/callback");
                          await bank.createInvoice(currentUser.id, "123456789", 45000.00, "Web Hosting Payout");
                          showToast('success', 'Merchant profile and invoice generated.');
                          await reloadUserData();
                        } catch(e: any) {
                          showToast('error', e.message);
                        }
                      }} 
                      className="py-3 rounded-xl bg-slate-950 hover:bg-slate-850 border border-slate-800 text-slate-300 text-center transition-all cursor-pointer"
                    >
                      Seed Merchant
                    </button>
                  </div>
                </div>

                {/* Trace events logs */}
                <div className="flex-1 flex flex-col gap-3 min-h-0">
                  <span className="text-[9px] font-black text-slate-400 uppercase tracking-widest block">Security Event traces</span>
                  <div className="flex-1 bg-slate-950 border border-slate-850 rounded-2xl p-4 font-mono text-[9px] overflow-y-auto flex flex-col gap-3 text-slate-400">
                    {securityLogs.length === 0 ? (
                      <span className="text-slate-600 block text-center mt-20">Monitoring idle. Events will appear here.</span>
                    ) : (
                      securityLogs.map((log, index) => (
                        <div key={index} className="flex flex-col gap-1 border-b border-slate-900 pb-2.5">
                          <div className="flex justify-between items-center text-slate-500">
                            <span>{log.time}</span>
                            <span className="text-blue-500 font-bold uppercase tracking-wider">{log.event}</span>
                          </div>
                          <span className="text-slate-300 break-all leading-normal mt-0.5">{log.detail}</span>
                        </div>
                      ))
                    )}
                  </div>
                </div>

              </div>

              <div className="border-t border-slate-800 pt-4 mt-4">
                <button 
                  onClick={() => setDevHudOpen(false)}
                  className={btnSecondary}
                >
                  Close HUD
                </button>
              </div>
            </motion.aside>
          </div>
        )}
      </AnimatePresence>
    </>
  );
};

export default DeveloperHud;
