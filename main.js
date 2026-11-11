const electron = require('electron');
const { app, BrowserWindow, ipcMain } = electron;
const pty = require('@lydell/node-pty');

// Ekta-i app process thakbe. Dwitiyo-bar `npm start` dile notun process
// khulbe na, ager app-e ekta notun window khulbe.
const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  app.quit();
} else {
  // prottek window-er nijer shell (pty) thake
  const sessions = new Map();   // webContents.id -> { win, term }

  function createWindow() {
    const base = BrowserWindow.getFocusedWindow() || BrowserWindow.getAllWindows()[0];
    const pos = base ? { x: base.getBounds().x + 32, y: base.getBounds().y + 32 } : {};

    const win = new BrowserWindow({
      width: 1000,
      height: 640,
      minWidth: 480,
      minHeight: 300,
      ...pos,
      frame: false,            // Windows-er title bar o border thakbe na
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
  app.whenReady().then(createWindow);
  app.on('window-all-closed', () => app.quit());

  // ---- Shell (WSL) ----
  ipcMain.on('pty-start', (e, size) => {
    const s = sessions.get(e.sender.id);
    if (!s || s.term) return;
    s.term = pty.spawn('wsl.exe', ['--cd', '~/retroshell', '-e', './retroshell'], {
      name: 'xterm-256color',
      cols: size.cols,
      rows: size.rows,
      env: process.env
    });
    s.term.onData((d) => { if (!s.win.isDestroyed()) s.win.webContents.send('pty-data', d); });
    s.term.onExit(() => {
      s.term = null;                              // PTY exit hoyegele null kore dao
      if (!s.win.isDestroyed()) s.win.close();   // shell bondho -> window bondho
    });
  });
  ipcMain.on('pty-input', (e, d) => { const s = sessions.get(e.sender.id); if (s && s.term) s.term.write(d); });
  ipcMain.on('pty-resize', (e, z) => {
    const s = sessions.get(e.sender.id);
    // PTY already exit hoye gele resize korba na - nahole crash
    if (s && s.term) { try { s.term.resize(z.cols, z.rows); } catch (_) { } }
  });

  // ---- Window buttons ----
  const winOf = (e) => BrowserWindow.fromWebContents(e.sender);
  ipcMain.on('win-min', (e) => winOf(e).minimize());
  ipcMain.on('win-max', (e) => { const w = winOf(e); w.isMaximized() ? w.unmaximize() : w.maximize(); });
  ipcMain.on('win-close', (e) => winOf(e).close());
  ipcMain.on('new-window', () => createWindow());
}
