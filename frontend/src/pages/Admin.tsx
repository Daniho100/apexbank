import React from 'react';
import { Shield, Terminal } from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../mockBackend';

export const Admin: React.FC = () => {
  const { loansList, auditLogs, reloadUserData, showToast } = useBank();

  const handleApproveLoan = (loanId: string) => {
    try {
      bank.approveLoan(loanId);
      showToast('success', 'Loan approved and cash disbursed.');
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const pendingLoans = loansList.filter(l => l.status === 'pending');

  return (
    <div className="flex flex-col gap-8">
      {/* Loan applications approvals */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
        <h3 className="font-bold text-white text-base mb-6 flex items-center gap-2">
          <Shield size={18} className="text-blue-500" /> Pending Loan Approvals
        </h3>
        
        {pendingLoans.length === 0 ? (
          <div className="text-center py-12 text-slate-500 text-sm">No credit requests requiring processing.</div>
        ) : (
          <div className="flex flex-col gap-4">
            {pendingLoans.map(loan => (
              <div key={loan.id} className="bg-slate-950/40 border border-slate-850 rounded-2xl p-5 flex flex-col sm:flex-row justify-between items-start sm:items-center gap-4">
                <div>
                  <span className="text-[10px] text-slate-500 font-mono block">APPLICATION ID: {loan.id}</span>
                  <span className="text-base font-bold text-slate-200 block mt-1">Requested Principal: {loan.amount.toLocaleString()} NGN</span>
                  <span className="text-xs text-slate-400">Term: {loan.duration_months} Months | Interest: {loan.interest_rate}% APR</span>
                </div>
                <button 
                  onClick={() => handleApproveLoan(loan.id)}
                  className="px-5 py-3 rounded-xl bg-emerald-600 hover:bg-emerald-500 text-white font-semibold text-xs transition-colors shadow-lg shadow-emerald-600/10 active:scale-[0.98] cursor-pointer"
                >
                  Approve & Disburse Cash
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Audit Trail Explorer */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
        <h3 className="font-bold text-white text-base mb-6 flex items-center gap-2">
          <Terminal size={18} className="text-blue-500" /> Security Audit Log Trail
        </h3>
        
        {auditLogs.length === 0 ? (
          <div className="text-center py-12 text-slate-500 text-sm">No audit logs registered.</div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-xs text-left text-slate-400">
              <thead>
                <tr className="border-b border-slate-800 text-slate-500 uppercase tracking-wider">
                  <th className="pb-3 pr-4">Time</th>
                  <th className="pb-3 pr-4">Event Action</th>
                  <th className="pb-3 pr-4">Detailed Description</th>
                  <th className="pb-3">IP Address</th>
                </tr>
              </thead>
              <tbody>
                {auditLogs.slice(0, 15).map(log => (
                  <tr key={log.id} className="border-b border-slate-850 text-slate-350 hover:bg-slate-855/20">
                    <td className="py-3.5 pr-4 font-mono text-[10px] text-slate-500">{new Date(log.created_at).toLocaleTimeString()}</td>
                    <td className="py-3.5 pr-4 uppercase font-bold text-[9px] tracking-widest text-slate-300">{log.action}</td>
                    <td className="py-3.5 pr-4 text-slate-400 text-xs">{log.description}</td>
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
