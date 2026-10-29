import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import Viewport3D from './components/Viewport3D';
import GameViewport from './components/GameViewport';
import SwarmPanel from './components/SwarmPanel';
import SketchPad from './components/SketchPad';
import HardwareModal from './components/HardwareModal';
import CommandPalette from './components/CommandPalette';

const PRESET_PROMPTS = [
  { label: 'katana', prompt: 'Damascus forged silver and gold katana with glowing cyan conduit gem and carved dragon guard' },
  { label: 'hypercar', prompt: 'Crimson and carbon-fiber aero widebody GT hypercar with 10-spoke alloy wheels and swan-neck wing' },
  { label: 'mecha', prompt: 'Articulated cobalt & titanium mecha guardian with glowing cyan fusion core and dorsal flight wings' },
  { label: 'dragon', prompt: 'Crimson and bone elder dragon miniature with spread wings on sculpted basalt cliff base' },
  { label: 'railgun', prompt: 'M-LOK ventilated tactical sci-fi plasma railgun with holographic optic and fluted rail barrel' },
  { label: 'citadel', prompt: 'Medieval stone bastion citadel keep with 4 corner turrets, crenellations, and warm lit windows' },
];

const PBR_PRESETS = [
  { name: 'damascus-steel', color: '#cbd5e1', metallic: 0.94, roughness: 0.20, finish: 'damascus_steel', emissive: null, estr: 0 },
  { name: 'forged-gold', color: '#d4af37', metallic: 0.94, roughness: 0.20, finish: 'forged_metal', emissive: null, estr: 0 },
  { name: 'carbon-fiber', color: '#1f2937', metallic: 0.26, roughness: 0.30, finish: 'carbon_fiber', emissive: null, estr: 0 },
  { name: 'dragon-scales', color: '#7f1d1d', metallic: 0.16, roughness: 0.52, finish: 'dragon_scales', emissive: null, estr: 0 },
  { name: 'painted-armor', color: '#1d4ed8', metallic: 0.78, roughness: 0.24, finish: 'painted_armor', emissive: null, estr: 0 },
  { name: 'carved-bone', color: '#e7e5e4', metallic: 0.06, roughness: 0.56, finish: 'carved_bone', emissive: null, estr: 0 },
  { name: 'basalt-stone', color: '#57534e', metallic: 0.05, roughness: 0.88, finish: 'weathered_stone', emissive: null, estr: 0 },
  { name: 'cyan-plasma', color: '#22d3ee', metallic: 0.20, roughness: 0.10, finish: 'forged_metal', emissive: '#22d3ee', estr: 0.95 },
];

const SLASH_COMMANDS = [
  { cmd: '/build', desc: 'Switch composer to BUILD mode (6-Agent Swarm 3D Model Synthesis)' },
  { cmd: '/detail', desc: 'Run @kitbash-detailer Ultra-Detail & Add-Everything pass on active 3D model' },
  { cmd: '/game', desc: 'Switch to 3D GAME ENGINE mode (build & play 3D games with AI agents)' },
  { cmd: '/edit', desc: 'Switch composer to EDIT mode (modify active 3D assembly in place)' },
  { cmd: '/plan', desc: 'Switch composer to PLAN mode (scope multi-part blueprint read-only)' },
  { cmd: '/swarm', desc: 'Open 6-Agent Swarm Pipeline & Ultra-Detailer inspector' },
  { cmd: '/sketch', desc: 'Open 2D Sketch & Image-to-3D inflation drawer' },
  { cmd: '/gold', desc: 'Apply Forged Gold + Cyan Plasma shader to active model' },
  { cmd: '/chrome', desc: 'Upgrade active 3D assembly to Mirror Chrome' },
  { cmd: '/wings', desc: 'Attach bilateral anatomical dragon wings to active model' },
  { cmd: '/pedestal', desc: 'Attach sculpted stone display base' },
  { cmd: '/repair', desc: 'Run C++20 Manifold Print Doctor & flat build-plate cut' },
  { cmd: '/split', desc: 'Auto-split model on Y-plane with 0.25mm snap alignment pins' },
  { cmd: '/rig', desc: 'Generate 15-bone proportional humanoid skeleton & preview motion' },
  { cmd: '/export glb', desc: 'Download multi-node glTF 2.0 PBR (.glb)' },
  { cmd: '/export 3mf', desc: 'Download Bambu/Orca/Prusa 3MF package (.3mf)' },
  { cmd: '/export stl', desc: 'Download Z-up millimeter binary STL (.stl)' },
  { cmd: '/export game', desc: 'Download standalone playable HTML5 3D Game (.html)' },
  { cmd: '/theme', desc: 'Toggle OpenCode Obsidian Dark / Cream Paper theme' },
  { cmd: '/config', desc: 'Open Hardware & Universal Local LLM Router config' },
];

const AGENT_MODES = ['build', 'detail', 'edit', 'game', 'plan'];

