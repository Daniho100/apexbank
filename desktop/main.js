const { app, BrowserWindow, session } = require('electron');
const path = require('path');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 800,
    title: "Apex Trust Bank",
    icon: path.join(__dirname, 'icon.png'),
    webPreferences: {
      // Secure Sandbox Configuration (OWASP compliant)
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  // Desktop Application Firewall: Block unauthorized outbound connections
  session.defaultSession.webRequest.onBeforeRequest((details, callback) => {
    const url = details.url;
    try {
      const parsedUrl = new URL(url);
      const host = parsedUrl.hostname.toLowerCase();

      // Whitelist of allowed hosts
      const allowedHosts = [
        'localhost',
        '127.0.0.1',
        'apexbank-y8k7.onrender.com',
        'fonts.googleapis.com',
        'fonts.gstatic.com'
      ];

      // Internal protocols used by Electron and DevTools
      const allowedProtocols = [
        'file:',
        'devtools:',
        'chrome-extension:',
        'chrome-devtools:'
      ];

      if (allowedProtocols.includes(parsedUrl.protocol)) {
        callback({ cancel: false });
        return;
      }

      const isAllowedHost = allowedHosts.some(allowed => host === allowed || host.endsWith('.' + allowed));
      if (!isAllowedHost) {
        console.warn(`Desktop Network Firewall BLOCKED request to: ${url}`);
        callback({ cancel: true }); // Cancel/block request
        return;
      }

      callback({ cancel: false });
    } catch (e) {
      console.error(`Desktop Firewall URL parsing error: ${e.message}`);
      callback({ cancel: true });
    }
  });

  // Enable Secure Content Security Policy (CSP)
  session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    callback({
      responseHeaders: {
        ...details.responseHeaders,
        'Content-Security-Policy': [
          "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; connect-src 'self' wss://apexbank-y8k7.onrender.com https://apexbank-y8k7.onrender.com;"
        ]
      }
    });
  });

  // Smart Loader: Load Vite Dev Server if running, else load static index.html from Vite Build
  const isDev = process.env.NODE_ENV === 'development';
  if (isDev) {
    mainWindow.loadURL('http://localhost:5173');
    mainWindow.webContents.openDevTools();
  } else {
    // Load static build HTML file relative to app package root
    const indexPath = app.isPackaged
      ? path.join(__dirname, 'frontend-dist/index.html')
      : path.join(__dirname, '../frontend/dist/index.html');

    mainWindow.loadFile(indexPath).catch(() => {
      // Fallback if build is not compiled yet
      mainWindow.loadURL('http://localhost:5173').catch(() => {
        mainWindow.loadURL('data:text/html,<h1>Development Server Offline</h1><p>Vite dev server is offline and static react build was not compiled. Run <code>npm run build</code> inside the frontend folder.</p>');
      });
    });
  }

  mainWindow.on('closed', function () {
    mainWindow = null;
  });
}

app.on('ready', createWindow);

app.on('window-all-closed', function () {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', function () {
  if (mainWindow === null) {
    createWindow();
  }
});
