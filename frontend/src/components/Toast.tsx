import React from 'react';
import { CheckCircle, AlertCircle } from 'lucide-react';
import { useBank } from '../context/BankContext';
import { motion, AnimatePresence } from 'framer-motion';

export const Toast: React.FC = () => {
  const { toast } = useBank();

  return (
    <AnimatePresence>
      {toast && (
        <motion.div 
          initial={{ opacity: 0, y: -20, scale: 0.9 }}
          animate={{ opacity: 1, y: 0, scale: 1 }}
          exit={{ opacity: 0, y: -20, scale: 0.9 }}
          transition={{ type: 'spring', damping: 25, stiffness: 350 }}
          className={`fixed top-6 right-6 z-50 flex items-center gap-3 px-6 py-4 rounded-2xl shadow-2xl border backdrop-blur-md ${
            toast.type === 'success' 
              ? 'bg-emerald-950/95 border-emerald-500/30 text-emerald-400' 
              : 'bg-rose-950/95 border-rose-500/30 text-rose-400'
          }`}
        >
          {toast.type === 'success' ? (
            <CheckCircle size={20} className="shrink-0 animate-bounce" />
          ) : (
            <AlertCircle size={20} className="shrink-0 animate-pulse" />
          )}
          <span className="font-semibold text-sm">{toast.message}</span>
        </motion.div>
      )}
    </AnimatePresence>
  );
};

export default Toast;
