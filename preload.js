const { contextBridge, ipcRenderer, clipboard } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  send: (channel, data) => {
    const validChannels = ['pty-start', 'pty-input', 'pty-resize', 'win-min', 'win-max', 'win-close', 'new-window'];
    if (validChannels.includes(channel)) {
      ipcRenderer.send(channel, data);
    }
  },
  on: (channel, func) => {
    const validChannels = ['pty-data'];
    if (validChannels.includes(channel)) {
      ipcRenderer.on(channel, (event, ...args) => func(...args));
    }
  },
  clipboard: {
    readText: () => clipboard.readText(),
    writeText: (text) => clipboard.writeText(text)
  }
});
