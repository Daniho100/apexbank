import React, { useMemo } from 'react';
import { Bell } from 'lucide-react';
import { useBank } from '../context/BankContext';
import * as bank from '../mockBackend';

export const Header: React.FC = () => {
  const { 
    currentUser, 
    activeTab, 
    notificationOpen, 
    setNotificationOpen, 
    notifications, 
    reloadUserData 
  } = useBank();

  if (!currentUser) return null;

  const unreadNotificationsCount = useMemo(() => {
    return notifications.filter(n => !n.is_read).length;
  }, [notifications]);

  const handleMarkAllRead = () => {
    const notes = bank.getStorage<bank.Notification[]>('notifications', []);
    notes.forEach(n => {
      if (n.user_id === currentUser.id) {
        n.is_read = true;
      }
    });
    bank.setStorage('notifications', notes);
    reloadUserData();
  };

  return (
    <header className="h-20 bg-slate-900 border-b border-slate-800 flex items-center justify-between px-8 z-10 shrink-0">
      <div className="flex items-center gap-4">
        <h2 className="text-lg font-bold text-white capitalize">{activeTab.replace('-', ' ')}</h2>
      </div>
      
      <div className="flex items-center gap-4">
        {/* Notifications Bell */}
        <div className="relative">
          <button 
            onClick={() => setNotificationOpen(!notificationOpen)}
            className="p-2.5 rounded-xl bg-slate-800 border border-slate-700 text-slate-400 hover:text-white transition-colors relative cursor-pointer"
          >
            <Bell size={18} />
            {unreadNotificationsCount > 0 && (
              <span className="absolute -top-1.5 -right-1.5 w-5 h-5 bg-rose-600 rounded-full text-white font-extrabold text-[10px] flex items-center justify-center border border-slate-900 animate-pulse">
                {unreadNotificationsCount}
              </span>
            )}
          </button>

          {/* Dropdown notifications list */}
          {notificationOpen && (
            <div className="absolute right-0 mt-3 w-80 bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-2xl z-30">
              <div className="flex justify-between items-center border-b border-slate-800 pb-2 mb-3">
                <span className="text-xs font-bold text-white uppercase tracking-wider">Alert Feed</span>
                <button 
                  onClick={handleMarkAllRead}
                  className="text-[10px] font-semibold text-blue-400 hover:text-blue-300 cursor-pointer"
                >
                  Mark all read
                </button>
              </div>
              <div className="flex flex-col gap-2 max-h-60 overflow-y-auto">
                {notifications.length === 0 ? (
                  <div className="text-center py-6 text-slate-500 text-xs">No alerts.</div>
                ) : (
                  notifications.map(n => (
                    <div 
                      key={n.id} 
                      className={`p-2.5 rounded-xl border text-xs transition-colors ${
                        n.is_read ? 'bg-slate-950/30 border-slate-850 text-slate-400' : 'bg-blue-950/20 border-blue-900/30 text-slate-200'
                      }`}
                    >
                      <span className="font-bold block mb-0.5">{n.title}</span>
                      <p className="text-[11px] text-slate-400 leading-normal">{n.content}</p>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}
        </div>

        <div className="h-6 w-[1px] bg-slate-800"></div>

        <span className="text-xs font-mono text-slate-500">Correlation Network online</span>
      </div>
    </header>
  );
};

export default Header;
