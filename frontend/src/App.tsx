import { BankProvider, useBank } from './context/BankContext';
import Sidebar from './components/Sidebar';
import Header from './components/Header';
import DeveloperHud from './components/DeveloperHud';
import Toast from './components/Toast';
import Auth from './pages/Auth';
import Overview from './pages/Overview';
import Transfers from './pages/Transfers';
import FixedDeposits from './pages/FixedDeposits';
import Loans from './pages/Loans';
import Utilities from './pages/Utilities';
import Merchant from './pages/Merchant';
import Admin from './pages/Admin';

function AppContent() {
  const { currentUser, activeTab } = useBank();

  if (!currentUser) {
    return <Auth />;
  }

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col antialiased">
      <Toast />
      
      <div className="flex-1 flex flex-col md:flex-row min-h-screen">
        <Sidebar />

        <div className="flex-1 flex flex-col min-h-0 bg-slate-950">
          <Header />

          <main className="flex-1 p-8 overflow-y-auto">
            {activeTab === 'overview' && <Overview />}
            {activeTab === 'transfers' && <Transfers />}
            {activeTab === 'fixed-deposits' && <FixedDeposits />}
            {activeTab === 'loans' && <Loans />}
            {activeTab === 'utilities' && <Utilities />}
            {activeTab === 'merchant' && <Merchant />}
            {activeTab === 'admin-dashboard' && <Admin initialTab="dashboard" />}
            {activeTab === 'admin-directory' && <Admin initialTab="directory" />}
            {activeTab === 'admin-approvals' && <Admin initialTab="approvals" />}
            {activeTab === 'admin-audits' && <Admin initialTab="audits" />}
          </main>
        </div>
      </div>

      <DeveloperHud />
    </div>
  );
}

export default function App() {
  return (
    <BankProvider>
      <AppContent />
    </BankProvider>
  );
}
