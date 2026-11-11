const electron = require('electron');
const { app, BrowserWindow, ipcMain, globalShortcut } = electron;
const pty = require('@lydell/node-pty');

// Ensure a single instance of the application. If another instance is launched,
// focus the existing instance and open a new window rather than spawning a new process.
const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  app.quit();
} else {
  // Map each window's webContents ID to its terminal session: webContents.id -> { win, term }
  const sessions = new Map();

  function createWindow() {
    const base = BrowserWindow.getFocusedWindow() || BrowserWindow.getAllWindows()[0];
    const pos = base ? { x: base.getBounds().x + 32, y: base.getBounds().y + 32 } : {};

    const win = new BrowserWindow({
      width: 1000,
      height: 640,
      minWidth: 480,
      minHeight: 300,
      ...pos,
      frame: false,            // Frameless window without OS-native title bar or borders
      backgroundColor: '#000000',
      webPreferences: {
        nodeIntegration: true,
        contextIsolation: false
      }
    });
    win.setMenuBarVisibility(false);

    const id = win.webContents.id;
    sessions.set(id, { win, term: null });
    win.on('closed', () => {
      const s = sessions.get(id);
      if (s && s.term) { try { s.term.kill(); } catch (e) { } }
      sessions.delete(id);
    });

    win.loadFile('index.html');
  }

  app.on('second-instance', () => createWindow());
  app.whenReady().then(() => {
    createWindow();
    // Register global hotkey: Ctrl+Alt+T toggles or opens a new window from anywhere in Windows
    globalShortcut.register('CommandOrControl+Alt+T', () => {
      const win = BrowserWindow.getFocusedWindow() || BrowserWindow.getAllWindows()[0];
      if (win) {
        if (win.isMinimized()) win.restore();
        win.focus();
      } else {
        createWindow();
      }
    });
  });
  app.on('will-quit', () => {
    globalShortcut.unregisterAll();
  });
  app.on('window-all-closed', () => app.quit());

  // ---- Shell Process Management (WSL) ----
  ipcMain.on('pty-start', (e, size) => {
    const s = sessions.get(e.sender.id);
    if (!s || s.term) return;

    // Launch shell directly into the Windows user profile directory
    const winHome = process.env.USERPROFILE || 'C:\\Users\\mdsho';
    s.term = pty.spawn('wsl.exe', ['--cd', winHome, '-e', '/home/comando_32/retroshell/retroshell'], {
      name: 'xterm-256color',
      cols: size.cols,
      rows: size.rows,
      env: process.env
    });
    s.term.onData((d) => { if (!s.win.isDestroyed()) s.win.webContents.send('pty-data', d); });
    s.term.onExit(() => {
      s.term = null;                              // Clear reference upon PTY exit
      if (!s.win.isDestroyed()) s.win.close();   // Close corresponding window when shell terminates
    });
  });
  ipcMain.on('pty-input', (e, d) => { const s = sessions.get(e.sender.id); if (s && s.term) s.term.write(d); });
  ipcMain.on('pty-resize', (e, z) => {
    const s = sessions.get(e.sender.id);
    // Prevent resizing an exited PTY to avoid native exception
    if (s && s.term) { try { s.term.resize(z.cols, z.rows); } catch (_) { } }
  });

  // ---- Window Controls ----
  const winOf = (e) => BrowserWindow.fromWebContents(e.sender);
  ipcMain.on('win-min', (e) => winOf(e).minimize());
  ipcMain.on('win-max', (e) => { const w = winOf(e); w.isMaximized() ? w.unmaximize() : w.maximize(); });
  ipcMain.on('win-close', (e) => winOf(e).close());
  ipcMain.on('new-window', () => createWindow());
}
