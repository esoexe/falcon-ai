const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('falconDesktop', {
  isDesktop: true,
  appName: 'Falcon AI',
  version: '2.0.0',
  platform: process.platform,
  minimize: () => ipcRenderer.send('falcon:window-minimize'),
  maximize: () => ipcRenderer.send('falcon:window-maximize'),
  close: () => ipcRenderer.send('falcon:window-close'),

  // Live Animated Setup Wizard IPC
  getInstallerDefaults: () => ipcRenderer.invoke('falcon:installer-defaults'),
  browseInstallDir: (currentDir) => ipcRenderer.invoke('falcon:installer-browse', currentDir),
  runInstallation: (opts) => ipcRenderer.invoke('falcon:installer-run', opts),
  onInstallProgress: (cb) => {
    ipcRenderer.removeAllListeners('falcon:installer-progress');
    ipcRenderer.on('falcon:installer-progress', (_ev, data) => cb(data));
  },
  finishSetupAndLaunch: (opts) => ipcRenderer.invoke('falcon:installer-finish', opts),
});
