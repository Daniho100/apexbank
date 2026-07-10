import React, { useState } from 'react';
import { useBank } from '../context/BankContext';
import * as bank from '../api/bankApi';
import { motion, AnimatePresence } from 'framer-motion';
import { AlertCircle, Loader2, ChevronLeft, Eye, EyeOff } from 'lucide-react';

const inputClass = "w-full bg-slate-950/80 border border-slate-800 rounded-xl pl-4 pr-10 py-3.5 text-white placeholder-slate-600 focus:outline-none focus:ring-2 focus:ring-orange-500/20 focus:border-orange-500 transition-all text-sm block text-left";
const labelClass = "block text-xs font-semibold text-slate-400 uppercase tracking-wider mb-2 text-left";
const btnPrimary = "w-full bg-orange-600 hover:bg-orange-500 text-white font-bold py-3.5 px-4 rounded-2xl transition-all shadow-lg shadow-orange-500/25 active:scale-[0.98] text-center text-sm cursor-pointer border border-orange-500/10 flex items-center justify-center gap-2";

const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080';

export const Auth: React.FC = () => {
  const { setCurrentUser, showToast, logSecurity } = useBank();
  const [authMode, setAuthMode] = useState<'login' | 'register'>('login');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [role, setRole] = useState('customer');
  const [isLoading, setIsLoading] = useState(false);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  
  // Interactive password toggle
  const [showPassword, setShowPassword] = useState(false);

  const handleAuthSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setErrorMsg(null);
    if (!email || !password) {
      setErrorMsg('Please enter both your email address and password.');
      return;
    }

    setIsLoading(true);

    try {
      if (authMode === 'register') {
        const res = await fetch(`${API_URL}/api/auth/register`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({ email, password, role })
        });
        
        const data = await res.json();
        if (!res.ok) {
          setIsLoading(false);
          setErrorMsg(data.message || 'Registration failed.');
          return;
        }

        // Local storage fallback sync so front-end views can run
        const users = bank.getStorage<bank.User[]>('users', []);
        if (!users.some(u => u.email === email)) {
          const newUser: bank.User = {
            id: crypto.randomUUID(),
            email,
            role,
            status: 'active',
            created_at: new Date().toISOString()
          };
          users.push(newUser);
          bank.setStorage('users', users);

          const accounts = bank.getStorage<bank.Account[]>('accounts', []);
          accounts.push({
            id: crypto.randomUUID(),
            user_id: newUser.id,
            account_number: data.account_number || `1${Math.floor(100000000 + Math.random() * 900000000)}`,
            type: 'savings',
            balance: 0.0,
            status: 'active',
            currency: 'NGN',
            created_at: new Date().toISOString()
          });
          bank.setStorage('accounts', accounts);
        }

        logSecurity("User Registration (Argon2id)", `User registered in PostgreSQL. Account: ${data.account_number}`);
        setIsLoading(false);
        showToast('success', 'Profile registered successfully! Please log in.');
        
        setPassword('');
        setAuthMode('login');
        return;
      }

      // Login flow
      const res = await fetch(`${API_URL}/api/auth/login`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({ email, password })
      });
      
      const data = await res.json();
      if (!res.ok) {
        setIsLoading(false);
        setErrorMsg(data.message || 'Invalid email or passcode.');
        return;
      }

      // Save token to session storage
      sessionStorage.setItem('auth_token', data.token);

      // Sync local storage user if not present (for default accounts)
      const users = bank.getStorage<bank.User[]>('users', []);
      let localUser = users.find(u => u.email === data.user.email);
      if (!localUser) {
        localUser = {
          id: data.user.id,
          email: data.user.email,
          role: data.user.role,
          status: data.user.status,
          created_at: new Date().toISOString()
        };
        users.push(localUser);
        bank.setStorage('users', users);
        
        // Add default account
        const accounts = bank.getStorage<bank.Account[]>('accounts', []);
        if (!accounts.some(a => a.user_id === localUser.id)) {
          const savingsAccNo = `1${Math.floor(100000000 + Math.random() * 900000000)}`;
          accounts.push({
            id: crypto.randomUUID(),
            user_id: localUser.id,
            account_number: savingsAccNo,
            type: 'savings',
            balance: 0.0,
            status: 'active',
            currency: 'NGN',
            created_at: new Date().toISOString()
          });
          bank.setStorage('accounts', accounts);
        }
      }

      logSecurity("Access Token Asserted", `JWT issued for ${data.user.email} from PostgreSQL`);
      showToast('success', 'Login successful. Welcome back.');
      
      // Update UI context
      setCurrentUser(localUser);
      setIsLoading(false);
    } catch (err: any) {
      setIsLoading(false);
      setErrorMsg(err.message || 'Could not connect to the database auth server.');
    }
  };

  return (
    <div className="min-h-screen w-full flex items-center justify-center p-4 bg-slate-950 relative overflow-hidden">
      {/* Visual background glows */}
      <div className="absolute top-1/4 left-1/4 w-96 h-96 bg-orange-600/5 rounded-full blur-3xl glow-bg pointer-events-none"></div>
      <div className="absolute bottom-1/4 right-1/4 w-96 h-96 bg-indigo-650/5 rounded-full blur-3xl glow-bg pointer-events-none"></div>
      
      <div className="bg-slate-900 border border-slate-800 rounded-3xl p-8 max-w-md w-full relative z-10 shadow-2xl overflow-hidden flex flex-col justify-between min-h-[520px]">
        
        {/* Loading Spinner Overlay */}
        <AnimatePresence>
          {isLoading && (
            <motion.div 
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              className="absolute inset-0 bg-slate-950/85 backdrop-blur-sm z-30 flex flex-col items-center justify-center gap-4 text-center p-6"
            >
              <Loader2 size={40} className="text-orange-500 animate-spin" />
              <div>
                <h4 className="font-bold text-white text-base">
                  {authMode === 'login' ? "Authenticating Profile" : "Creating Account Ledger"}
                </h4>
                <p className="text-slate-500 text-xs mt-1.5 leading-normal">
                  Securing cryptographic system assertions. Please hold.
                </p>
              </div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Back navigation button (matching Stellas back arrow) */}
        <div className="h-8 flex items-center justify-start mb-4">
          {authMode === 'register' && (
            <button 
              type="button"
              onClick={() => { setAuthMode('login'); setErrorMsg(null); }}
              className="text-slate-400 hover:text-white transition-colors p-1.5 rounded-xl hover:bg-slate-800 cursor-pointer flex items-center justify-center shrink-0 border border-slate-800/40 bg-slate-950/30"
            >
              <ChevronLeft size={20} />
            </button>
          )}
        </div>

        {/* Stellas Mockup Header Section */}
        <div className="mb-6 text-left">
          <h2 className="text-2xl font-black text-white tracking-tight leading-tight">
            {authMode === 'login' ? "Login into your account" : "Create your account"}
          </h2>
          <p className="text-slate-400 text-xs mt-2 leading-relaxed font-normal">
            {authMode === 'login' 
              ? "Please type in the email address and password linked to your Stellas account." 
              : "Please enter a valid email address and password to register a new ledger account."
            }
          </p>
        </div>

        {/* Validation Errors */}
        {errorMsg && (
          <div className="mb-5 p-4 rounded-xl border border-rose-500/20 bg-rose-950/20 text-rose-450 text-xs flex items-start gap-2.5 text-left">
            <AlertCircle size={16} className="shrink-0 mt-0.5" />
            <span>{errorMsg}</span>
          </div>
        )}

        {/* Inputs Form */}
        <form onSubmit={handleAuthSubmit} className="flex-1 flex flex-col justify-between gap-6">
          <div className="flex flex-col gap-5">
            <div>
              <label className={labelClass}>Email address</label>
              <input 
                type="email" 
                value={email}
                onChange={e => setEmail(e.target.value)}
                className={inputClass}
                placeholder="name@gmail.com"
                disabled={isLoading}
              />
            </div>

            <div>
              <label className={labelClass}>Password</label>
              <div className="relative">
                <input 
                  type={showPassword ? "text" : "password"} 
                  value={password}
                  onChange={e => setPassword(e.target.value)}
                  className={inputClass}
                  placeholder="Enter Password"
                  disabled={isLoading}
                />
                <button
                  type="button"
                  onClick={() => setShowPassword(!showPassword)}
                  className="absolute right-3 top-3.5 text-slate-500 hover:text-white transition-colors cursor-pointer"
                  disabled={isLoading}
                >
                  {showPassword ? <EyeOff size={18} /> : <Eye size={18} />}
                </button>
              </div>
              
              {authMode === 'login' && (
                <button 
                  type="button" 
                  className="text-xs font-semibold text-indigo-400 hover:text-indigo-300 transition-colors mt-2 block ml-auto cursor-pointer"
                >
                  Forgot Password?
                </button>
              )}
            </div>

            {authMode === 'register' && (
              <div>
                <label className={labelClass}>System Access Level</label>
                <select 
                  value={role} 
                  onChange={e => setRole(e.target.value)}
                  className="w-full bg-slate-950/80 border border-slate-800 rounded-xl px-4 py-3.5 text-white placeholder-slate-500 focus:outline-none focus:ring-2 focus:ring-orange-500/20 focus:border-orange-500 transition-all text-sm cursor-pointer appearance-none text-left"
                  disabled={isLoading}
                >
                  <option value="customer">Customer User (Client)</option>
                  <option value="administrator">System Administrator</option>
                  <option value="support">Support Agent Desk</option>
                  <option value="merchant">Merchant Business Portal</option>
                </select>
              </div>
            )}
          </div>

          <div className="mt-4 flex flex-col gap-4">
            <button 
              type="submit" 
              className={btnPrimary}
              disabled={isLoading}
            >
              {authMode === 'login' ? "Log in" : "Sign up"}
            </button>

            {/* Bottom Toggle Link matching mockup */}
            <div className="text-xs text-center text-slate-400">
              {authMode === 'login' ? (
                <>
                  Dont have an account?
                  <button 
                    type="button"
                    onClick={() => { setAuthMode('register'); setErrorMsg(null); }}
                    className="text-indigo-400 hover:text-indigo-300 font-bold ml-1 transition-colors cursor-pointer"
                  >
                    Sign up
                  </button>
                </>
              ) : (
                <>
                  Already have an account?
                  <button 
                    type="button"
                    onClick={() => { setAuthMode('login'); setErrorMsg(null); }}
                    className="text-indigo-400 hover:text-indigo-300 font-bold ml-1 transition-colors cursor-pointer"
                  >
                    Sign in
                  </button>
                </>
              )}
            </div>
          </div>
        </form>
      </div>
    </div>
  );
};

export default Auth;
