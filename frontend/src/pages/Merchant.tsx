import React, { useState } from 'react';
import { Globe } from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../mockBackend';

const inputClass = "w-full bg-slate-900/80 border border-slate-800 rounded-xl px-4 py-3 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500 transition-all";
const labelClass = "block text-xs font-semibold text-slate-400 uppercase tracking-wider mb-2";
const btnPrimary = "w-full bg-gradient-to-r from-blue-600 to-indigo-600 hover:from-blue-500 hover:to-indigo-500 text-white font-semibold py-3 px-4 rounded-xl transition-all shadow-lg shadow-blue-500/20 active:scale-[0.98] cursor-pointer";

export const Merchant: React.FC = () => {
  const { 
    currentUser, 
    merchantProfile, 
    merchantInvoices, 
    merchantWallet, 
    reloadUserData, 
    showToast 
  } = useBank();

  const [merchantBusinessName, setMerchantBusinessName] = useState('');
  const [merchantWebhookUrl, setMerchantWebhookUrl] = useState('');
  const [invoiceCustomer, setInvoiceCustomer] = useState('');
  const [invoiceAmount, setInvoiceAmount] = useState('');
  const [invoiceDesc, setInvoiceDesc] = useState('');

  const handleRegisterMerchant = (e: React.FormEvent) => {
    e.preventDefault();
    if (!currentUser || !merchantBusinessName) return;
    try {
      bank.registerMerchant(currentUser.id, merchantBusinessName, merchantWebhookUrl);
      showToast('success', 'Merchant account generated.');
      setMerchantBusinessName('');
      setMerchantWebhookUrl('');
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  const handleCreateInvoice = (e: React.FormEvent) => {
    e.preventDefault();
    if (!currentUser || !invoiceCustomer || !invoiceAmount) return;
    try {
      const amt = parseFloat(invoiceAmount);
      bank.createInvoice(currentUser.id, invoiceCustomer, amt, invoiceDesc);
      showToast('success', 'Invoice published.');
      setInvoiceCustomer('');
      setInvoiceAmount('');
      setInvoiceDesc('');
      reloadUserData();
    } catch (err: any) {
      showToast('error', err.message);
    }
  };

  return (
    <div className="flex flex-col gap-8">
      {/* Setup & API credentials */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
        <div className="md:col-span-2 bg-slate-900 border border-slate-800 rounded-2xl p-6">
          <h3 className="font-bold text-white text-lg mb-2 flex items-center gap-2">
            <Globe size={20} className="text-blue-500" /> Merchant Integration Console
          </h3>
          <p className="text-slate-400 text-sm leading-relaxed">
            Setup your store wallet, query invoice settlements, and secure API callback webhooks.
          </p>
          
          {merchantProfile ? (
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mt-6 text-xs bg-slate-950/80 p-5 rounded-2xl border border-slate-850">
              <div>
                <span className="text-slate-500 block text-[9px] uppercase font-black tracking-wider">Business API Secret</span>
                <span className="text-blue-400 font-mono block overflow-hidden text-ellipsis mt-1">{merchantProfile.api_key}</span>
              </div>
              <div>
                <span className="text-slate-500 block text-[9px] uppercase font-black tracking-wider">Webhook Endpoint</span>
                <span className="text-slate-300 font-semibold block overflow-hidden text-ellipsis mt-1">{merchantProfile.webhook_url || 'Not set'}</span>
              </div>
              <div className="sm:col-span-2 border-t border-slate-900 pt-4 mt-2 flex justify-between">
                <div>
                  <span className="text-slate-500 block text-[9px] uppercase font-black tracking-wider">Business Settlement Wallet</span>
                  <span className="text-slate-300 font-semibold mt-1 block">{merchantWallet?.account_number}</span>
                </div>
                <div className="text-right">
                  <span className="text-slate-500 block text-[9px] uppercase font-black tracking-wider">Settled Funds</span>
                  <span className="text-emerald-400 font-black mt-1 block text-lg">{merchantWallet?.balance.toLocaleString()} NGN</span>
                </div>
              </div>
            </div>
          ) : (
            <div className="mt-6 text-slate-500 text-xs leading-relaxed">
              You do not have an active merchant store account. Register your trade profile on the right to start issuing customer checkout bills.
            </div>
          )}
        </div>

        {/* Register form */}
        {!merchantProfile ? (
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
            <h3 className="font-bold text-white text-base mb-6">Register Merchant Store</h3>
            <form onSubmit={handleRegisterMerchant} className="flex flex-col gap-4">
              <div>
                <label className={labelClass}>Registered Trade Name</label>
                <input 
                  type="text" 
                  value={merchantBusinessName}
                  onChange={e => setMerchantBusinessName(e.target.value)}
                  className={inputClass}
                  placeholder="Apex Tech Services"
                />
              </div>
              <div>
                <label className={labelClass}>Callback Webhook URL</label>
                <input 
                  type="url" 
                  value={merchantWebhookUrl}
                  onChange={e => setMerchantWebhookUrl(e.target.value)}
                  className={inputClass}
                  placeholder="https://api.yourstore.com/webhooks"
                />
              </div>
              <button type="submit" className={btnPrimary}>Register Store profile</button>
            </form>
          </div>
        ) : (
          /* Invoice publisher */
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
            <h3 className="font-bold text-white text-base mb-6">Publish Customer Invoice</h3>
            <form onSubmit={handleCreateInvoice} className="flex flex-col gap-4">
              <div>
                <label className={labelClass}>Customer Account No</label>
                <input 
                  type="text" 
                  value={invoiceCustomer}
                  onChange={e => setInvoiceCustomer(e.target.value)}
                  className={inputClass}
                  placeholder="e.g. 104829103"
                />
              </div>
              <div>
                <label className={labelClass}>Billing Amount (NGN)</label>
                <input 
                  type="number" 
                  value={invoiceAmount}
                  onChange={e => setInvoiceAmount(e.target.value)}
                  className={inputClass}
                  placeholder="e.g. 45,000"
                />
              </div>
              <div>
                <label className={labelClass}>Product Description</label>
                <input 
                  type="text" 
                  value={invoiceDesc}
                  onChange={e => setInvoiceDesc(e.target.value)}
                  className={inputClass}
                  placeholder="Hosting fees billing"
                />
              </div>
              <button type="submit" className={btnPrimary}>Publish Invoice</button>
            </form>
          </div>
        )}
      </div>

      {/* Invoice ledger */}
      {merchantProfile && (
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6">
          <h3 className="font-bold text-white text-base mb-6">Published Invoices</h3>
          {merchantInvoices.length === 0 ? (
            <div className="text-center py-12 text-slate-500 text-sm">No invoices published yet.</div>
          ) : (
            <div className="overflow-x-auto">
              <table className="w-full text-sm text-left text-slate-300">
                <thead>
                  <tr className="border-b border-slate-800 text-xs text-slate-500 uppercase tracking-wider">
                    <th className="pb-3">Invoice UUID</th>
                    <th className="pb-3">Customer Acc</th>
                    <th className="pb-3">Product Description</th>
                    <th className="pb-3 text-right">Amount</th>
                    <th className="pb-3 text-center">Status</th>
                  </tr>
                </thead>
                <tbody>
                  {merchantInvoices.map(inv => (
                    <tr key={inv.id} className="border-b border-slate-850">
                      <td className="py-4 font-mono text-xs text-slate-400">{inv.id.slice(0, 18)}...</td>
                      <td className="py-4 font-mono text-xs">{inv.customer_account_number}</td>
                      <td className="py-4 text-slate-400 text-xs">{inv.description}</td>
                      <td className="py-4 text-right font-black text-white">{inv.amount.toLocaleString()} NGN</td>
                      <td className="py-4 text-center">
                        <span className={`px-2.5 py-0.5 rounded-full text-[10px] font-black uppercase border ${
                          inv.status === 'paid' 
                            ? 'bg-emerald-950/40 border-emerald-500/20 text-emerald-400' 
                            : 'bg-amber-950/40 border-amber-500/20 text-amber-400 animate-pulse'
                        }`}>
                          {inv.status}
                        </span>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default Merchant;
