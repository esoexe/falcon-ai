/**
 * Falcon AI Desktop & Live-Animated Setup Wizard — Electron Main Process.
 *
 * Dual-Mode Architecture:
 *  - When launched as `Falcon-AI-Setup-*.exe` (or before first-run installation receipt,
 *    or with `--setup`), opens a frameless 1040×650 60-FPS 3D Holographic Setup Wizard
 *    that installs Falcon AI cleanly with live laser-slicer mesh animation, Desktop/Start Menu
 *    shortcuts, and 68 pre-configured free AI providers.
 *  - When launched from an installed shortcut (`--studio`) or after clicking
 *    `[Launch Falcon AI Studio]` / `[Run Portable Mode]`, morphs smoothly into the
 *    full 1440×900 OpenCode 3D & Playable Game Studio backed by the C++20 daemon.
 */

const { app, BrowserWindow, dialog, ipcMain, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const os = require('os');
const net = require('net');
const http = require('http');
const { spawn } = require('child_process');
const { startFallbackServer } = require('./fallback_engine');
const { getDefaultInstallDir, performLiveInstallation } = require('./installer_engine');

let mainWindow = null;
let cppDaemon = null;
let fallbackHttpServer = null;
let backendPort = null;

function findFreePort() {
  return new Promise((resolve, reject) => {
    const srv = net.createServer();
    srv.listen(0, '127.0.0.1', () => {
      const { port } = srv.address();
      srv.close(() => resolve(port));
    });
    srv.on('error', reject);
  });
}

function waitForHttpReady(url, timeoutMs = 6000) {
  const start = Date.now();
  return new Promise((resolve) => {
    const check = () => {
      http
        .get(url, (res) => {
          res.resume();
          if (res.statusCode === 200) resolve(true);
          else retry();
        })
        .on('error', retry);
    };
    const retry = () => {
      if (Date.now() - start > timeoutMs) resolve(false);
      else setTimeout(check, 120);
    };
    check();
  });
}

function locateCppEngineBinary() {
  const exeName = process.platform === 'win32' ? 'falcon_server.exe' : 'falcon_server';
  const candidates = [
    path.join(process.resourcesPath || '', 'bin', exeName),
    path.join(__dirname, '..', '..', 'bin', exeName),
    path.join(process.cwd(), 'bin', exeName),
  ];
  for (const p of candidates) {
    if (p && fs.existsSync(p)) return p;
  }
  return null;
}

async function ensureBackendEngine() {
  if (backendPort) return backendPort;
  const port = await findFreePort();
  const userDataDir = app.getPath('userData');
  fs.mkdirSync(userDataDir, { recursive: true });

  const cppBin = locateCppEngineBinary();
  const starterDir = path.join(__dirname, '..', 'starter_assets');
  const assetsOut = path.join(userDataDir, 'workspace_assets');
  const gamesOut = path.join(userDataDir, 'workspace_games');
  try {
    fs.mkdirSync(assetsOut, { recursive: true });
    fs.mkdirSync(gamesOut, { recursive: true });
    if (fs.existsSync(starterDir) && fs.readdirSync(assetsOut).filter((f) => f.endsWith('.json')).length === 0) {
      for (const f of fs.readdirSync(starterDir)) {
        if (f.startsWith('game_')) {
          fs.copyFileSync(path.join(starterDir, f), path.join(gamesOut, f.replace(/^game_/, '')));
        } else {
          fs.copyFileSync(path.join(starterDir, f), path.join(assetsOut, f));
        }
      }
    }
  } catch {}

  if (cppBin) {
    cppDaemon = spawn(cppBin, ['--port', String(port), '--dir', userDataDir], {
      windowsHide: true,
      stdio: 'ignore',
    });
    const ok = await waitForHttpReady(`http://127.0.0.1:${port}/api/hardware`, 20000);
    if (ok) {
      backendPort = port;
      return port;
    }
  }

  const webDir = fs.existsSync(path.join(__dirname, '..', 'web_bundle'))
    ? path.join(__dirname, '..', 'web_bundle')
    : path.join(__dirname, '..', '..', 'frontend', 'web_bundle');

  fallbackHttpServer = await startFallbackServer({
    port,
    webDir,
    assetsDir: path.join(userDataDir, 'workspace_assets'),
    starterDir,
  });
  backendPort = port;
  return port;
}

function shouldOpenAnimatedSetup() {
  const argv = process.argv.map((a) => a.toLowerCase());
  if (argv.includes('--studio')) return false;
  if (argv.includes('--setup') || argv.includes('--installer')) return true;

  const launchedFile = path.basename(process.env.PORTABLE_EXECUTABLE_FILE || process.execPath || '').toLowerCase();
  const receiptFile = path.join(app.getPath('userData'), 'falcon_install_receipt.json');
  if (launchedFile.includes('setup') && !fs.existsSync(receiptFile)) {
    return true;
  }
  return !fs.existsSync(receiptFile);
}

async function createFalconWindow() {
  const port = await ensureBackendEngine();
  const showSetup = shouldOpenAnimatedSetup();

  mainWindow = new BrowserWindow({
    width: showSetup ? 1080 : 1440,
    height: showSetup ? 680 : 900,
    minWidth: showSetup ? 960 : 980,
    minHeight: showSetup ? 620 : 640,
    title: showSetup ? 'Falcon AI v3.0 Setup' : 'Falcon AI Studio v3.0',
    frame: false,
    backgroundColor: '#0b0c10',
    icon: path.join(__dirname, '..', 'assets', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    if (url.includes('/api/export/') || url.includes('/export/')) {
      mainWindow.webContents.downloadURL(url);
      return { action: 'deny' };
    }
    shell.openExternal(url);
    return { action: 'deny' };
  });

  if (showSetup) {
    const setupHtml = path.join(__dirname, '..', 'installer', 'setup.html');
    if (fs.existsSync(setupHtml)) {
      await mainWindow.loadFile(setupHtml);
      return;
    }
  }

  await mainWindow.loadURL(`http://127.0.0.1:${port}`);
}

// Window Chrome IPC
ipcMain.on('falcon:window-minimize', () => mainWindow?.minimize());
ipcMain.on('falcon:window-maximize', () => {
  if (!mainWindow) return;
  if (mainWindow.isMaximized()) mainWindow.unmaximize();
  else mainWindow.maximize();
});
ipcMain.on('falcon:window-close', () => mainWindow?.close());

// Live Animated Setup Wizard IPC
ipcMain.handle('falcon:installer-defaults', () => {
  const cpus = os.cpus() || [];
  return {
    version: '3.0.0',
    platform: process.platform,
    arch: process.arch,
    defaultDir: getDefaultInstallDir(),
    cpu: (cpus[0]?.model || 'x86_64 Multi-Core Processor').trim().slice(0, 34),
    cores: cpus.length || 8,
    ramGb: Math.max(2, Math.round(os.totalmem() / (1024 ** 3))),
  };
});

ipcMain.handle('falcon:installer-browse', async (_ev, currentDir) => {
  if (!mainWindow) return null;
  const res = await dialog.showOpenDialog(mainWindow, {
    title: 'Select Falcon AI Installation Directory',
    defaultPath: currentDir || getDefaultInstallDir(),
    properties: ['openDirectory', 'createDirectory'],
  });
  if (res.canceled || !res.filePaths?.length) return null;
  return res.filePaths[0];
});

ipcMain.handle('falcon:installer-run', async (_ev, opts) => {
  const starterDir = path.join(__dirname, '..', 'starter_assets');
  return await performLiveInstallation({
    opts: opts || {},
    userDataDir: app.getPath('userData'),
    starterDir,
    emit: (progress) => {
      if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.webContents.send('falcon:installer-progress', progress);
      }
    },
  });
});

ipcMain.handle('falcon:installer-finish', async (_ev, opts) => {
  const port = await ensureBackendEngine();
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.setMinimumSize(980, 640);
    mainWindow.setSize(1440, 900, true);
    mainWindow.center();
    mainWindow.setTitle('Falcon AI Studio v3.0');
    const mode = opts?.launchMode ? `?launch=${encodeURIComponent(opts.launchMode)}` : '';
    await mainWindow.loadURL(`http://127.0.0.1:${port}/${mode}`);
  }
  return { ok: true };
});

app.whenReady().then(createFalconWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

app.on('will-quit', () => {
  if (cppDaemon && !cppDaemon.killed) {
    try { cppDaemon.kill(); } catch {}
  }
  if (fallbackHttpServer) {
    try { fallbackHttpServer.close(); } catch {}
  }
});