export default function App() {
  const isElectron = typeof window !== 'undefined' && Boolean(window.falconDesktop?.isDesktop);

  // Theme & Layout Panes (OpenCode 3-Column Collapsible)
  const [theme, setTheme] = useState('dark');
  const [leftOpen, setLeftOpen] = useState(true);
  const [rightOpen, setRightOpen] = useState(true);
  const [rightTab, setRightTab] = useState('tool'); // parts | tool | export
  const [activeTool, setActiveTool] = useState('swarm'); // swarm | sketch | pbr | print | rig | code
  const [stageMode, setStageMode] = useState('model'); // model | game
  const [tuiExpanded, setTuiExpanded] = useState(true);

  // Modals
  const [paletteOpen, setPaletteOpen] = useState(false);
  const [hwModalOpen, setHwModalOpen] = useState(false);

  // Backend State
  const [hardware, setHardware] = useState(null);
  const [routerCfg, setRouterCfg] = useState(null);
  const [assets, setAssets] = useState([]);
  const [currentAsset, setCurrentAsset] = useState(null);
  const [games, setGames] = useState([]);
  const [currentGame, setCurrentGame] = useState(null);
  const [busy, setBusy] = useState(false);
  const [busyLabel, setBusyLabel] = useState('running 6-agent c++20 swarm...');

  // Viewport State
  const [shadingMode, setShadingMode] = useState('pbr');
  const [lightingPreset, setLightingPreset] = useState('studio');
  const [showGrid, setShowGrid] = useState(true);
  const [autoRotate, setAutoRotate] = useState(false);
  const [selectedPartId, setSelectedPartId] = useState(null);
  const [showRig, setShowRig] = useState(false);
  const [activeAnimation, setActiveAnimation] = useState('rest');
  const [cameraCommand, setCameraCommand] = useState(null);

  // OpenCode Composer State
  const [agentMode, setAgentMode] = useState('build'); // build | edit | plan
  const [promptInput, setPromptInput] = useState('');
  const [stylePreset, setStylePreset] = useState('realistic');
  const [sdfRes, setSdfRes] = useState(112);
  const [slashIndex, setSlashIndex] = useState(0);
  const composerRef = useRef(null);

  // TUI Activity Stream
  const [tuiFeed, setTuiFeed] = useState([
    {
      id: 'boot',
      kind: 'system',
      text: 'falcon-ai v2.0 initialized · c++20 openmp surface-nets online · tab switches [BUILD / EDIT / PLAN] · type / for commands.',
    },
  ]);

  // Image / Sketch-to-3D State
  const [imgSubMode, setImgSubMode] = useState('sketch');
  const [reconMode, setReconMode] = useState('volumetric');
  const [depthStrength, setDepthStrength] = useState(0.68);
  const [detailRelief, setDetailRelief] = useState(0.38);

  // PBR Shader State
  const [matColor, setMatColor] = useState('#cbd5e1');
  const [matMetallic, setMatMetallic] = useState(0.92);
  const [matRoughness, setMatRoughness] = useState(0.22);
  const [matFinish, setMatFinish] = useState('damascus_steel');
  const [matEmissiveOn, setMatEmissiveOn] = useState(false);
  const [matEmissive, setMatEmissive] = useState('#22d3ee');

  // CAD Exact Millimeter & Topology
  const [dimX, setDimX] = useState(80);
  const [dimY, setDimY] = useState(80);
  const [dimZ, setDimZ] = useState(80);
  const [lockAspect, setLockAspect] = useState(true);
  const [blueprintJson, setBlueprintJson] = useState('');
  const [jsonError, setJsonError] = useState('');

  // Sync theme attribute on <html>
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

  const appendTui = useCallback((entry) => {
    setTuiFeed((prev) => [...prev.slice(-18), { id: `${Date.now()}-${Math.random()}`, ...entry }]);
  }, []);

  const selectAsset = useCallback((asset) => {
    setCurrentAsset(asset);
    setSelectedPartId(null);
    if (asset?.telemetry?.dimensions_mm) {
      setDimX(asset.telemetry.dimensions_mm.x);
      setDimY(asset.telemetry.dimensions_mm.y);
      setDimZ(asset.telemetry.dimensions_mm.z);
    }
    setBlueprintJson(asset?.blueprint ? JSON.stringify(asset.blueprint, null, 2) : '');
    setShowRig(Boolean(asset?.rig));
  }, []);

  const fetchInitialData = useCallback(async () => {
    try {
      const [hwRes, rtRes, asRes, gmRes] = await Promise.all([
        fetch('/api/hardware'),
        fetch('/api/settings/router'),
        fetch('/api/assets'),
        fetch('/api/games'),
      ]);
      if (hwRes.ok) {
        const hw = await hwRes.json();
        setHardware(hw);
        if (hw.profile?.sdf_grid_resolution) setSdfRes(hw.profile.sdf_grid_resolution);
      }
      if (rtRes.ok) setRouterCfg(await rtRes.json());
      if (asRes.ok) {
        const list = await asRes.json();
        setAssets(list);
        if (list.length > 0) selectAsset(list[0]);
      }
      if (gmRes.ok) {
        const gList = await gmRes.json();
        setGames(gList);
        if (gList.length > 0) setCurrentGame(gList[0]);
      }
    } catch (e) {
      console.error('Falcon boot error:', e);
    }
  }, [selectAsset]);

  useEffect(() => {
    fetchInitialData();
  }, [fetchInitialData]);

  const refreshAssetsAndSelect = useCallback(
    async (updated) => {
      const r = await fetch('/api/assets');
      if (r.ok) setAssets(await r.json());
      if (updated) selectAsset(updated);
    },
    [selectAsset]
  );

  // Global OpenCode Keyboard Shortcuts (Ctrl+P, Ctrl+B, Ctrl+I, Ctrl+,)
  useEffect(() => {
    const onKey = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'p') {
        e.preventDefault();
        setPaletteOpen((v) => !v);
      } else if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') {
        e.preventDefault();
        setPaletteOpen((v) => !v);
      } else if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'b') {
        e.preventDefault();
        setLeftOpen((v) => !v);
      } else if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'i') {
        e.preventDefault();
        setRightOpen((v) => !v);
      } else if ((e.ctrlKey || e.metaKey) && e.key === ',') {
        e.preventDefault();
        setHwModalOpen((v) => !v);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  const openToolDrawer = (tool) => {
    setActiveTool(tool);
    if (tool !== 'agent') {
      setRightOpen(true);
      setRightTab('tool');
    }
  };

  const triggerDownload = (fmt) => {
    if (!currentAsset) return;
    window.open(`/api/export/${currentAsset.id}/${fmt}`, '_blank');
    appendTui({
      kind: 'system',
      text: `exported ${currentAsset.title} -> .${fmt.toUpperCase()} [zero-paywall]`,
    });
  };

  // Synthesis, Swarm Detail, Game Creation & Edit Actions
  const runBuildFromPrompt = async (textPrompt) => {
    if (!textPrompt.trim()) return;
    setStageMode('model');
    setBusy(true);
    setBusyLabel(`6-agent swarm (${sdfRes}³) -> sculpting + kitbashing + pbr baking...`);
    appendTui({ kind: 'user', text: `[BUILD] ${textPrompt}` });
    try {
      const res = await fetch('/api/generate/text', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt: textPrompt, style: stylePreset, resolution: sdfRes, ultra_detail: true }),
      });
      if (res.ok) {
        const created = await res.json();
        await refreshAssetsAndSelect(created);
        appendTui({
          kind: 'system',
          text: `[+] 6-agent swarm built "${created.title}" · ${created.telemetry?.faces?.toLocaleString()} tris · ${created.parts?.length || 1} parts · ${created.generation_ms}ms`,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  const runModelDetailPass = async (instruction = 'Add everything needed ultra detail kitbash') => {
    if (!currentAsset) return;
    setStageMode('model');
    setBusy(true);
    setBusyLabel('@kitbash-detailer + @sculptor-csg adding high-detail assemblies...');
    appendTui({ kind: 'user', text: `[DETAIL] ${instruction}` });
    try {
      const res = await fetch('/api/swarm/detail', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ asset_id: currentAsset.id, instruction, resolution: Math.max(sdfRes, 108) }),
      });
      if (res.ok) {
        const updated = await res.json();
        await refreshAssetsAndSelect(updated);
        appendTui({
          kind: 'system',
          text: `[+] @kitbash-detailer upgraded "${updated.title}" -> ${updated.parts?.length} parts · ${updated.telemetry?.faces?.toLocaleString()} tris`,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  const runCreateGame = async (gamePrompt) => {
    if (!gamePrompt.trim()) return;
    setStageMode('game');
    setAgentMode('game');
    setBusy(true);
    setBusyLabel('@game-director + @sculptor-csg building playable 3d game & .glb cast...');
    appendTui({ kind: 'user', text: `[GAME BUILD] ${gamePrompt}` });
    try {
      const res = await fetch('/api/games/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt: gamePrompt }),
      });
      if (res.ok) {
        const createdGame = await res.json();
        const [gmRes, asRes] = await Promise.all([fetch('/api/games'), fetch('/api/assets')]);
        if (gmRes.ok) setGames(await gmRes.json());
        if (asRes.ok) setAssets(await asRes.json());
        setCurrentGame(createdGame);
        appendTui({
          kind: 'system',
          text: `[+] @game-director launched playable 3D game "${createdGame.title}" [${createdGame.genre}] — WASD / Click / E live!`,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  const runMutateGame = async (instruction) => {
    if (!currentGame) {
      return runCreateGame(instruction);
    }
    setStageMode('game');
    setBusy(true);
    setBusyLabel('@game-director mutating live 3d game world...');
    appendTui({ kind: 'user', text: `[GAME UPDATE] ${instruction}` });
    try {
      const res = await fetch(`/api/games/${currentGame.id}/mutate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ instruction }),
      });
      if (res.ok) {
        const out = await res.json();
        setCurrentGame(out.game);
        const gmRes = await fetch('/api/games');
        if (gmRes.ok) setGames(await gmRes.json());
        appendTui({
          kind: 'system',
          text: `[+] @game-director updated "${out.game.title}": ${out.summary}`,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  const triggerGameDownload = (fmt) => {
    if (!currentGame) return;
    window.open(`/api/games/${currentGame.id}/export/${fmt}`, '_blank');
    appendTui({
      kind: 'system',
      text: `exported 3d game "${currentGame.title}" -> .${fmt.toUpperCase()} [standalone]`,
    });
  };

  const runAgentEdit = async (instruction) => {
    if (!instruction.trim()) return;
    setBusy(true);
    setBusyLabel('falcon agent modifying 3d assembly...');
    appendTui({ kind: 'user', text: `[EDIT] ${instruction}` });
    try {
      const res = await fetch('/api/agent/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message: instruction,
          current_asset_id: currentAsset?.id || null,
        }),
      });
      if (res.ok) {
        const data = await res.json();
        await refreshAssetsAndSelect(data.asset);
        appendTui({ kind: 'system', text: `[+] ${data.reply}` });
      }
    } finally {
      setBusy(false);
    }
  };

  const runPlanFromPrompt = async (textPrompt) => {
    if (!textPrompt.trim()) return;
    setBusy(true);
    setBusyLabel('scoping read-only 3d blueprint plan...');
    appendTui({ kind: 'user', text: `[PLAN] ${textPrompt}` });
    setTuiExpanded(true);
    try {
      const res = await fetch('/api/agent/plan', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt: textPrompt }),
      });
      if (res.ok) {
        const plan = await res.json();
        appendTui({
          kind: 'plan',
          title: `PLAN READY // ${plan.title} (${plan.parts_count} parts · ${plan.provider})`,
          steps: plan.steps || [],
          blueprint: plan.blueprint,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  const executeApprovedBlueprint = async (bp) => {
    if (!bp) return;
    setBusy(true);
    setBusyLabel('compiling approved plan in c++20...');
    try {
      const res = await fetch('/api/generate/blueprint', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ blueprint: bp, resolution: sdfRes }),
      });
      if (res.ok) {
        const created = await res.json();
        setAgentMode('build');
        await refreshAssetsAndSelect(created);
        appendTui({
          kind: 'system',
          text: `[+] plan executed -> "${created.title}" (${created.telemetry?.faces?.toLocaleString()} tris in ${created.generation_ms}ms)`,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  const handleEnhancePrompt = async () => {
    if (!promptInput.trim()) return;
    const res = await fetch('/api/prompt/enhance', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ prompt: promptInput }),
    });
    if (res.ok) {
      const d = await res.json();
      setPromptInput(d.enhanced);
    }
  };

  const handlePrintRepair = async () => {
    if (!currentAsset) return;
    setBusy(true);
    setBusyLabel('running c++20 manifold & flat build-plate repair...');
    try {
      const res = await fetch(`/api/mesh/${currentAsset.id}/print-repair`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ flat_bottom_cut: true }),
      });
      if (res.ok) {
        const updated = await res.json();
        await refreshAssetsAndSelect(updated);
        appendTui({ kind: 'system', text: `[+] manifold repair complete -> flat Z=0 build plate verified.` });
      }
    } finally {
      setBusy(false);
    }
  };

  const handleAutoSplit = async (axis = 'y') => {
    if (!currentAsset) return;
    setBusy(true);
    setBusyLabel(`splitting along ${axis.toUpperCase()}-plane + 0.25mm alignment pins...`);
    try {
      const res = await fetch(`/api/mesh/${currentAsset.id}/auto-split`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ axis, explode_gap: 0.36 }),
      });
      if (res.ok) {
        const splitAsset = await res.json();
        await refreshAssetsAndSelect(splitAsset);
        appendTui({ kind: 'system', text: `[+] split into 2 capped halves + 0.25mm snap connector pins.` });
      }
    } finally {
      setBusy(false);
    }
  };

  const handleAutoRig = async (rigType = 'biped') => {
    if (!currentAsset) return;
    setBusy(true);
    setBusyLabel(`solving ${rigType} bone hierarchy...`);
    try {
      const res = await fetch(`/api/mesh/${currentAsset.id}/rig`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ rig_type: rigType }),
      });
      if (res.ok) {
        const updated = await res.json();
        await refreshAssetsAndSelect(updated);
        setShowRig(true);
        setActiveAnimation('idle_breathe');
        appendTui({ kind: 'system', text: `[+] rigged ${updated.rig?.bones_count || 15} bones · idle_breathe active.` });
      }
    } finally {
      setBusy(false);
    }
  };

  const handleApplyMaterial = async (override = null) => {
    if (!currentAsset) return;
    const payload = override || {
      color: matColor,
      metallic: matMetallic,
      roughness: matRoughness,
      finish: matFinish,
      emissive: matEmissiveOn ? matEmissive : '',
      emissive_strength: matEmissiveOn ? 0.85 : 0.0,
      part_id: selectedPartId || '',
    };
    setBusy(true);
    setBusyLabel('baking 256x256 albedo/orm/normal pbr maps...');
    try {
      const res = await fetch(`/api/mesh/${currentAsset.id}/material`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      if (res.ok) await refreshAssetsAndSelect(await res.json());
    } finally {
      setBusy(false);
    }
  };

  const handleResizeMm = async () => {
    if (!currentAsset) return;
    setBusy(true);
    setBusyLabel('scaling exact millimeter cad bounds...');
    try {
      const res = await fetch(`/api/mesh/${currentAsset.id}/resize`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ x_mm: Number(dimX), y_mm: Number(dimY), z_mm: Number(dimZ) }),
      });
      if (res.ok) {
        const updated = await res.json();
        await refreshAssetsAndSelect(updated);
        appendTui({ kind: 'system', text: `[+] resized exact cad bounds -> ${dimX} × ${dimY} × ${dimZ} mm.` });
      }
    } finally {
      setBusy(false);
    }
  };

  const handleTopologyOp = async (operation) => {
    if (!currentAsset) return;
    setBusy(true);
    setBusyLabel(`running c++20 ${operation} topology op...`);
    try {
      const res = await fetch(`/api/mesh/${currentAsset.id}/topology`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ operation, smooth_iterations: 4, target_ratio: 0.55 }),
      });
      if (res.ok) await refreshAssetsAndSelect(await res.json());
    } finally {
      setBusy(false);
    }
  };

  const handleImageUpload = async (fileOrBlob, filename = 'sketch.png') => {
    setBusy(true);
    setBusyLabel('c++20 libpng + 2d edt volumetric inflation...');
    try {
      const fd = new FormData();
      fd.append('file', fileOrBlob, filename);
      fd.append('mode', reconMode);
      fd.append('depth_strength', String(depthStrength));
      fd.append('detail_relief', String(detailRelief));
      fd.append('resolution', '112');
      const res = await fetch('/api/generate/image', { method: 'POST', body: fd });
      if (res.ok) {
        const created = await res.json();
        await refreshAssetsAndSelect(created);
        appendTui({
          kind: 'system',
          text: `[+] inflated 2d image -> "${created.title}" (${created.telemetry?.faces?.toLocaleString()} tris in ${created.generation_ms}ms)`,
        });
      }
    } finally {
      setBusy(false);
    }
  };

  const handleCompileJsonBlueprint = async () => {
    setJsonError('');
    let parsed;
    try {
      parsed = JSON.parse(blueprintJson);
    } catch (e) {
      setJsonError(`JSON syntax error: ${e.message}`);
      return;
    }
    setBusy(true);
    setBusyLabel('re-compiling json blueprint in c++20...');
    try {
      const res = await fetch('/api/generate/blueprint', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          blueprint: parsed,
          resolution: sdfRes,
          asset_id: currentAsset?.id || '',
        }),
      });
      if (res.ok) await refreshAssetsAndSelect(await res.json());
    } finally {
      setBusy(false);
    }
  };

  const handleDeleteAsset = async (e, id) => {
    e.stopPropagation();
    await fetch(`/api/assets/${id}`, { method: 'DELETE' });
    const r = await fetch('/api/assets');
    if (r.ok) {
      const list = await r.json();
      setAssets(list);
      if (currentAsset?.id === id && list.length > 0) selectAsset(list[0]);
    }
  };

  // Slash Menu Filtering & Execution
  const showSlashMenu = promptInput.startsWith('/');
  const matchingSlash = useMemo(() => {
    if (!showSlashMenu) return [];
    const q = promptInput.trim().toLowerCase();
    return SLASH_COMMANDS.filter((s) => s.cmd.startsWith(q) || s.desc.toLowerCase().includes(q.slice(1)));
  }, [showSlashMenu, promptInput]);

  const executeSlashCommand = (cmdStr) => {
    setPromptInput('');
    if (cmdStr === '/build') {
      setStageMode('model');
      setAgentMode('build');
    } else if (cmdStr === '/detail') {
      runModelDetailPass('Add everything needed ultra detail kitbash');
    } else if (cmdStr === '/game') {
      setStageMode('game');
      setAgentMode('game');
      openToolDrawer('swarm');
    } else if (cmdStr === '/swarm') {
      openToolDrawer('swarm');
    } else if (cmdStr === '/edit') setAgentMode('edit');
    else if (cmdStr === '/plan') setAgentMode('plan');
    else if (cmdStr === '/sketch') openToolDrawer('sketch');
    else if (cmdStr === '/gold') runAgentEdit('Make it forged gold with cyan plasma glow');
    else if (cmdStr === '/chrome') runAgentEdit('Upgrade surfaces to mirror chrome');
    else if (cmdStr === '/wings') runModelDetailPass('Add anatomical 4-fingered dragon wings');
    else if (cmdStr === '/pedestal') runAgentEdit('Add a 3D print pedestal base');
    else if (cmdStr === '/repair') handlePrintRepair();
    else if (cmdStr === '/split') handleAutoSplit('y');
    else if (cmdStr === '/rig') handleAutoRig('biped');
    else if (cmdStr === '/export game') triggerGameDownload('html');
    else if (cmdStr.startsWith('/export ')) triggerDownload(cmdStr.split(' ')[1]);
    else if (cmdStr === '/theme') setTheme((t) => (t === 'dark' ? 'cream' : 'dark'));
    else if (cmdStr === '/config') setHwModalOpen(true);
  };

  const handleComposerSubmit = () => {
    const text = promptInput.trim();
    if (!text || busy) return;
    if (text.startsWith('/')) {
      const target = matchingSlash[slashIndex]?.cmd || text;
      executeSlashCommand(target);
      return;
    }
    setPromptInput('');
    const low = text.toLowerCase();
    // Auto-route game requests even if user didn't toggle GAME mode first!
    const looksLikeNewGame =
      low.includes('create a game') ||
      low.includes('build a game') ||
      low.includes('make a game') ||
      low.includes('3d game') ||
      (agentMode === 'game' && (!currentGame || low.startsWith('create ') || low.startsWith('build ') || low.startsWith('new ')));

    if (looksLikeNewGame) runCreateGame(text);
    else if (agentMode === 'game' || stageMode === 'game') runMutateGame(text);
    else if (agentMode === 'detail') runModelDetailPass(text);
    else if (agentMode === 'plan') runPlanFromPrompt(text);
    else if (agentMode === 'edit') runAgentEdit(text);
    else runBuildFromPrompt(text);
  };

  const handleComposerKeyDown = (e) => {
    if (e.key === 'Tab') {
      e.preventDefault();
      if (showSlashMenu && matchingSlash[slashIndex]) {
        setPromptInput(matchingSlash[slashIndex].cmd);
        return;
      }
      setAgentMode((cur) => {
        const idx = AGENT_MODES.indexOf(cur);
        const next = AGENT_MODES[(idx + (e.shiftKey ? AGENT_MODES.length - 1 : 1)) % AGENT_MODES.length];
        return next;
      });
      return;
    }
    if (showSlashMenu && matchingSlash.length > 0) {
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        setSlashIndex((i) => (i + 1) % matchingSlash.length);
        return;
      }
      if (e.key === 'ArrowUp') {
        e.preventDefault();
        setSlashIndex((i) => (i - 1 + matchingSlash.length) % matchingSlash.length);
        return;
      }
    }
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleComposerSubmit();
    }
  };

  const handleDimChange = (axis, val) => {
    const num = Math.max(2, Number(val) || 2);
    const base = currentAsset?.telemetry?.dimensions_mm;
    if (lockAspect && base) {
      const scale =
        axis === 'x' ? num / Math.max(base.x, 0.1) : axis === 'y' ? num / Math.max(base.y, 0.1) : num / Math.max(base.z, 0.1);
      setDimX(Number((base.x * scale).toFixed(1)));
      setDimY(Number((base.y * scale).toFixed(1)));
      setDimZ(Number((base.z * scale).toFixed(1)));
    } else {
      if (axis === 'x') setDimX(num);
      if (axis === 'y') setDimY(num);
      if (axis === 'z') setDimZ(num);
    }
  };

  // Global Command Palette Actions
  const paletteCommands = useMemo(
    () => [
      { id: 'm-build', category: 'mode', label: 'Switch to BUILD mode (6-Agent Swarm 3D Synthesis)', shortcut: 'TAB', action: () => { setStageMode('model'); setAgentMode('build'); } },
      { id: 'm-detail', category: 'mode', label: 'Run @kitbash-detailer Ultra-Detail Pass on Active Model', shortcut: '/DETAIL', action: () => runModelDetailPass('Add everything needed ultra detail kitbash') },
      { id: 'm-game', category: 'game', label: 'Launch 3D Game Engine & Multi-Agent Game Studio', shortcut: '/GAME', action: () => { setStageMode('game'); setAgentMode('game'); openToolDrawer('swarm'); } },
      { id: 'm-edit', category: 'mode', label: 'Switch to EDIT mode (Agent In-Place Modify)', shortcut: 'TAB', action: () => setAgentMode('edit') },
      { id: 'm-plan', category: 'mode', label: 'Switch to PLAN mode (Read-Only Blueprint Scope)', shortcut: 'TAB', action: () => setAgentMode('plan') },
      { id: 'v-left', category: 'view', label: 'Toggle Left Sessions Sidebar', shortcut: 'CTRL+B', action: () => setLeftOpen((v) => !v) },
      { id: 'v-right', category: 'view', label: 'Toggle Right Inspector Panel', shortcut: 'CTRL+I', action: () => setRightOpen((v) => !v) },
      { id: 'v-theme', category: 'view', label: `Switch Theme (${theme === 'dark' ? 'Cream Paper #fdfcfc' : 'Obsidian Dark #131010'})`, shortcut: '/THEME', action: () => setTheme((t) => (t === 'dark' ? 'cream' : 'dark')) },
      { id: 't-sketch', category: 'tool', label: 'Open 2D Sketch & Image-to-3D Inflation Drawer', shortcut: '/SKETCH', action: () => openToolDrawer('sketch') },
      { id: 't-pbr', category: 'tool', label: 'Open PBR Shader & Studio Relighting Drawer', action: () => openToolDrawer('pbr') },
      { id: 't-print', category: 'tool', label: 'Open CAD Millimeter & 3D Print Doctor Drawer', action: () => openToolDrawer('print') },
      { id: 't-rig', category: 'tool', label: 'Auto-Rig Humanoid (15 Bones) & Animate', shortcut: '/RIG', action: () => handleAutoRig('biped') },
      { id: 't-repair', category: 'print', label: 'Run C++20 Manifold Repair & Flat Build-Plate Cut', shortcut: '/REPAIR', action: handlePrintRepair },
      { id: 't-split', category: 'print', label: 'Auto-Split Y-Plane + 0.25mm Alignment Pins', shortcut: '/SPLIT', action: () => handleAutoSplit('y') },
      { id: 'e-glb', category: 'export', label: 'Export glTF 2.0 PBR Binary (.GLB)', shortcut: '.GLB', action: () => triggerDownload('glb') },
      { id: 'e-3mf', category: 'export', label: 'Export Bambu / Orca Slicer Package (.3MF)', shortcut: '.3MF', action: () => triggerDownload('3mf') },
      { id: 'e-stl', category: 'export', label: 'Export Millimeter Binary Mesh (.STL)', shortcut: '.STL', action: () => triggerDownload('stl') },
      { id: 'e-cs', category: 'export', label: 'Export Unity C# Procedural Builder (.CS)', shortcut: '.CS', action: () => triggerDownload('cs') },
      { id: 'c-hw', category: 'system', label: 'Hardware Profiler & Universal Local LLM Router', shortcut: 'CTRL+,', action: () => setHwModalOpen(true) },
    ],
    [theme, currentAsset]
  );

  const tel = currentAsset?.telemetry;
  const activeSlug = (currentAsset?.title || 'untitled').toLowerCase().replace(/[^a-z0-9]+/g, '_');

  return (
    <div className="oc-app">
      {/* 1. Frameless OpenCode / Falcon AI Desktop Titlebar */}
      <header className="oc-titlebar">
        <div className="oc-brand">
          <div className="oc-wordmark">
            <span className="oc-pixel-glyph">F</span>
            <span>FALCON</span>
            <span style={{ color: 'var(--ink-muted)', fontWeight: 400, fontSize: 10.5 }}>
              [v2.0 · c++20]
            </span>
          </div>
          <span className="oc-uri">falcon://workspace/{activeSlug}.glb</span>
          <span
            style={{
              fontSize: 10.5,
              color: 'var(--sig-green)',
              paddingLeft: 8,
              borderLeft: '1px solid var(--hairline)',
            }}
          >
            [● daemon online · 0 credits]
          </span>
        </div>

        <div className="oc-title-actions oc-no-drag">
          <button
            className={`oc-ghost-btn ${leftOpen ? 'active' : ''}`}
            onClick={() => setLeftOpen((v) => !v)}
            title="Toggle Left Sidebar (Ctrl+B)"
          >
            [sidebar]
          </button>
          <button
            className={`oc-ghost-btn ${rightOpen ? 'active' : ''}`}
            onClick={() => setRightOpen((v) => !v)}
            title="Toggle Right Inspector (Ctrl+I)"
          >
            [inspector]
          </button>
          <button className="oc-ghost-btn" onClick={() => setPaletteOpen(true)}>
            <span>&gt; commands</span>
            <span className="oc-kbd">ctrl+p</span>
          </button>
          <button
            className="oc-ghost-btn"
            onClick={() => setTheme((t) => (t === 'dark' ? 'cream' : 'dark'))}
            title="Switch OpenCode Obsidian / Cream Theme"
          >
            [{theme === 'dark' ? 'cream' : 'dark'}]
          </button>
          <button className="oc-ghost-btn" onClick={() => setHwModalOpen(true)}>
            [{hardware?.profile?.tier === 'cpu_cpp_native' ? '0-gpu c++20' : 'gpu c++20'}]
          </button>

          {!isElectron && (
            <>
              <button
                className="oc-ghost-btn"
                onClick={() => { window.location.href = '/setup'; }}
                title="Preview Live-Animated 3D Holographic Setup Wizard"
              >
                [setup ui]
              </button>
              <button
                className="oc-ghost-btn"
                onClick={() => window.open('/download/source.zip', '_blank')}
                title="Download MIT Open-Source Repository Source Code (.zip)"
              >
                [↓ src.zip]
              </button>
              <button
                className="oc-btn oc-btn-primary oc-btn-sm"
                onClick={() => window.open('/download/setup.exe', '_blank')}
                title="Download Live-Animated Windows Setup Executable (Falcon-AI-Setup-2.0.0-x64.exe)"
              >
                [↓ setup.exe]
              </button>
            </>
          )}

          {isElectron && (
            <div className="oc-win-controls">
              <button className="oc-win-btn" onClick={() => window.falconDesktop.minimize()} title="Minimize">
                ─
              </button>
              <button className="oc-win-btn" onClick={() => window.falconDesktop.maximize()} title="Maximize">
                □
              </button>
              <button className="oc-win-btn close" onClick={() => window.falconDesktop.close()} title="Close">
                ✕
              </button>
            </div>
          )}
        </div>
      </header>

      {/* 2. 3-Column OpenCode Workbench */}
      <div className="oc-workbench">
        {/* LEFT SIDEBAR: Sessions, Workbench Modes & Quick Prompts */}
        {leftOpen && (
          <aside className="oc-left-sidebar">
            <div className="oc-section" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
              <button
                className={`oc-btn oc-btn-sm ${stageMode === 'model' ? 'oc-btn-primary' : ''}`}
                onClick={() => {
                  setStageMode('model');
                  setAgentMode('build');
                  composerRef.current?.focus();
                }}
              >
                + 3d model
              </button>
              <button
                className={`oc-btn oc-btn-sm ${stageMode === 'game' ? 'oc-btn-primary' : ''}`}
                onClick={() => {
                  setStageMode('game');
                  setAgentMode('game');
                  openToolDrawer('swarm');
                }}
              >
                ▶ 3d game
              </button>
            </div>

            <div className="oc-section">
              <div className="oc-section-label">
                <span>workbench</span>
                <span>agents & tools</span>
              </div>
              <div className="oc-nav-list">
                {[
                  { id: 'swarm', label: '6-agent swarm & game', kbd: '1' },
                  { id: 'sketch', label: 'sketch & img-to-3d', kbd: '2' },
                  { id: 'pbr', label: 'pbr shader & light', kbd: '3' },
                  { id: 'print', label: 'print & cad mm', kbd: '4' },
                  { id: 'rig', label: 'skeleton & motion', kbd: '5' },
                  { id: 'code', label: 'blueprint & c#', kbd: '6' },
                ].map((t) => (
                  <button
                    key={t.id}
                    className={`oc-nav-row ${activeTool === t.id ? 'active' : ''}`}
                    onClick={() => openToolDrawer(t.id)}
                  >
                    <span>
                      {activeTool === t.id ? '[>] ' : '[ ] '}
                      {t.label}
                    </span>
                    <span className="oc-kbd">{t.kbd}</span>
                  </button>
                ))}
              </div>
            </div>

            <div className="oc-section">
              <div className="oc-section-label">
                <span>quick starters</span>
                <span>1-click</span>
              </div>
              <div className="chip-grid">
                {PRESET_PROMPTS.map((p) => (
                  <button
                    key={p.label}
                    className="chip"
                    disabled={busy}
                    onClick={() => runBuildFromPrompt(p.prompt)}
                  >
                    /{p.label}
                  </button>
                ))}
              </div>
            </div>

            {games.length > 0 && (
              <div className="oc-section">
                <div className="oc-section-label">
                  <span>playable 3d games ({games.length})</span>
                  <span style={{ color: 'var(--sig-green)' }}>60fps</span>
                </div>
                <div className="oc-nav-list">
                  {games.map((gm) => {
                    const sel = stageMode === 'game' && currentGame?.id === gm.id;
                    return (
                      <div
                        key={gm.id}
                        className={`oc-nav-row ${sel ? 'active' : ''}`}
                        onClick={() => {
                          setCurrentGame(gm);
                          setStageMode('game');
                          setAgentMode('game');
                        }}
                      >
                        <div style={{ overflow: 'hidden', flex: 1 }}>
                          <div style={{ whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                            {sel ? '[▶] ' : '    '}
                            {gm.title}
                          </div>
                          <div style={{ fontSize: 10, color: 'var(--ink-muted)', paddingLeft: 20 }}>
                            {gm.genre} · {gm.props?.length || 6} keeps · boss
                          </div>
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>
            )}

            <div
              className="oc-section"
              style={{ flex: 1, overflowY: 'auto', borderBottom: 'none', display: 'flex', flexDirection: 'column' }}
            >
              <div className="oc-section-label">
                <span>sessions ({assets.length})</span>
                <span>.glb</span>
              </div>
              <div className="oc-nav-list" style={{ overflowY: 'auto' }}>
                {assets.map((a) => {
                  const sel = currentAsset?.id === a.id;
                  return (
                    <div
                      key={a.id}
                      className={`oc-nav-row ${stageMode === 'model' && sel ? 'active' : ''}`}
                      onClick={() => {
                        setStageMode('model');
                        selectAsset(a);
                      }}
                    >
                      <div style={{ overflow: 'hidden', flex: 1 }}>
                        <div style={{ whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                          {sel ? '[•] ' : '    '}
                          {a.title}
                        </div>
                        <div style={{ fontSize: 10, color: 'var(--ink-muted)', paddingLeft: 20 }}>
                          {(a.telemetry?.faces || 0).toLocaleString()} tris · {a.generation_ms}ms
                        </div>
                      </div>
                      {assets.length > 1 && (
                        <span
                          onClick={(e) => handleDeleteAsset(e, a.id)}
                          style={{ color: 'var(--ink-muted)', padding: '0 4px' }}
                          title="Delete session"
                        >
                          [x]
                        </span>
                      )}
                    </div>
                  );
                })}
              </div>
            </div>
          </aside>
        )}

        {/* CENTER STAGE: Hairline Viewport Bar + 3D Canvas + TUI Feed + OpenCode Bottom Composer */}
        <main className="oc-center-stage">
          {/* Top Hairline Stage Switcher & Shading/Camera Strip */}
          <div className="oc-stage-toolbar">
            <div className="oc-pill-group">
              <button
                className={`oc-pill ${stageMode === 'model' ? 'active' : ''}`}
                onClick={() => setStageMode('model')}
              >
                [3d model studio]
              </button>
              <button
                className={`oc-pill ${stageMode === 'game' ? 'active' : ''}`}
                onClick={() => {
                  setStageMode('game');
                  setAgentMode('game');
                }}
              >
                [▶ 3d game engine]
              </button>
              {stageMode === 'model' && (
                <>
                  <span style={{ color: 'var(--hairline-strong)', padding: '0 2px' }}>|</span>
                  {[
                    { id: 'pbr', label: 'pbr' },
                    { id: 'pbr_wire', label: 'pbr+wire' },
                    { id: 'wireframe', label: 'wire' },
                    { id: 'clay', label: 'clay' },
                    { id: 'normals', label: 'normals' },
                    { id: 'overhang', label: 'overhangs' },
                    { id: 'xray', label: 'xray' },
                  ].map((m) => (
                    <button
                      key={m.id}
                      className={`oc-pill ${shadingMode === m.id ? 'active' : ''}`}
                      onClick={() => setShadingMode(m.id)}
                    >
                      {m.label}
                    </button>
                  ))}
                </>
              )}
            </div>

            {stageMode === 'model' ? (
              <div className="oc-pill-group">
                <button
                  className="oc-pill"
                  style={{ color: 'var(--sig-green)', borderColor: 'var(--sig-green)' }}
                  disabled={busy || !currentAsset}
                  onClick={() => runModelDetailPass('Add everything needed ultra detail kitbash')}
                  title="Run @kitbash-detailer to add ultra-detail parts & micro-textures"
                >
                  [+ ultra detail]
                </button>
                {['iso', 'front', 'side', 'top'].map((cam) => (
                  <button
                    key={cam}
                    className="oc-pill"
                    onClick={() => setCameraCommand({ type: cam, ts: Date.now() })}
                  >
                    [{cam}]
                  </button>
                ))}
                <button
                  className={`oc-pill ${showGrid ? 'active' : ''}`}
                  onClick={() => setShowGrid((g) => !g)}
                >
                  [grid]
                </button>
                <button
                  className={`oc-pill ${autoRotate ? 'active' : ''}`}
                  onClick={() => setAutoRotate((r) => !r)}
                >
                  [spin]
                </button>
                <button
                  className="oc-pill"
                  onClick={() => setCameraCommand({ type: 'screenshot', ts: Date.now() })}
                >
                  [snap.png]
                </button>
              </div>
            ) : (
              <div className="oc-pill-group">
                <span style={{ fontSize: 11, color: 'var(--sig-green)' }}>
                  [● {currentGame?.title || '3D Game Ready'} · {currentGame?.genre || 'action_rpg'}]
                </span>
              </div>
            )}
          </div>

          {/* 3D WebGL Model Viewport OR Live 60-FPS Playable 3D Game Engine */}
          <div className="oc-viewport-wrap">
            {stageMode === 'game' && currentGame ? (
              <GameViewport
                game={currentGame}
                onExportGameHtml={() => triggerGameDownload('html')}
                onExportGameCs={() => triggerGameDownload('cs')}
                onQuickMutate={(cmd) => runMutateGame(cmd)}
              />
            ) : (
              <Viewport3D
                glbUrl={currentAsset?.glb_url}
                shadingMode={shadingMode}
                lightingPreset={lightingPreset}
                showGrid={showGrid}
                autoRotate={autoRotate}
                selectedPartId={selectedPartId}
                onSelectPart={(pid) => setSelectedPartId(pid)}
                rigData={currentAsset?.rig}
                showRig={showRig}
                activeAnimation={activeAnimation}
                cameraCommand={cameraCommand}
                theme={theme}
              />
            )}

            {busy && (
              <div className="loading-overlay">
                <div className="spinner" />
                <div style={{ fontSize: 11.5 }}>{busyLabel}</div>
              </div>
            )}
          </div>

          {/* Monospace Telemetry Status Strip */}
          {tel && (
            <div className="oc-telemetry-bar">
              <div style={{ display: 'flex', gap: 14 }}>
                <span>verts: <strong>{tel.vertices.toLocaleString()}</strong></span>
                <span>tris: <strong>{tel.faces.toLocaleString()}</strong></span>
                <span>parts: <strong>{tel.parts_count}</strong></span>
                <span>
                  dims: <strong>{tel.dimensions_mm.x}×{tel.dimensions_mm.y}×{tel.dimensions_mm.z}mm</strong>
                </span>
              </div>
              <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
                <span style={{ color: tel.watertight ? 'var(--sig-green)' : 'var(--sig-amber)' }}>
                  {tel.watertight ? '[+] watertight manifold' : '[!] non-manifold (/repair)'}
                </span>
                <span>latency: <strong>{currentAsset.generation_ms}ms</strong></span>
                <button
                  className="oc-pill"
                  onClick={() => setTuiExpanded((v) => !v)}
                  style={{ padding: '0 5px' }}
                >
                  {tuiExpanded ? '[hide log]' : `[show log (${tuiFeed.length})]`}
                </button>
              </div>
            </div>
          )}

          {/* OpenCode TUI Execution & Plan Stream */}
          {tuiExpanded && tuiFeed.length > 0 && (
            <div className="oc-tui-feed">
              {tuiFeed.map((item) => (
                <div key={item.id} className={`oc-tui-entry ${item.kind}`}>
                  {item.kind === 'plan' ? (
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <strong style={{ color: 'var(--sig-amber)' }}>{item.title}</strong>
                        <button
                          className="oc-btn oc-btn-primary oc-btn-sm"
                          disabled={busy}
                          onClick={() => executeApprovedBlueprint(item.blueprint)}
                        >
                          [execute build plan -&gt;]
                        </button>
                      </div>
                      {item.steps.map((st, i) => (
                        <div key={i} style={{ fontSize: 11, color: 'var(--ink-secondary)' }}>
                          {st}
                        </div>
                      ))}
                    </div>
                  ) : (
                    <span>{item.text}</span>
                  )}
                </div>
              ))}
            </div>
          )}

          {/* OpenCode Bottom Prompt Composer */}
          <div className="oc-composer-dock">
            {showSlashMenu && matchingSlash.length > 0 && (
              <div className="oc-slash-menu">
                {matchingSlash.map((s, idx) => (
                  <button
                    key={s.cmd}
                    className={`oc-slash-item ${idx === slashIndex ? 'selected' : ''}`}
                    onMouseEnter={() => setSlashIndex(idx)}
                    onClick={() => executeSlashCommand(s.cmd)}
                  >
                    <strong>{s.cmd}</strong>
                    <span style={{ color: 'var(--ink-muted)', fontSize: 11 }}>{s.desc}</span>
                  </button>
                ))}
              </div>
            )}

            <div className="oc-composer-box">
              <span style={{ color: 'var(--ink-muted)', fontWeight: 700, paddingBottom: 2 }}>&gt;</span>
              <textarea
                ref={composerRef}
                rows={1}
                className="oc-composer-input"
                value={promptInput}
                onChange={(e) => {
                  setPromptInput(e.target.value);
                  setSlashIndex(0);
                }}
                onKeyDown={handleComposerKeyDown}
                placeholder={
                  agentMode === 'game' || stageMode === 'game'
                    ? 'Game Swarm: "create a mecha shooter game" or mutate live ("add dragon boss, turrets & storm rain")...'
                    : agentMode === 'detail'
                    ? 'Detail Swarm (@kitbash-detailer): "add shoulder cannons, arcane crystals, sword & stone arch"...'
                    : agentMode === 'plan'
                    ? 'Plan mode (read-only): describe a 3D asset to scope its multi-part architecture...'
                    : agentMode === 'edit'
                    ? 'Edit mode: modify active model ("make it forged gold", "add swept wings", "30% taller")...'
                    : 'Ask the 6-Agent Swarm to build any ultra-detailed 3D model or 3D game, or type /...'
                }
              />
              <button
                className="oc-ghost-btn"
                onClick={handleEnhancePrompt}
                disabled={busy || !promptInput.trim() || promptInput.startsWith('/')}
                title="Expand prompt with PBR & geometric detail"
              >
                [enhance]
              </button>
              <button
                className="oc-btn oc-btn-primary oc-btn-sm"
                disabled={busy || !promptInput.trim()}
                onClick={handleComposerSubmit}
              >
                run ↵
              </button>
            </div>

            {/* Signature OpenCode Status & Mode Row under Composer */}
            <div className="oc-composer-footer">
              <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>
                <button
                  className={`oc-mode-badge ${agentMode}`}
                  onClick={() =>
                    setAgentMode((cur) => AGENT_MODES[(AGENT_MODES.indexOf(cur) + 1) % AGENT_MODES.length])
                  }
                  title="Press Tab to cycle Build / Edit / Plan"
                >
                  {agentMode.toUpperCase()}
                </button>

                <button className="oc-ghost-btn" onClick={() => setHwModalOpen(true)}>
                  model: {hardware?.profile?.llm_mode === 'builtin_symbolic' ? 'c++20-symbolic' : hardware?.profile?.llm_mode || 'auto'}
                </button>

                <select
                  className="oc-ghost-btn"
                  value={sdfRes}
                  onChange={(e) => setSdfRes(Number(e.target.value))}
                  title="C++20 OpenMP Voxel Grid Resolution"
                >
                  <option value={68}>grid: 68³ (draft)</option>
                  <option value={92}>grid: 92³ (fast)</option>
                  <option value={104}>grid: 104³ (hd)</option>
                  <option value={112}>grid: 112³ (hd+)</option>
                  <option value={128}>grid: 128³ (ultra)</option>
                </select>

                <select
                  className="oc-ghost-btn"
                  value={stylePreset}
                  onChange={(e) => setStylePreset(e.target.value)}
                >
                  <option value="realistic">style: realistic-pbr</option>
                  <option value="hard_surface">style: hard-surface</option>
                  <option value="stylized">style: stylized</option>
                  <option value="sculpture">style: clay-sculpt</option>
                  <option value="voxel">style: low-poly</option>
                </select>
              </div>

              <div style={{ fontSize: 10.5, color: 'var(--ink-muted)' }}>
                <span className="oc-kbd">tab</span> cycle agent · <span className="oc-kbd">/</span> commands ·{' '}
                <span className="oc-kbd">ctrl+p</span> palette
              </div>
            </div>
          </div>
        </main>

        {/* RIGHT COLLAPSIBLE INSPECTOR PANEL */}
        {rightOpen && (
          <aside className="oc-right-panel">
            <div className="oc-tab-strip">
              {[
                { id: 'parts', label: 'parts' },
                { id: 'tool', label: `tool (${activeTool})` },
                { id: 'export', label: 'slicer & export' },
              ].map((tb) => (
                <button
                  key={tb.id}
                  className={`oc-tab-btn ${rightTab === tb.id ? 'active' : ''}`}
                  onClick={() => setRightTab(tb.id)}
                >
                  {tb.label}
                </button>
              ))}
            </div>

            <div className="oc-panel-body">
              {/* TAB 1: SEMANTIC PART HIERARCHY */}
              {rightTab === 'parts' && currentAsset && (
                <>
                  <div className="field-group">
                    <div className="field-label">
                      <span>active session</span>
                      <span>{currentAsset.source_mode}</span>
                    </div>
                    <div style={{ fontWeight: 700, fontSize: 12.5 }}>{currentAsset.title}</div>
                    <div style={{ fontSize: 10.5, color: 'var(--ink-muted)' }}>{currentAsset.engine_used}</div>
                  </div>

                  <div className="field-group">
                    <div className="field-label">
                      <span>semantic parts ({currentAsset.parts?.length || 1})</span>
                      {selectedPartId && (
                        <span
                          style={{ color: 'var(--ink-primary)', cursor: 'pointer' }}
                          onClick={() => setSelectedPartId(null)}
                        >
                          [show all]
                        </span>
                      )}
                    </div>
                    <div className="part-list">
                      {(currentAsset.parts || []).map((p) => {
                        const sel = selectedPartId === p.id;
                        return (
                          <div
                            key={p.id}
                            className={`part-row ${sel ? 'selected' : ''}`}
                            onClick={() => setSelectedPartId(sel ? null : p.id)}
                          >
                            <div style={{ display: 'flex', alignItems: 'center', gap: 7, overflow: 'hidden' }}>
                              <span className="swatch-dot" style={{ background: p.color || '#94a3b8' }} />
                              <span style={{ whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                                {sel ? '[*] ' : ''}
                                {p.name}
                              </span>
                            </div>
                            <span style={{ fontSize: 10, color: 'var(--ink-muted)' }}>{p.primitive}</span>
                          </div>
                        );
                      })}
                    </div>
                  </div>

                  <div className="field-group">
                    <div className="field-label">quick agent edits</div>
                    <div className="chip-grid">
                      {[
                        'Make it forged gold with cyan glow',
                        'Upgrade to mirror chrome',
                        'Change to obsidian & purple plasma',
                        'Add swept wings',
                        'Add a 3D print pedestal base',
                        'Make it 30% taller',
                      ].map((c) => (
                        <button key={c} className="chip" disabled={busy} onClick={() => runAgentEdit(c)}>
                          {c}
                        </button>
                      ))}
                    </div>
                  </div>
                </>
              )}

              {/* TAB 2: CONTEXT-AWARE WORKBENCH TOOL */}
              {rightTab === 'tool' && (
                <>
                  <div className="chip-grid">
                    {['swarm', 'sketch', 'pbr', 'print', 'rig', 'code'].map((t) => (
                      <button
                        key={t}
                        className={`oc-pill ${activeTool === t ? 'active' : ''}`}
                        onClick={() => setActiveTool(t)}
                      >
                        [{t}]
                      </button>
                    ))}
                  </div>

                  {activeTool === 'swarm' && (
                    <SwarmPanel
                      busy={busy}
                      stageMode={stageMode}
                      setStageMode={setStageMode}
                      currentAsset={currentAsset}
                      currentGame={currentGame}
                      onRunModelDetail={(cmd) => runModelDetailPass(cmd)}
                      onCreateGame={(pr) => runCreateGame(pr)}
                      onMutateGame={(cmd) => runMutateGame(cmd)}
                      onExportGameHtml={() => triggerGameDownload('html')}
                      onExportGameCs={() => triggerGameDownload('cs')}
                    />
                  )}

                  {activeTool === 'sketch' && (
                    <div className="field-group">
                      <div className="field-label">
                        <span>2d sketch / image -&gt; 3d inflation</span>
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
                        <button
                          className={`oc-btn oc-btn-sm ${imgSubMode === 'sketch' ? 'oc-btn-primary' : ''}`}
                          onClick={() => setImgSubMode('sketch')}
                        >
                          draw 2d sketch
                        </button>
                        <button
                          className={`oc-btn oc-btn-sm ${imgSubMode === 'upload' ? 'oc-btn-primary' : ''}`}
                          onClick={() => setImgSubMode('upload')}
                        >
                          upload png/jpg
                        </button>
                      </div>

                      <select
                        className="select-input"
                        value={reconMode}
                        onChange={(e) => setReconMode(e.target.value)}
                      >
                        <option value="volumetric">360° volumetric cushion inflation</option>
                        <option value="medallion">2.5d bas-relief coin / medallion</option>
                      </select>

                      <div className="field-label">
                        <span>inflation depth</span>
                        <span>{Math.round(depthStrength * 100)}%</span>
                      </div>
                      <input
                        type="range"
                        min={0.2}
                        max={1.0}
                        step={0.05}
                        value={depthStrength}
                        onChange={(e) => setDepthStrength(Number(e.target.value))}
                      />

                      {imgSubMode === 'sketch' ? (
                        <SketchPad
                          busy={busy}
                          onGenerateFromSketch={(blob) => handleImageUpload(blob, 'Falcon_Sketch.png')}
                        />
                      ) : (
                        <label
                          style={{
                            border: '1px dashed var(--hairline-strong)',
                            borderRadius: 4,
                            padding: 20,
                            textAlign: 'center',
                            cursor: 'pointer',
                            background: 'var(--bg-surface)',
                          }}
                        >
                          <div>[+] click to upload png / jpeg</div>
                          <div style={{ fontSize: 10, color: 'var(--ink-muted)', marginTop: 4 }}>
                            decoded natively via c++20 libpng/libjpeg
                          </div>
                          <input
                            type="file"
                            accept="image/png,image/jpeg"
                            style={{ display: 'none' }}
                            onChange={(e) => {
                              const f = e.target.files?.[0];
                              if (f) handleImageUpload(f, f.name);
                            }}
                          />
                        </label>
                      )}
                    </div>
                  )}

                  {activeTool === 'pbr' && (
                    <div className="field-group">
                      <div className="field-label">
                        <span>scope: {selectedPartId || 'all parts'}</span>
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
                        {PBR_PRESETS.map((p) => (
                          <button
                            key={p.name}
                            className="oc-btn oc-btn-sm"
                            style={{ justifyContent: 'flex-start' }}
                            disabled={busy}
                            onClick={() => {
                              setMatColor(p.color);
                              setMatMetallic(p.metallic);
                              setMatRoughness(p.roughness);
                              if (p.finish) setMatFinish(p.finish);
                              handleApplyMaterial({
                                color: p.color,
                                metallic: p.metallic,
                                roughness: p.roughness,
                                finish: p.finish || matFinish,
                                emissive: p.emissive || '',
                                emissive_strength: p.estr,
                                part_id: selectedPartId || '',
                              });
                            }}
                          >
                            <span className="swatch-dot" style={{ background: p.color }} />
                            {p.name}
                          </button>
                        ))}
                      </div>

                      <div className="field-label" style={{ marginTop: 6 }}>
                        <span>normal / orm micro-texture</span>
                      </div>
                      <select
                        className="select-input"
                        value={matFinish}
                        onChange={(e) => setMatFinish(e.target.value)}
                      >
                        <option value="forged_metal">forged-metal (brushed grain)</option>
                        <option value="damascus_steel">damascus-steel (folded ripples)</option>
                        <option value="gold_filigree">gold-filigree (baroque scrollwork)</option>
                        <option value="obsidian_crystal">obsidian-crystal (conchoidal glass)</option>
                        <option value="dragon_scales">dragon-scales (imbricated armor)</option>
                        <option value="carbon_fiber">carbon-fiber (2x2 twill weave)</option>
                        <option value="painted_armor">painted-armor (beveled seams)</option>
                        <option value="rusted_iron">rusted-iron (battle-worn pitting)</option>
                        <option value="carved_bone">carved-bone (striated horn/ivory)</option>
                        <option value="weathered_stone">weathered-stone (ashlar rock)</option>
                        <option value="wrapped_leather">wrapped-leather (pebbled hide)</option>
                        <option value="bark_wood">bark-wood (deep organic grain)</option>
                        <option value="rubber_tire">rubber-tire (herringbone tread)</option>
                      </select>

                      <div className="field-label" style={{ marginTop: 6 }}>
                        <span>albedo hex</span>
                        <span>{matColor}</span>
                      </div>
                      <input
                        type="color"
                        value={matColor}
                        onChange={(e) => setMatColor(e.target.value)}
                        style={{ width: '100%', height: 28, background: 'transparent', border: '1px solid var(--hairline)' }}
                      />

                      <div className="field-label">
                        <span>metallic</span>
                        <span>{matMetallic.toFixed(2)}</span>
                      </div>
                      <input
                        type="range"
                        min={0}
                        max={1}
                        step={0.02}
                        value={matMetallic}
                        onChange={(e) => setMatMetallic(Number(e.target.value))}
                      />

                      <div className="field-label">
                        <span>roughness</span>
                        <span>{matRoughness.toFixed(2)}</span>
                      </div>
                      <input
                        type="range"
                        min={0.04}
                        max={1}
                        step={0.02}
                        value={matRoughness}
                        onChange={(e) => setMatRoughness(Number(e.target.value))}
                      />

                      <label style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 11 }}>
                        <input
                          type="checkbox"
                          checked={matEmissiveOn}
                          onChange={(e) => setMatEmissiveOn(e.target.checked)}
                        />
                        enable emissive glow ({matEmissive})
                      </label>

                      <button className="oc-btn oc-btn-primary" disabled={busy} onClick={() => handleApplyMaterial()}>
                        [bake pbr material]
                      </button>

                      <div className="field-label" style={{ marginTop: 8 }}>studio environment rig</div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
                        {['studio', 'cyberpunk', 'forge', 'showroom'].map((lp) => (
                          <button
                            key={lp}
                            className={`oc-btn oc-btn-sm ${lightingPreset === lp ? 'oc-btn-primary' : ''}`}
                            onClick={() => setLightingPreset(lp)}
                          >
                            {lp}
                          </button>
                        ))}
                      </div>
                    </div>
                  )}

                  {activeTool === 'print' && (
                    <div className="field-group">
                      <div className="field-label">
                        <span>exact cad dimensions (mm)</span>
                        <label style={{ textTransform: 'none', cursor: 'pointer' }}>
                          <input
                            type="checkbox"
                            checked={lockAspect}
                            onChange={(e) => setLockAspect(e.target.checked)}
                          />{' '}
                          lock ratio
                        </label>
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 5 }}>
                        {[
                          { k: 'x', v: dimX },
                          { k: 'y', v: dimY },
                          { k: 'z', v: dimZ },
                        ].map((ax) => (
                          <div key={ax.k}>
                            <div style={{ fontSize: 10, color: 'var(--ink-muted)' }}>{ax.k.toUpperCase()} (mm)</div>
                            <input
                              type="number"
                              className="text-input"
                              value={ax.v}
                              onChange={(e) => handleDimChange(ax.k, e.target.value)}
                            />
                          </div>
                        ))}
                      </div>
                      <button className="oc-btn" disabled={busy} onClick={handleResizeMm}>
                        [apply exact mm scale]
                      </button>

                      <div className="field-label" style={{ marginTop: 8 }}>c++20 topology ops</div>
                      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 5 }}>
                        <button className="oc-btn oc-btn-sm" disabled={busy} onClick={() => handleTopologyOp('smooth')}>
                          smooth
                        </button>
                        <button className="oc-btn oc-btn-sm" disabled={busy} onClick={() => handleTopologyOp('subdivide')}>
                          subdivide
                        </button>
                        <button className="oc-btn oc-btn-sm" disabled={busy} onClick={() => handleTopologyOp('decimate')}>
                          decimate
                        </button>
                      </div>

                      <div className="field-label" style={{ marginTop: 8 }}>3d print doctor &amp; split</div>
                      <button className="oc-btn oc-btn-primary" disabled={busy} onClick={handlePrintRepair}>
                        [manifold repair + flat bed cut]
                      </button>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
                        <button className="oc-btn oc-btn-sm" disabled={busy} onClick={() => handleAutoSplit('y')}>
                          split Y + pins
                        </button>
                        <button className="oc-btn oc-btn-sm" disabled={busy} onClick={() => handleAutoSplit('x')}>
                          split X + pins
                        </button>
                      </div>
                    </div>
                  )}

                  {activeTool === 'rig' && (
                    <div className="field-group">
                      <div className="field-label">proportional auto-rigger</div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
                        <button className="oc-btn oc-btn-primary" disabled={busy} onClick={() => handleAutoRig('biped')}>
                          rig biped (15b)
                        </button>
                        <button className="oc-btn" disabled={busy} onClick={() => handleAutoRig('quadruped')}>
                          rig creature (10b)
                        </button>
                      </div>

                      <label style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, marginTop: 4 }}>
                        <input type="checkbox" checked={showRig} onChange={(e) => setShowRig(e.target.checked)} />
                        xray bone overlay
                      </label>

                      <div className="field-label" style={{ marginTop: 6 }}>motion preview</div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 5 }}>
                        {['rest', 'idle_breathe', 'walk_stride', 'hero_action', 'hover_float', 'turntable'].map((an) => (
                          <button
                            key={an}
                            className={`oc-btn oc-btn-sm ${activeAnimation === an ? 'oc-btn-primary' : ''}`}
                            onClick={() => setActiveAnimation(an)}
                          >
                            {an}
                          </button>
                        ))}
                      </div>
                    </div>
                  )}

                  {activeTool === 'code' && (
                    <div className="field-group">
                      <div className="field-label">live json blueprint -&gt; c++20</div>
                      {blueprintJson ? (
                        <>
                          <textarea
                            className="textarea-input"
                            style={{ minHeight: 240, fontSize: 10.5 }}
                            value={blueprintJson}
                            onChange={(e) => setBlueprintJson(e.target.value)}
                          />
                          {jsonError && <div style={{ color: 'var(--sig-red)', fontSize: 10.5 }}>{jsonError}</div>}
                          <div style={{ display: 'flex', gap: 5 }}>
                            <button className="oc-btn oc-btn-primary" style={{ flex: 1 }} onClick={handleCompileJsonBlueprint}>
                              [re-compile]
                            </button>
                            <button className="oc-btn" onClick={() => triggerDownload('cs')}>
                              [unity .cs]
                            </button>
                          </div>
                        </>
                      ) : (
                        <div style={{ color: 'var(--ink-muted)', fontSize: 11 }}>
                          select a prompt/agent session to live-edit its semantic json blueprint.
                        </div>
                      )}
                    </div>
                  )}
                </>
              )}

              {/* TAB 3: SLICER TELEMETRY & ZERO-PAYWALL EXPORT MATRIX */}
              {rightTab === 'export' && currentAsset && (
                <>
                  {tel && (
                    <div className="field-group">
                      <div className="field-label">slicer &amp; print telemetry</div>
                      <div className="stat-grid-2">
                        <div className="stat-box">
                          <div className="k">VOLUME</div>
                          <div className="v">{tel.volume_cm3} cm³</div>
                        </div>
                        <div className="stat-box">
                          <div className="k">OVERHANG &gt;45°</div>
                          <div
                            className="v"
                            style={{ color: tel.needs_supports ? 'var(--sig-amber)' : 'var(--sig-green)' }}
                          >
                            {tel.overhang_percent}%
                          </div>
                        </div>
                        <div className="stat-box">
                          <div className="k">PLA WEIGHT</div>
                          <div className="v">{tel.print_estimates.pla_weight_g} g</div>
                        </div>
                        <div className="stat-box">
                          <div className="k">EST. FILAMENT</div>
                          <div className="v">${tel.print_estimates.pla_cost_usd}</div>
                        </div>
                      </div>
                    </div>
                  )}

                  <div className="field-group">
                    <div className="field-label">zero-paywall export</div>
                    <div className="export-grid">
                      {[
                        { fmt: 'glb', label: '.GLB', sub: 'gltf pbr' },
                        { fmt: '3mf', label: '.3MF', sub: 'bambu/orca' },
                        { fmt: 'stl', label: '.STL', sub: 'print mm' },
                        { fmt: 'obj', label: '.OBJ', sub: 'dcc mesh' },
                        { fmt: 'ply', label: '.PLY', sub: 'vtx color' },
                        { fmt: 'cs', label: '.CS', sub: 'unity c#' },
                      ].map((ex) => (
                        <button
                          key={ex.fmt}
                          className="oc-btn"
                          style={{ flexDirection: 'column', padding: '10px 4px', gap: 2 }}
                          onClick={() => triggerDownload(ex.fmt)}
                        >
                          <strong>{ex.label}</strong>
                          <span style={{ fontSize: 9.5, color: 'var(--ink-muted)' }}>{ex.sub}</span>
                        </button>
                      ))}
                    </div>
                  </div>
                </>
              )}
            </div>
          </aside>
        )}
      </div>

      {/* Global Ctrl+P Command Palette */}
      <CommandPalette open={paletteOpen} onClose={() => setPaletteOpen(false)} commands={paletteCommands} />

      {/* Hardware & Universal Local LLM Router Config Modal */}
      <HardwareModal
        open={hwModalOpen}
        onClose={() => setHwModalOpen(false)}
        hardware={hardware}
        routerConfig={routerCfg}
        onRescan={async () => {
          const r = await fetch('/api/hardware/rescan', { method: 'POST' });
          if (r.ok) setHardware(await r.json());
        }}
        onSaveRouter={async (newCfg) => {
          const r = await fetch('/api/settings/router', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(newCfg),
          });
          if (r.ok) {
            const d = await r.json();
            setRouterCfg(d.config);
            const hwR = await fetch('/api/hardware/rescan', { method: 'POST' });
            if (hwR.ok) setHardware(await hwR.json());
          }
        }}
      />
    </div>
  );
}
