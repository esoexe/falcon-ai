import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import Viewport3D from './components/Viewport3D.jsx';
import GameViewport from './components/GameViewport.jsx';
import SwarmPanel from './components/SwarmPanel.jsx';
import SketchPad from './components/SketchPad.jsx';
import HardwareModal from './components/HardwareModal.jsx';
import CommandPalette from './components/CommandPalette.jsx';

const STARTER_PROMPTS = [
  { label: 'GT Sports Car', mode: 'build', text: 'Create a 3D realistic model of a car' },
  { label: 'Sculpted Bust', mode: 'build', text: 'Sculpted marble warrior portrait bust with armored collar' },
  { label: 'Space Fighter', mode: 'build', text: 'Armored twin-engine sci-fi starfighter with cockpit' },
  { label: 'Space Dogfight Game', mode: 'game', text: 'Playable 3D space dogfight game in an asteroid belt' },
];

const MATERIAL_PRESETS = [
  { id: 'brushed_titanium', label: 'Brushed Titanium', color: '#94a3b8', metallic: 0.9, roughness: 0.25, finish: 'brushed_titanium' },
  { id: 'carbon_fiber', label: 'Forged Carbon', color: '#1f2937', metallic: 0.35, roughness: 0.32, finish: 'carbon_fiber' },
  { id: 'crimson_clearcoat', label: 'Crimson Armor', color: '#dc2626', metallic: 0.72, roughness: 0.18, finish: 'painted_armor' },
  { id: 'royal_gold', label: 'Anodized Gold', color: '#eab308', metallic: 0.94, roughness: 0.2, finish: 'anodized_gold' },
  { id: 'damascus_steel', label: 'Damascus Steel', color: '#cbd5e1', metallic: 0.92, roughness: 0.2, finish: 'damascus_steel' },
  { id: 'stylized_handpainted', label: 'Stylized Hand-Paint', color: '#60a5fa', metallic: 0.16, roughness: 0.48, finish: 'stylized_handpainted' },
  { id: 'anime_cel', label: 'Anime Cel-Shade', color: '#38bdf8', metallic: 0.05, roughness: 0.65, finish: 'anime_cel' },
  { id: 'organic_skin', label: 'Organic Skin', color: '#d6b49c', metallic: 0.04, roughness: 0.44, finish: 'organic_skin' },
  { id: 'woven_fabric', label: 'Woven Fabric', color: '#7c2d12', metallic: 0.06, roughness: 0.72, finish: 'woven_fabric' },
  { id: 'marble_veined', label: 'Veined Marble', color: '#f1f5f9', metallic: 0.08, roughness: 0.24, finish: 'marble_veined' },
];

const SLASH_COMMANDS = [
  { cmd: '/providers', desc: 'Open Settings & Providers (API keys, Ollama, custom URLs)' },
  { cmd: '/drafts', desc: 'Generate 4-candidate draft preview grid for current prompt' },
  { cmd: '/remesh', desc: 'Open Smart Quad Remesh & exact polycount panel' },
  { cmd: '/plugins', desc: 'Open Unity / Unreal / Godot / Roblox / Blender Live-Link plugins' },
  { cmd: '/new', desc: 'Start a fresh 3D model session' },
  { cmd: '/game', desc: 'Switch composer to playable 3D Game mode' },
  { cmd: '/inspector', desc: 'Toggle right-hand Studio Inspector' },
  { cmd: '/export fbx', desc: 'Download Autodesk .FBX 7.4 (Quads + Skeleton + Anim)' },
  { cmd: '/export usdz', desc: 'Download Apple Vision Pro / iOS AR .USDZ' },
  { cmd: '/export glb', desc: 'Download textured PBR .GLB binary' },
  { cmd: '/export 3mf', desc: 'Download slicer-ready .3MF package' },
  { cmd: '/theme', desc: 'Switch between OpenCode Dark and Light themes' },
];

const normalizeList = (data, key) => {
  if (Array.isArray(data)) return data;
  if (data && Array.isArray(data[key])) return data[key];
  return [];
};

export default function App() {
  const isElectron = typeof window !== 'undefined' && Boolean(window.falconDesktop?.isElectron);

  // Clean OpenCode layout state (Inspector & History closed by default = zero clutter!)
  const [theme, setTheme] = useState(() => localStorage.getItem('falcon-theme') || 'dark');
  const [leftOpen, setLeftOpen] = useState(true);
  const [inspectorOpen, setInspectorOpen] = useState(false);
  const [inspectorTab, setInspectorTab] = useState('parts');
  const [exportMenuOpen, setExportMenuOpen] = useState(false);
  const [historyOpen, setHistoryOpen] = useState(false);

  // Settings & Providers Modal + Command Palette
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [settingsSection, setSettingsSection] = useState('connect');
  const [paletteOpen, setPaletteOpen] = useState(false);

  // Hardware & AI Provider state
  const [hardware, setHardware] = useState(null);
  const [routerCfg, setRouterCfg] = useState(null);
  const [providerCounts, setProviderCounts] = useState({ active_in_swarm: 16 });

  // Workspace Sessions (Models & Games)
  const [studioMode, setStudioMode] = useState('model'); // 'model' | 'game'
  const [assets, setAssets] = useState([]);
  const [activeAsset, setActiveAsset] = useState(null);
  const [games, setGames] = useState([]);
  const [activeGame, setActiveGame] = useState(null);

  // Viewport & Generation Controls
  const [shadingMode, setShadingMode] = useState('pbr');
  const [animMode, setAnimMode] = useState('none');
  const [autoRotate, setAutoRotate] = useState(false);
  const [cameraPreset, setCameraPreset] = useState('iso');
  const [cameraNonce, setCameraNonce] = useState(0);
  // Parent renders (for example, while typing) must not reissue a camera command.
  const cameraCommand = useMemo(() => ({ type: cameraPreset, nonce: cameraNonce }), [cameraPreset, cameraNonce]);
  const [showSkeleton, setShowSkeleton] = useState(false);
  const [selectedPartIdx, setSelectedPartIdx] = useState(-1);

  const [qualityMode, setQualityMode] = useState('ultra');
  const [gridRes, setGridRes] = useState(96);

  // OpenCode Prompt Composer
  const [agentMode, setAgentMode] = useState('build'); // 'build' | 'edit' | 'game' | 'plan'
  const [composerText, setComposerText] = useState('');
  const [slashIdx, setSlashIdx] = useState(0);
  const [loading, setLoading] = useState(false);
  const [loadingMsg, setLoadingMsg] = useState('');
  const [pendingPlan, setPendingPlan] = useState(null);

  const [activityLog, setActivityLog] = useState([
    { role: 'system', text: 'Falcon AI ready · Pure C++20 engine + multi-provider router online.' },
  ]);

  // Inspector tool states (v3.0 Full Meshy Parity)
  const [imgMode, setImgMode] = useState('volumetric');
  const [imgDepth, setImgDepth] = useState(0.68);
  const [mvFiles, setMvFiles] = useState({ front: null, side: null, back: null, top: null });
  const [cadMmX, setCadMmX] = useState(120);
  const [cadMmY, setCadMmY] = useState(60);
  const [cadMmZ, setCadMmZ] = useState(80);
  const [rigTemplate, setRigTemplate] = useState('auto');
  const [motionLibrary, setMotionLibrary] = useState([]);
  const [selectedClipId, setSelectedClipId] = useState('walk_standard');
  const [bpJsonText, setBpJsonText] = useState('');

  // v3.0 Smart Quad Remesh, 4-Draft Grid, HD 4K PBR & 3D Region Brush states
  const [draftCandidates, setDraftCandidates] = useState([]);
  const [remeshTargetFaces, setRemeshTargetFaces] = useState(12000);
  const [remeshQuadMode, setRemeshQuadMode] = useState(true);
  const [texResolution, setTexResolution] = useState(1024);
  const [aiTexPrompt, setAiTexPrompt] = useState('');
  const [regionBrushActive, setRegionBrushActive] = useState(false);
  const [regionBrushColor, setRegionBrushColor] = useState('#38bdf8');
  const [regionBrushRadius, setRegionBrushRadius] = useState(0.32);

  const composerRef = useRef(null);
  const quickImageInputRef = useRef(null);
  const importMeshInputRef = useRef(null);

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('falcon-theme', theme);
  }, [theme]);

  const toggleTheme = useCallback(() => {
    setTheme((t) => (t === 'dark' ? 'cream' : 'dark'));
  }, []);

  const openProvidersModal = useCallback((sec = 'connect') => {
    setSettingsSection(sec);
    setSettingsOpen(true);
  }, []);

  useEffect(() => {
    try {
      const q = new URLSearchParams(window.location.search).get('launch');
      if (q === 'game') {
        setStudioDomain('game');
        setAgentMode('game');
      } else if (q === 'providers') {
        setSettingsSection('connect');
        setSettingsOpen(true);
      }
    } catch {}
  }, []);

  const pushLog = useCallback((role, text) => {
    setActivityLog((prev) => [...prev.slice(-24), { role, text }]);
  }, []);

  const syncActiveAsset = useCallback((item) => {
    if (!item) return;
    setActiveAsset(item);
    setSelectedPartIdx(-1);
    if (item.blueprint) setBpJsonText(JSON.stringify(item.blueprint, null, 2));
    const dm = item.telemetry?.dimensions_mm || item.stats?.dimensions_mm;
    if (dm) {
      setCadMmX(Math.round(dm.x || 100));
      setCadMmY(Math.round(dm.y || 100));
      setCadMmZ(Math.round(dm.z || 100));
    }
  }, []);

  const loadAssetDetail = useCallback(
    async (id) => {
      try {
        const r = await fetch(`/api/assets/${id}`);
        if (!r.ok) return;
        const data = await r.json();
        syncActiveAsset(data);
      } catch {
        /* ignore */
      }
    },
    [syncActiveAsset]
  );

  const refreshProviderCounts = useCallback(async () => {
    try {
      const r = await fetch('/api/providers');
      if (r.ok) {
        const d = await r.json();
        if (d.counts) setProviderCounts(d.counts);
      }
    } catch {
      /* ignore */
    }
  }, []);

  const refreshLists = useCallback(async () => {
    try {
      const [asRes, gmRes] = await Promise.all([fetch('/api/assets'), fetch('/api/games')]);
      const nextAssets = asRes.ok ? normalizeList(await asRes.json(), 'assets') : [];
      const nextGames = gmRes.ok ? normalizeList(await gmRes.json(), 'games') : [];
      setAssets(nextAssets);
      setGames(nextGames);
      return { nextAssets, nextGames };
    } catch {
      return { nextAssets: [], nextGames: [] };
    }
  }, []);

  const bootstrap = useCallback(async () => {
    try {
      const [hwRes, rtRes, asRes, gmRes, pvRes, animRes] = await Promise.all([
        fetch('/api/hardware'),
        fetch('/api/settings/router'),
        fetch('/api/assets'),
        fetch('/api/games'),
        fetch('/api/providers'),
        fetch('/api/animations'),
      ]);
      const [hw, rt, asRaw, gmRaw, pvData, animRaw] = await Promise.all([
        hwRes.json(),
        rtRes.json(),
        asRes.ok ? asRes.json() : [],
        gmRes.ok ? gmRes.json() : [],
        pvRes.ok ? pvRes.json() : { counts: { active_in_swarm: 16 } },
        animRes.ok ? animRes.json() : [],
      ]);
      if (Array.isArray(animRaw) && animRaw.length > 0) setMotionLibrary(animRaw);

      const asList = normalizeList(asRaw, 'assets');
      const gmList = normalizeList(gmRaw, 'games');

      setHardware(hw);
      setRouterCfg(rt);
      setAssets(asList);
      setGames(gmList);
      if (pvData?.counts) setProviderCounts(pvData.counts);
      if (hw?.recommended_settings?.voxel_grid_resolution) {
        setGridRes(Math.max(88, hw.recommended_settings.voxel_grid_resolution));
      }

      if (asList.length > 0) {
        syncActiveAsset(asList[0]);
      } else {
        setLoading(true);
        setLoadingMsg('Preparing starter 3D model...');
        const gen = await fetch('/api/generate/text', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            prompt: 'Create a 3D realistic model of a car',
            style: 'realistic',
            ultra_detail: true,
            resolution: 96,
          }),
        });
        const gData = await gen.json();
        syncActiveAsset(gData);
        await refreshLists();
        setLoading(false);
      }
      if (gmList.length > 0) {
        setActiveGame(gmList[0]);
      }
    } catch (err) {
      setLoading(false);
      pushLog('error', `Backend connection error: ${err.message}`);
    }
  }, [pushLog, refreshLists, syncActiveAsset]);

  useEffect(() => {
    bootstrap();
  }, [bootstrap]);

  // Core Actions (100% matched to C++20 v3.0 main.cpp routes)
  const generateDraftGrid = async (promptText) => {
    const q = (promptText || '').trim();
    if (!q) return;
    setStudioMode('model');
    setLoading(true);
    setLoadingMsg('Synthesizing 4 parallel draft candidates...');
    pushLog('user', `[4-Draft Preview] ${q}`);
    try {
      const r = await fetch('/api/generate/draft-grid', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt: q }),
      });
      const data = await r.json();
      if (data.error) throw new Error(data.error);
      const cands = data.candidates || [];
      setDraftCandidates(cands);
      if (cands.length > 0) syncActiveAsset(cands[0]);
      await refreshLists();
      pushLog('agent', `4-Draft Grid ready (${data.total_ms}ms) — pick any card or click Refine to Ultra.`);
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const refineCandidateToUltra = async (cand) => {
    if (!cand) return;
    setLoading(true);
    setLoadingMsg(`Refining "${cand.variant_label || cand.title}" to Ultra...`);
    try {
      const r = await fetch('/api/swarm/detail', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          asset_id: cand.id,
          instruction: 'refine ultra high detail',
          resolution: Math.max(gridRes, 108),
        }),
      });
      const d = await r.json();
      if (d.error) throw new Error(d.error);
      setDraftCandidates([]);
      syncActiveAsset(d);
      await refreshLists();
      pushLog('agent', `Refined "${d.title}" · ${(d.telemetry?.faces || 0).toLocaleString()} tris`);
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const generateFromText = async (promptText, overrideQuality) => {
    const q = (promptText || '').trim();
    if (!q) return;
    const qMode = overrideQuality || qualityMode;
    if (qMode === 'draft4') {
      await generateDraftGrid(q);
      return;
    }
    setStudioMode('model');
    setLoading(true);
    const neuralModes = ['hunyuan3d', 'triposg', 'sf3d', 'neural_auto', 'local_neural'];
    setLoadingMsg(
      neuralModes.includes(qMode)
        ? `Bridging to ${qMode.toUpperCase()} Neural 3D Diffusion (CGT-Subdiv failover active)...`
        : qMode === 'neural_organic'
          ? 'Evaluating C++20 SIREN implicit organic field...'
          : 'Synthesizing CGT-Standard Catmull-Clark quads & PBR materials...'
    );
    pushLog('user', q);
    try {
      const sculptModeMap = {
        ultra: 'auto',
        cgt_subdiv: 'cgt_subdiv',
        hunyuan3d: 'hunyuan3d',
        triposg: 'triposg',
        sf3d: 'sf3d',
        neural_organic: 'neural_organic',
        csg_swarm: 'csg_swarm',
        sculpted: 'auto',
        draft: 'auto',
      };
      const r = await fetch('/api/generate/text', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          prompt: q,
          style: 'realistic',
          sculpt_mode: sculptModeMap[qMode] || 'auto',
          ultra_detail: qMode !== 'draft',
          resolution: qMode === 'draft' ? 68 : gridRes,
        }),
      });
      const data = await r.json();
      if (data.error) throw new Error(data.error);
      syncActiveAsset(data);
      await refreshLists();
      const tris = data.telemetry?.faces || data.stats?.triangles || 0;
      const pCount = data.parts?.length || data.blueprint?.parts?.length || 0;
      pushLog(
        'agent',
        `Built "${data.title || data.name}" · ${tris.toLocaleString()} tris · ${pCount} parts · ${data.generation_ms}ms`
      );
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const generateGameFromPrompt = async (promptText, mutateExisting = false) => {
    const q = (promptText || '').trim();
    if (!q) return;
    setStudioMode('game');
    setLoading(true);
    setLoadingMsg(mutateExisting && activeGame ? 'Updating 3D game world...' : 'Building playable 3D game...');
    pushLog('user', `[Game] ${q}`);
    try {
      const url = mutateExisting && activeGame ? `/api/games/${activeGame.id}/mutate` : '/api/games/create';
      const bodyPayload = mutateExisting && activeGame ? { instruction: q } : { prompt: q };
      const r = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(bodyPayload),
      });
      const raw = await r.json();
      if (raw.error) throw new Error(raw.error);
      const gameObj = raw.game || raw;
      setActiveGame(gameObj);
      setBpJsonText(JSON.stringify(gameObj, null, 2));
      await refreshLists();
      pushLog(
        'agent',
        `Ready: "${gameObj.title}" (${gameObj.genre}) · WASD + Space playable`
      );
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const handleImageUpload = async (file) => {
    if (!file) return;
    setStudioMode('model');
    setLoading(true);
    setLoadingMsg(`Reconstructing 3D mesh from ${file.name}...`);
    pushLog('user', `[Image-to-3D] ${file.name}`);
    try {
      const fd = new FormData();
      fd.append('file', file, file.name);
      fd.append('mode', imgMode);
      fd.append('depth_strength', String(imgDepth));
      fd.append('resolution', '104');

      const r = await fetch('/api/generate/image', {
        method: 'POST',
        body: fd,
      });
      const data = await r.json();
      if (data.error) throw new Error(data.error);
      syncActiveAsset(data);
      await refreshLists();
      const tris = data.telemetry?.faces || 0;
      pushLog('agent', `Reconstructed "${data.title}" · ${tris.toLocaleString()} tris`);
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const handleSketchDataUrl = async (dataUrl) => {
    try {
      const res = await fetch(dataUrl);
      const blob = await res.blob();
      const file = new File([blob], 'Sketch.png', { type: 'image/png' });
      await handleImageUpload(file);
    } catch (e) {
      pushLog('error', e.message);
    }
  };

  const downloadExport = (ext) => {
    setExportMenuOpen(false);
    if (studioMode === 'game' && activeGame && (ext === 'html' || ext === 'game_cs')) {
      const fmt = ext === 'game_cs' ? 'cs' : 'html';
      window.open(`/api/games/${activeGame.id}/export/${fmt}`, '_blank');
      pushLog('system', `Exported game "${activeGame.title}" (.${fmt})`);
      return;
    }
    if (!activeAsset) return;
    window.open(`/api/export/${activeAsset.id}/${ext}`, '_blank');
    pushLog('system', `Exported ${activeAsset.title || 'model'}.${ext}`);
  };

  const handleMeshImport = async (file) => {
    if (!file) return;
    setStudioMode('model');
    setLoading(true);
    setLoadingMsg(`Importing 3D mesh ${file.name}...`);
    pushLog('user', `[Import Mesh] ${file.name}`);
    try {
      const fd = new FormData();
      fd.append('file', file, file.name);
      const r = await fetch('/api/assets/import', { method: 'POST', body: fd });
      const data = await r.json();
      if (data.error) throw new Error(data.error);
      syncActiveAsset(data);
      await refreshLists();
      pushLog('agent', `Imported "${data.title}" · ${(data.telemetry?.faces || 0).toLocaleString()} tris — ready for 4K PBR, Quad Remesh & Auto-Rig!`);
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const handleMultiViewGenerate = async () => {
    if (!mvFiles.front && !mvFiles.side) {
      pushLog('error', 'Select at least a Front view image for Multi-View 3D.');
      return;
    }
    setStudioMode('model');
    setLoading(true);
    setLoadingMsg('Carving multi-view visual hull & projecting photo RGB...');
    try {
      const fd = new FormData();
      if (mvFiles.front) fd.append('front', mvFiles.front, mvFiles.front.name);
      if (mvFiles.side) fd.append('side', mvFiles.side, mvFiles.side.name);
      if (mvFiles.back) fd.append('back', mvFiles.back, mvFiles.back.name);
      if (mvFiles.top) fd.append('top', mvFiles.top, mvFiles.top.name);
      fd.append('resolution', '96');
      fd.append('depth_inflation', String(imgDepth));
      const r = await fetch('/api/generate/multiview', { method: 'POST', body: fd });
      const data = await r.json();
      if (data.error) throw new Error(data.error);
      syncActiveAsset(data);
      await refreshLists();
      pushLog('agent', `Multi-View 3D built · ${(data.telemetry?.faces || 0).toLocaleString()} photo-projected tris`);
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const runSmartRemesh = async () => {
    if (!activeAsset) return;
    setLoading(true);
    setLoadingMsg(`Running Garland-Heckbert QEM (${remeshQuadMode ? 'Quad-Dominant' : 'Triangle'} · ${remeshTargetFaces.toLocaleString()} target)...`);
    try {
      const r = await fetch(`/api/mesh/${activeAsset.id}/smart-remesh`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          target_faces: Number(remeshTargetFaces),
          quad_mode: remeshQuadMode,
          preserve_creases: true,
          tex_resolution: Number(texResolution),
        }),
      });
      const data = await r.json();
      if (data.error) throw new Error(data.error);
      syncActiveAsset(data);
      await refreshLists();
      const rep = data.remesh_report || {};
      pushLog(
        'agent',
        remeshQuadMode
          ? `Quad Remesh complete · ${(rep.quad_count || 0).toLocaleString()} quads (${(rep.final_polygons || 0).toLocaleString()} total faces)`
          : `QEM Remesh complete · ${(rep.final_triangles || 0).toLocaleString()} triangles`
      );
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const runAiTexture = async () => {
    if (!activeAsset || !aiTexPrompt.trim()) return;
    setLoading(true);
    setLoadingMsg(`Baking ${texResolution}px PBR maps ("${aiTexPrompt.trim()}")...`);
    try {
      const r = await fetch(`/api/mesh/${activeAsset.id}/ai-texture`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt: aiTexPrompt.trim(), tex_resolution: Number(texResolution) }),
      });
      const data = await r.json();
      if (data.error) throw new Error(data.error);
      syncActiveAsset(data);
      await refreshLists();
      pushLog('agent', `Baked ${texResolution}×${texResolution} HD PBR texture for "${aiTexPrompt.trim()}"`);
    } catch (e) {
      pushLog('error', e.message);
    } finally {
      setLoading(false);
    }
  };

  const handlePaintRegionPoint = async (xyz) => {
    if (!activeAsset || !xyz) return;
    try {
      const r = await fetch(`/api/mesh/${activeAsset.id}/paint-region`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          center: xyz,
          radius: Number(regionBrushRadius),
          color: regionBrushColor,
          feather: 0.42,
        }),
      });
      const data = await r.json();
      if (!data.error) {
        syncActiveAsset(data);
        pushLog('agent', `3D Brush healed ${data.painted_vertices || 0} vertices at [${xyz.map((n) => n.toFixed(2)).join(', ')}]`);
      }
    } catch (e) {
      pushLog('error', e.message);
    }
  };

  const launchDesktopSlicer = async (slicerId) => {
    if (!activeAsset) return;
    setExportMenuOpen(false);
    try {
      const r = await fetch(`/api/slicer-links/${activeAsset.id}`);
      const d = await r.json();
      const hit = (d.slicers || []).find((s) => s.id === slicerId);
      if (hit?.uri) {
        window.location.href = hit.uri;
        pushLog('system', `Sent ${activeAsset.title}.3mf to ${hit.name} (${hit.uri.split('://')[0]}://)`);
      }
    } catch (e) {
      pushLog('error', e.message);
    }
  };

  const downloadPluginZip = (engineSlug) => {
    setExportMenuOpen(false);
    window.open(`/api/plugins/${engineSlug}`, '_blank');
    pushLog('system', `Downloaded official Falcon AI v4.0 ${engineSlug.toUpperCase()} Live-Link plugin (.zip)`);
  };

  const applyMaterial = async (preset) => {
    if (!activeAsset) return;
    setLoading(true);
    setLoadingMsg(`Baking ${texResolution}px ${preset.label}...`);
    try {
      const partId =
        selectedPartIdx >= 0 ? (activeAsset.parts || [])[selectedPartIdx]?.id || '' : '';
      const r = await fetch(`/api/mesh/${activeAsset.id}/material`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          color: preset.color,
          metallic: preset.metallic,
          roughness: preset.roughness,
          finish: preset.finish,
          tex_resolution: Number(texResolution),
          part_id: partId,
        }),
      });
      const data = await r.json();
      syncActiveAsset(data);
      await refreshLists();
      pushLog('agent', `Applied ${texResolution}px PBR "${preset.label}"`);
    } finally {
      setLoading(false);
    }
  };

  const repairManifold = async () => {
    if (!activeAsset) return;
    setLoading(true);
    setLoadingMsg('Repairing watertight manifold & flat bed...');
    try {
      const r = await fetch(`/api/mesh/${activeAsset.id}/print-repair`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ flat_bottom_cut: true }),
      });
      const data = await r.json();
      syncActiveAsset(data);
      await refreshLists();
      pushLog('agent', `Mesh repaired · ${(data.telemetry?.faces || 0).toLocaleString()} tris`);
    } finally {
      setLoading(false);
    }
  };

  const splitWithPins = async () => {
    if (!activeAsset) return;
    setLoading(true);
    setLoadingMsg('Splitting mesh with 0.25mm alignment pins...');
    try {
      const r = await fetch(`/api/mesh/${activeAsset.id}/auto-split`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ axis: 'y', explode_gap: 0.36 }),
      });
      const data = await r.json();
      syncActiveAsset(data);
      await refreshLists();
      pushLog('agent', `Split into ${data.parts?.length || 3} parts with 0.25mm pins`);
    } finally {
      setLoading(false);
    }
  };

  const applyCadScale = async () => {
    if (!activeAsset) return;
    setLoading(true);
    setLoadingMsg(`Resizing to ${cadMmX}×${cadMmY}×${cadMmZ} mm...`);
    try {
      const r = await fetch(`/api/mesh/${activeAsset.id}/resize`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          x_mm: Number(cadMmX),
          y_mm: Number(cadMmY),
          z_mm: Number(cadMmZ),
        }),
      });
      const data = await r.json();
      syncActiveAsset(data);
      pushLog('agent', `Scaled to ${cadMmX}×${cadMmY}×${cadMmZ} mm`);
    } finally {
      setLoading(false);
    }
  };

  const runAutoRig = async (overrideClip) => {
    if (!activeAsset) return;
    const clipToUse = overrideClip || selectedClipId || 'walk_standard';
    setLoading(true);
    setLoadingMsg('Binding 4-bone volumetric skin & 24 motion clips...');
    try {
      const r = await fetch(`/api/mesh/${activeAsset.id}/rig`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ rig_type: rigTemplate, clip_id: clipToUse, bake_skin: true }),
      });
      const data = await r.json();
      syncActiveAsset(data);
      setShowSkeleton(true);
      setAnimMode(clipToUse);
      pushLog('agent', `Skinned ${data.rig?.bone_count || data.rig?.joints?.length || 17} bones + embedded 24 glTF/FBX motion clips`);
    } finally {
      setLoading(false);
    }
  };

  const recompileBlueprint = async () => {
    if (!activeAsset) return;
    setLoading(true);
    setLoadingMsg('Recompiling 3D blueprint...');
    try {
      const parsed = JSON.parse(bpJsonText);
      const r = await fetch('/api/generate/blueprint', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ blueprint: parsed, resolution: gridRes, asset_id: activeAsset.id }),
      });
      const data = await r.json();
      syncActiveAsset(data);
      await refreshLists();
      pushLog('agent', `Recompiled blueprint · ${(data.telemetry?.faces || 0).toLocaleString()} tris`);
    } catch (e) {
      pushLog('error', `Invalid JSON: ${e.message}`);
    } finally {
      setLoading(false);
    }
  };

  const deleteAsset = async (id, e) => {
    e.stopPropagation();
    await fetch(`/api/assets/${id}`, { method: 'DELETE' });
    const next = assets.filter((a) => a.id !== id);
    setAssets(next);
    if (activeAsset?.id === id) {
      if (next.length > 0) syncActiveAsset(next[0]);
      else setActiveAsset(null);
    }
  };

  const deleteGame = async (id, e) => {
    e.stopPropagation();
    await fetch(`/api/games/${id}`, { method: 'DELETE' });
    const next = games.filter((g) => g.id !== id);
    setGames(next);
    if (activeGame?.id === id) {
      if (next.length > 0) setActiveGame(next[0]);
      else setActiveGame(null);
    }
  };

  const saveRouterSettings = async (cfg) => {
    const r = await fetch('/api/settings/router', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(cfg),
    });
    const saved = await r.json();
    setRouterCfg(saved);
    refreshProviderCounts();
  };

  const rescanHardware = async () => {
    const r = await fetch('/api/hardware/rescan', { method: 'POST' });
    if (r.ok) {
      setHardware(await r.json());
      pushLog('system', 'Scanned local hardware & local AI ports');
    }
  };

  // Composer Submission & Slash Commands
  const matchingSlash = useMemo(() => {
    if (!composerText.startsWith('/')) return [];
    const q = composerText.trim().toLowerCase();
    return SLASH_COMMANDS.filter((s) => s.cmd.startsWith(q));
  }, [composerText]);

  const cycleMode = useCallback(() => {
    setAgentMode((m) => (m === 'build' ? 'edit' : m === 'edit' ? 'game' : m === 'game' ? 'plan' : 'build'));
  }, []);

  const executeSlash = async (raw) => {
    const cmd = raw.trim().toLowerCase();
    setComposerText('');
    if (cmd.startsWith('/providers') || cmd.startsWith('/settings')) {
      openProvidersModal('connect');
    } else if (cmd.startsWith('/drafts')) {
      const rest = raw.trim().slice(7).trim() || activeAsset?.prompt || 'Armored sci-fi cyber knight';
      await generateDraftGrid(rest);
    } else if (cmd === '/remesh') {
      setInspectorOpen(true);
      setInspectorTab('parts');
    } else if (cmd === '/plugins') {
      setInspectorOpen(true);
      setInspectorTab('plugins');
    } else if (cmd === '/new') {
      setStudioMode('model');
      setAgentMode('build');
      composerRef.current?.focus();
    } else if (cmd === '/game') {
      setStudioMode('game');
      setAgentMode('game');
    } else if (cmd === '/build') {
      setStudioMode('model');
      setAgentMode('build');
    } else if (cmd === '/edit') {
      setAgentMode('edit');
    } else if (cmd === '/inspector') {
      setInspectorOpen((v) => !v);
    } else if (cmd.startsWith('/export')) {
      const ext = cmd.split(/\s+/)[1] || 'glb';
      downloadExport(ext);
    } else if (cmd === '/theme') {
      toggleTheme();
    }
  };

  const submitPrompt = async () => {
    const text = composerText.trim();
    if (!text || loading) return;

    if (text.startsWith('/')) {
      await executeSlash(text);
      return;
    }

    setComposerText('');

    const lower = text.toLowerCase();
    const looksLikeGame =
      agentMode === 'game' ||
      (studioMode === 'game' && agentMode === 'edit') ||
      /\b(playable|fps|arcade|dogfight|platformer|minigame)\b/.test(lower) ||
      /\bgame\b/.test(lower);

    if (agentMode === 'plan') {
      setHistoryOpen(true);
      setPendingPlan({
        prompt: text,
        isGame: looksLikeGame,
        summary: looksLikeGame
          ? `6-agent game plan: design world, build 3D hero & enemy meshes, configure physics & HUD for "${text}".`
          : `6-agent 3D plan: sculpt multi-part SDF/CSG mesh, bake 256×256 PBR maps, and verify watertight topology for "${text}".`,
      });
      pushLog('plan', `Plan ready for "${text}" — click Execute to build.`);
      return;
    }

    if (looksLikeGame) {
      await generateGameFromPrompt(text, studioMode === 'game' && agentMode === 'edit' && Boolean(activeGame));
      return;
    }

    if (agentMode === 'edit' && activeAsset) {
      setLoading(true);
      setLoadingMsg('Applying edit to current 3D model...');
      pushLog('user', `[Edit] ${text}`);
      try {
        const r = await fetch('/api/agent/chat', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            message: text,
            current_asset_id: activeAsset.id,
          }),
        });
        const data = await r.json();
        if (data.asset) {
          syncActiveAsset(data.asset);
          await refreshLists();
        }
        pushLog('agent', data.reply || 'Updated 3D model');
      } catch (e) {
        pushLog('error', e.message);
      } finally {
        setLoading(false);
      }
      return;
    }

    await generateFromText(text);
  };

  // Global OpenCode Keyboard Shortcuts
  useEffect(() => {
    const onKey = (e) => {
      const inInput = ['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement?.tagName);
      if ((e.ctrlKey || e.metaKey) && (e.key.toLowerCase() === 'k' || e.key.toLowerCase() === 'p')) {
        e.preventDefault();
        setPaletteOpen((v) => !v);
        return;
      }
      if ((e.ctrlKey || e.metaKey) && e.key === ',') {
        e.preventDefault();
        openProvidersModal('connect');
        return;
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'b') {
        e.preventDefault();
        setLeftOpen((v) => !v);
        return;
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'i') {
        e.preventDefault();
        setInspectorOpen((v) => !v);
        return;
      }
      if (!inInput && e.key === '/') {
        e.preventDefault();
        composerRef.current?.focus();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [openProvidersModal]);

  const paletteCommands = useMemo(
    () => [
      {
        id: 'add-provider',
        category: 'Providers',
        label: 'Add AI Provider / API Key / Custom Endpoint...',
        shortcut: 'Ctrl+,',
        action: () => openProvidersModal('connect'),
      },
      {
        id: 'free-providers',
        category: 'Providers',
        label: 'Browse 68 Free AI Providers Catalog',
        action: () => openProvidersModal('free'),
      },
      {
        id: 'local-ollama',
        category: 'Providers',
        label: 'Configure Local Ollama / LM Studio / llama.cpp',
        action: () => openProvidersModal('local'),
      },
      {
        id: 'hw-settings',
        category: 'Settings',
        label: 'Hardware & Voxel Sculpting Preferences',
        action: () => openProvidersModal('hardware'),
      },
      {
        id: 'new-model',
        category: 'Session',
        label: 'New 3D Model Prompt',
        action: () => {
          setStudioMode('model');
          setAgentMode('build');
          composerRef.current?.focus();
        },
      },
      {
        id: 'new-game',
        category: 'Session',
        label: 'New Playable 3D Game Prompt',
        action: () => {
          setStudioMode('game');
          setAgentMode('game');
          composerRef.current?.focus();
        },
      },
      {
        id: 'upload-img',
        category: '3D Tool',
        label: 'Upload Image to 3D Mesh...',
        action: () => quickImageInputRef.current?.click(),
      },
      {
        id: 'toggle-insp',
        category: 'View',
        label: 'Toggle Studio Inspector Drawer',
        shortcut: 'Ctrl+I',
        action: () => setInspectorOpen((v) => !v),
      },
      {
        id: 'import-mesh',
        category: '3D Tool',
        label: 'Import External 3D Mesh (.GLB / .OBJ / .STL / .PLY)...',
        action: () => importMeshInputRef.current?.click(),
      },
      {
        id: 'quad-remesh',
        category: '3D Tool',
        label: 'Smart Quad Remesh & Polycount Target...',
        action: () => {
          setInspectorOpen(true);
          setInspectorTab('parts');
        },
      },
      {
        id: 'export-fbx',
        category: 'Export',
        label: 'Export Autodesk .FBX 7.4 (Quads + Rig + Anim)',
        action: () => downloadExport('fbx'),
      },
      {
        id: 'export-usdz',
        category: 'Export',
        label: 'Export Apple Vision Pro / ARKit .USDZ',
        action: () => downloadExport('usdz'),
      },
      {
        id: 'export-glb',
        category: 'Export',
        label: 'Export Textured .GLB',
        action: () => downloadExport('glb'),
      },
      {
        id: 'export-3mf',
        category: 'Export',
        label: 'Export Slicer .3MF',
        action: () => downloadExport('3mf'),
      },
      {
        id: 'export-stl',
        category: 'Export',
        label: 'Export Watertight .STL',
        action: () => downloadExport('stl'),
      },
      {
        id: 'theme',
        category: 'View',
        label: `Switch to ${theme === 'dark' ? 'Light' : 'Dark'} Theme`,
        action: toggleTheme,
      },
    ],
    [openProvidersModal, theme, toggleTheme, activeAsset, activeGame, studioMode]
  );

  const activeRouterLabel = useMemo(() => {
    const p = routerCfg?.provider || 'auto';
    if (p === 'ollama') return `Ollama · ${routerCfg?.ollama_model || 'llama3.2'}`;
    if (p === 'openai_compat') return `API · ${routerCfg?.openai_model || 'custom'}`;
    if (p === 'builtin_symbolic') return 'Offline C++20';
    return `Auto Router · ${providerCounts.active_in_swarm ?? 16} ready`;
  }, [routerCfg, providerCounts]);

  const telem = activeAsset?.telemetry || {};
  const trisCount = telem.faces || activeAsset?.stats?.triangles || 0;
  const dim = telem.dimensions_mm || activeAsset?.stats?.dimensions_mm || {};
  const parts = activeAsset?.parts || activeAsset?.blueprint?.parts || [];
  const lastLog = activityLog[activityLog.length - 1];

  return (
    <div
      className="oc-app"
      onDragOver={(e) => e.preventDefault()}
      onDrop={(e) => {
        e.preventDefault();
        const file = e.dataTransfer?.files?.[0];
        if (!file) return;
        const low = file.name.toLowerCase();
        if (low.endsWith('.glb') || low.endsWith('.obj') || low.endsWith('.stl') || low.endsWith('.ply')) {
          handleMeshImport(file);
        } else if (file.type.startsWith('image/')) {
          handleImageUpload(file);
        }
      }}
    >
      <input
        ref={quickImageInputRef}
        type="file"
        accept="image/png,image/jpeg,image/bmp"
        style={{ display: 'none' }}
        onChange={(e) => {
          if (e.target.files?.[0]) handleImageUpload(e.target.files[0]);
          e.target.value = '';
        }}
      />
      <input
        ref={importMeshInputRef}
        type="file"
        accept=".glb,.obj,.stl,.ply"
        style={{ display: 'none' }}
        onChange={(e) => {
          if (e.target.files?.[0]) handleMeshImport(e.target.files[0]);
          e.target.value = '';
        }}
      />

      {/* ====================================================================
          1. CLEAN OPENCODE LEFT SIDEBAR (Sessions + Bottom Provider Settings)
          ==================================================================== */}
      {leftOpen && (
        <aside className="oc-sidebar">
          <div className="oc-sidebar-top">
            <div className="oc-logo">
              <span className="oc-logo-mark">F</span>
              <span>Falcon</span>
              <span className="oc-kbd" style={{ fontSize: 10 }}>v4.0</span>
            </div>
            <button
              className="oc-icon-btn oc-no-drag"
              title="Collapse sidebar (Ctrl+B)"
              onClick={() => setLeftOpen(false)}
            >
              ⇤
            </button>
          </div>

          <div className="oc-seg-wrap">
            <div className="oc-segmented">
              <button
                className={`oc-seg-btn ${studioMode === 'model' ? 'active' : ''}`}
                onClick={() => {
                  setStudioMode('model');
                  if (agentMode === 'game') setAgentMode('build');
                }}
              >
                3D Models ({assets.length})
              </button>
              <button
                className={`oc-seg-btn ${studioMode === 'game' ? 'active' : ''}`}
                onClick={() => {
                  setStudioMode('game');
                  setAgentMode('game');
                  if (!activeGame && games.length > 0) setActiveGame(games[0]);
                }}
              >
                3D Games ({games.length})
              </button>
            </div>
          </div>

          <div style={{ display: 'flex', gap: 6, margin: '0 10px 8px' }}>
            <button
              className="oc-new-btn"
              style={{ margin: 0, flex: 1 }}
              onClick={() => {
                setAgentMode(studioMode === 'game' ? 'game' : 'build');
                composerRef.current?.focus();
              }}
            >
              <span>+ New {studioMode === 'game' ? 'Game' : 'Model'}</span>
              <span className="oc-kbd">/</span>
            </button>
            {studioMode === 'model' && (
              <button
                className="oc-btn oc-btn-sm"
                title="Import external .GLB / .OBJ / .STL / .PLY mesh for 4K Retexturing, Quad Remesh & Auto-Rig"
                onClick={() => importMeshInputRef.current?.click()}
              >
                Import 3D
              </button>
            )}
          </div>

          <div className="oc-session-section-title">
            <span>{studioMode === 'game' ? 'Playable Worlds' : 'Recent Models'}</span>
          </div>

          <div className="oc-session-list">
            {studioMode === 'model' ? (
              assets.map((a) => {
                const aTris = a.telemetry?.faces || a.triangles || 0;
                const aParts = a.telemetry?.parts_count || a.parts?.length || 0;
                return (
                  <div
                    key={a.id}
                    className={`oc-session-item ${activeAsset?.id === a.id ? 'active' : ''}`}
                    onClick={() => {
                      setStudioMode('model');
                      syncActiveAsset(a);
                      loadAssetDetail(a.id);
                    }}
                  >
                    <div style={{ minWidth: 0, flex: 1 }}>
                      <div className="oc-session-title">{a.title || a.name}</div>
                      <div className="oc-session-meta">
                        {aTris.toLocaleString()} tris · {aParts} parts
                      </div>
                    </div>
                    <button className="oc-session-del" title="Delete" onClick={(e) => deleteAsset(a.id, e)}>
                      ×
                    </button>
                  </div>
                );
              })
            ) : games.length === 0 ? (
              <div style={{ padding: '14px 10px', fontSize: 12, color: 'var(--ink-muted)' }}>
                No games yet. Describe a game below or pick a starter prompt.
              </div>
            ) : (
              games.map((g) => (
                <div
                  key={g.id}
                  className={`oc-session-item ${activeGame?.id === g.id ? 'active' : ''}`}
                  onClick={() => {
                    setStudioMode('game');
                    setActiveGame(g);
                  }}
                >
                  <div style={{ minWidth: 0, flex: 1 }}>
                    <div className="oc-session-title">{g.title}</div>
                    <div className="oc-session-meta">
                      {g.genre} · {g.entities?.length || g.entity_count || 0} entities
                    </div>
                  </div>
                  <button className="oc-session-del" title="Delete" onClick={(e) => deleteGame(g.id, e)}>
                    ×
                  </button>
                </div>
              ))
            )}
          </div>

          {/* Pinned Bottom OpenCode Settings & Providers Trigger */}
          <div className="oc-sidebar-footer">
            <button
              className="oc-settings-trigger"
              onClick={() => openProvidersModal('connect')}
              title="Configure AI Providers, API Keys, Local Ollama & Hardware (Ctrl+,)"
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: 8, minWidth: 0 }}>
                <span className="oc-dot" />
                <div style={{ textAlign: 'left', minWidth: 0 }}>
                  <div style={{ fontSize: 12, fontWeight: 600 }}>Settings & Providers</div>
                  <div
                    style={{
                      fontSize: 10.5,
                      fontFamily: 'var(--font-mono)',
                      color: 'var(--ink-muted)',
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                    }}
                  >
                    {activeRouterLabel}
                  </div>
                </div>
              </div>
              <span className="oc-kbd">⚙</span>
            </button>
          </div>
        </aside>
      )}

      {/* ====================================================================
          2. MAIN CENTER WORKSPACE (Minimal Header + 3D Stage + OpenCode Composer)
          ==================================================================== */}
      <main className="oc-main">
        <header className="oc-topbar">
          <div className="oc-topbar-left oc-no-drag">
            {!leftOpen && (
              <button className="oc-icon-btn" title="Show sidebar (Ctrl+B)" onClick={() => setLeftOpen(true)}>
                ☰
              </button>
            )}
            <span className="oc-doc-title">
              {studioMode === 'game'
                ? activeGame?.title || 'Playable 3D Game Studio'
                : activeAsset?.title || activeAsset?.name || 'Untitled 3D Model'}
            </span>
            {studioMode === 'model' && activeAsset && (
              <span className="oc-doc-meta">
                {activeAsset.quad_mode && activeAsset.quad_count > 0
                  ? `${activeAsset.quad_count.toLocaleString()} quads · `
                  : `${trisCount.toLocaleString()} tris · `}
                {parts.length} parts
                {activeAsset.tex_resolution > 256 ? ` · ${activeAsset.tex_resolution >= 4096 ? '4K' : activeAsset.tex_resolution >= 2048 ? '2K' : '1K'} PBR` : ''}
                {telem.watertight ? ' · Watertight' : ''}
              </span>
            )}
            {studioMode === 'game' && activeGame && (
              <span className="oc-doc-meta">{activeGame.genre}</span>
            )}
          </div>

          <div className="oc-topbar-right oc-no-drag">
            {studioMode === 'model' && (
              <div className="oc-view-pills">
                {[
                  { id: 'pbr', label: 'PBR' },
                  { id: 'pbr_wire', label: 'Wire' },
                  { id: 'clay', label: 'Clay' },
                ].map((s) => (
                  <button
                    key={s.id}
                    className={`oc-view-pill ${shadingMode === s.id ? 'active' : ''}`}
                    onClick={() => setShadingMode(s.id)}
                  >
                    {s.label}
                  </button>
                ))}
              </div>
            )}

            <div className="oc-popover-wrap">
              <button className="oc-btn oc-btn-sm" onClick={() => setExportMenuOpen((v) => !v)}>
                Export ▾
              </button>
              {exportMenuOpen && (
                <div className="oc-popover" onMouseLeave={() => setExportMenuOpen(false)}>
                  {studioMode === 'game' && activeGame && (
                    <>
                      <button className="oc-popover-item" onClick={() => downloadExport('html')}>
                        <span>Standalone Game</span>
                        <span className="oc-kbd">.html</span>
                      </button>
                      <button className="oc-popover-item" onClick={() => downloadExport('game_cs')}>
                        <span>Unity Game Suite</span>
                        <span className="oc-kbd">.cs</span>
                      </button>
                    </>
                  )}
                  <button className="oc-popover-item" onClick={() => downloadExport('glb')}>
                    <span>Khronos Binary (PBR/Rig)</span>
                    <span className="oc-kbd">.glb</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => downloadExport('fbx')}>
                    <span>Autodesk Game/DCC</span>
                    <span className="oc-kbd">.fbx</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => downloadExport('usdz')}>
                    <span>Apple Vision / iOS AR</span>
                    <span className="oc-kbd">.usdz</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => downloadExport('3mf')}>
                    <span>Bambu / Orca Slicer</span>
                    <span className="oc-kbd">.3mf</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => downloadExport('stl')}>
                    <span>3D Print Mesh</span>
                    <span className="oc-kbd">.stl</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => downloadExport('obj')}>
                    <span>Quad / Tri Wavefront</span>
                    <span className="oc-kbd">.obj</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => downloadExport('cs')}>
                    <span>Unity C# Builder</span>
                    <span className="oc-kbd">.cs</span>
                  </button>
                  <div style={{ borderTop: '1px solid var(--border-subtle)', margin: '4px 0' }} />
                  <button className="oc-popover-item" onClick={() => launchDesktopSlicer('bambu')}>
                    <span>Send to Bambu Studio</span>
                    <span className="oc-kbd">↗</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => launchDesktopSlicer('orca')}>
                    <span>Send to OrcaSlicer</span>
                    <span className="oc-kbd">↗</span>
                  </button>
                  <button className="oc-popover-item" onClick={() => downloadPluginZip('unity')}>
                    <span>Unity / UE5 / Godot Plugins</span>
                    <span className="oc-kbd">.zip</span>
                  </button>
                </div>
              )}
            </div>

            <button className="oc-btn oc-btn-sm" onClick={() => openProvidersModal('connect')}>
              Providers
            </button>

            <button
              className={`oc-btn oc-btn-sm ${inspectorOpen ? 'oc-btn-primary' : ''}`}
              onClick={() => setInspectorOpen((v) => !v)}
              title="Toggle Studio Inspector (Ctrl+I)"
            >
              Inspector
            </button>

            <button
              className="oc-icon-btn"
              title="Command Palette (Ctrl+K)"
              onClick={() => setPaletteOpen(true)}
            >
              <span className="oc-kbd">⌘K</span>
            </button>

            {isElectron && (
              <div className="oc-win-controls">
                <button className="oc-win-btn" onClick={() => window.falconDesktop.minimize()}>
                  ─
                </button>
                <button className="oc-win-btn" onClick={() => window.falconDesktop.maximize()}>
                  □
                </button>
                <button className="oc-win-btn close" onClick={() => window.falconDesktop.close()}>
                  ✕
                </button>
              </div>
            )}
          </div>
        </header>

        <div className="oc-stage-canvas" onClick={() => exportMenuOpen && setExportMenuOpen(false)}>
          {studioMode === 'model' && (
            <div className="oc-viewport-hud">
              {['iso', 'front', 'side', 'top'].map((cam) => (
                <button
                  key={cam}
                  className={`oc-hud-btn ${cameraPreset === cam ? 'active' : ''}`}
                  onClick={() => {
                    setCameraPreset(cam);
                    setCameraNonce((n) => n + 1);
                  }}
                >
                  {cam.charAt(0).toUpperCase() + cam.slice(1)}
                </button>
              ))}
              <button
                className={`oc-hud-btn ${autoRotate ? 'active' : ''}`}
                onClick={() => setAutoRotate((v) => !v)}
              >
                Spin
              </button>
            </div>
          )}

          {loading && (
            <div className="loading-overlay">
              <div className="spinner" />
              <span>{loadingMsg}</span>
            </div>
          )}

          {studioMode === 'game' ? (
            <GameViewport
              game={activeGame}
              onQuickMutate={(p) => generateGameFromPrompt(p, Boolean(activeGame))}
              onExportGameHtml={() => downloadExport('html')}
              onExportGameCs={() => downloadExport('game_cs')}
            />
          ) : (
            <Viewport3D
              glbUrl={activeAsset?.glb_url || (activeAsset ? `/api/export/${activeAsset.id}/glb` : null)}
              shadingMode={shadingMode}
              autoRotate={autoRotate}
              regionBrushActive={regionBrushActive}
              onPaintRegionPoint={handlePaintRegionPoint}
              rigData={activeAsset?.rig || null}
              showRig={showSkeleton}
              activeAnimation={animMode === 'none' ? 'rest' : animMode}
              cameraCommand={cameraCommand}
              selectedPartId={selectedPartIdx >= 0 ? parts[selectedPartIdx]?.id || parts[selectedPartIdx]?.name : null}
              theme={theme}
            />
          )}

          {/* ================================================================
              SIGNATURE FLOATING OPENCODE COMPOSER AT BOTTOM OF STAGE
              ================================================================ */}
          <div className="oc-composer-region">
            {draftCandidates.length > 0 && (
              <div
                style={{
                  background: 'var(--bg-elevated)',
                  border: '1px solid var(--border-strong)',
                  borderRadius: 10,
                  padding: '8px 10px',
                  marginBottom: 8,
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 6,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', fontSize: 11.5 }}>
                  <span style={{ fontWeight: 600 }}>4-Candidate Draft Preview Grid — Click to inspect or Refine to Ultra</span>
                  <button className="oc-icon-btn" onClick={() => setDraftCandidates([])}>
                    ×
                  </button>
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, minmax(0, 1fr))', gap: 6 }}>
                  {draftCandidates.map((cand, idx) => {
                    const isSel = activeAsset?.id === cand.id;
                    return (
                      <div
                        key={cand.id || idx}
                        onClick={() => syncActiveAsset(cand)}
                        style={{
                          padding: '7px 8px',
                          borderRadius: 7,
                          cursor: 'pointer',
                          border: isSel ? '1px solid var(--ink-primary)' : '1px solid var(--border-subtle)',
                          background: isSel ? 'var(--bg-hover)' : 'var(--bg-surface)',
                          display: 'flex',
                          flexDirection: 'column',
                          gap: 4,
                        }}
                      >
                        <div style={{ fontSize: 11, fontWeight: 600, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                          {cand.variant_label || `Variant #${idx + 1}`}
                        </div>
                        <div style={{ fontSize: 10, color: 'var(--ink-muted)', fontFamily: 'var(--font-mono)' }}>
                          {(cand.telemetry?.faces || 0).toLocaleString()} tris
                        </div>
                        <button
                          className="oc-btn oc-btn-sm oc-btn-primary"
                          style={{ fontSize: 10.5, padding: '3px 6px' }}
                          onClick={(e) => {
                            e.stopPropagation();
                            refineCandidateToUltra(cand);
                          }}
                        >
                          Refine Ultra
                        </button>
                      </div>
                    );
                  })}
                </div>
              </div>
            )}
            {historyOpen && (
              <div className="oc-history-drawer">
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ fontWeight: 600, color: 'var(--ink-primary)' }}>Session Log</span>
                  <button className="oc-icon-btn" onClick={() => setHistoryOpen(false)}>
                    Close
                  </button>
                </div>
                {activityLog.map((m, i) => (
                  <div
                    key={i}
                    style={{
                      color:
                        m.role === 'error'
                          ? 'var(--sig-red)'
                          : m.role === 'user'
                          ? 'var(--ink-primary)'
                          : 'var(--ink-secondary)',
                    }}
                  >
                    <span style={{ color: 'var(--ink-muted)', marginRight: 6 }}>
                      {m.role === 'user' ? 'You:' : 'Falcon:'}
                    </span>
                    {m.text}
                  </div>
                ))}
                {pendingPlan && (
                  <div
                    style={{
                      marginTop: 4,
                      padding: 8,
                      borderRadius: 6,
                      border: '1px solid var(--sig-amber)',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      gap: 8,
                    }}
                  >
                    <span>{pendingPlan.summary}</span>
                    <div style={{ display: 'flex', gap: 6, flexShrink: 0 }}>
                      <button
                        className="oc-btn oc-btn-sm oc-btn-primary"
                        onClick={() => {
                          const p = pendingPlan;
                          setPendingPlan(null);
                          setAgentMode(p.isGame ? 'game' : 'build');
                          if (p.isGame) generateGameFromPrompt(p.prompt, false);
                          else generateFromText(p.prompt);
                        }}
                      >
                        Execute
                      </button>
                      <button className="oc-btn oc-btn-sm" onClick={() => setPendingPlan(null)}>
                        Cancel
                      </button>
                    </div>
                  </div>
                )}
              </div>
            )}

            {matchingSlash.length > 0 && (
              <div className="oc-slash-menu">
                {matchingSlash.map((s, idx) => (
                  <button
                    key={s.cmd}
                    className={`oc-slash-item ${idx === slashIdx % matchingSlash.length ? 'selected' : ''}`}
                    onClick={() => executeSlash(s.cmd)}
                  >
                    <span style={{ fontFamily: 'var(--font-mono)', fontWeight: 600 }}>{s.cmd}</span>
                    <span style={{ color: 'var(--ink-muted)' }}>{s.desc}</span>
                  </button>
                ))}
              </div>
            )}

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: 8, flexWrap: 'wrap' }}>
              {lastLog && (
                <button
                  className="oc-activity-bar"
                  onClick={() => setHistoryOpen((v) => !v)}
                  title="Click to view session log"
                >
                  <span className="oc-dot" />
                  <span style={{ whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', maxWidth: 380 }}>
                    {lastLog.text}
                  </span>
                </button>
              )}

              <div className="oc-quick-row">
                {STARTER_PROMPTS.map((sp) => (
                  <button
                    key={sp.label}
                    className="oc-quick-chip"
                    disabled={loading}
                    onClick={() => {
                      setAgentMode(sp.mode);
                      if (sp.mode === 'game') generateGameFromPrompt(sp.text, false);
                      else generateFromText(sp.text);
                    }}
                  >
                    {sp.label}
                  </button>
                ))}
              </div>
            </div>

            <div className="oc-composer-card">
              <textarea
                ref={composerRef}
                rows={1}
                className="oc-composer-input"
                placeholder={
                  agentMode === 'game'
                    ? 'Describe a playable 3D game to create or update...'
                    : agentMode === 'edit'
                    ? 'Describe how to edit this 3D model (e.g. apply brushed titanium, add spoiler)...'
                    : 'Ask Falcon to build a realistic 3D model, drop an image, or type / for commands...'
                }
                value={composerText}
                onChange={(e) => {
                  setComposerText(e.target.value);
                  setSlashIdx(0);
                }}
                onKeyDown={(e) => {
                  if (e.key === 'Tab' && !composerText.startsWith('/')) {
                    e.preventDefault();
                    cycleMode();
                  } else if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    if (matchingSlash.length > 0 && composerText.trim() === matchingSlash[0]?.cmd) {
                      executeSlash(matchingSlash[0].cmd);
                    } else {
                      submitPrompt();
                    }
                  }
                }}
              />

              <div className="oc-composer-bar">
                <div className="oc-composer-tools">
                  <button
                    type="button"
                    className={`oc-mode-pill ${agentMode}`}
                    onClick={cycleMode}
                    title="Click or press Tab to switch mode (Build / Edit / Game / Plan)"
                  >
                    {agentMode}
                  </button>

                  <button
                    type="button"
                    className="oc-meta-pill"
                    onClick={() => openProvidersModal('connect')}
                    title="Choose AI Provider, add API keys, or connect Ollama"
                  >
                    <span className="oc-dot" style={{ width: 6, height: 6 }} />
                    <span>{activeRouterLabel}</span>
                    <span>▾</span>
                  </button>

                  <button
                    type="button"
                    className="oc-meta-pill"
                    onClick={() => quickImageInputRef.current?.click()}
                    title="Upload PNG/JPEG reference image to reconstruct 3D model"
                  >
                    + Image
                  </button>

                  {agentMode !== 'game' && (
                    <select
                      className="oc-meta-pill"
                      value={qualityMode}
                      onChange={(e) => setQualityMode(e.target.value)}
                      title="3D Synthesis Engine: Auto CGT + Swarm, CGT Catmull-Clark Master, Hunyuan3D-2, TripoSG 1.5B, SF3D, 4-Draft Grid, or SIREN Bust"
                    >
                      <option value="ultra">Auto (CGT + Swarm)</option>
                      <option value="cgt_subdiv">CGT Master Subdiv</option>
                      <option value="hunyuan3d">Hunyuan3D-2 Neural</option>
                      <option value="triposg">TripoSG 1.5B Neural</option>
                      <option value="sf3d">SF3D Quad Neural</option>
                      <option value="draft4">4-Draft Grid</option>
                      <option value="neural_organic">SIREN Organic Bust</option>
                      <option value="csg_swarm">Hard-Surface CSG</option>
                      <option value="draft">Fast Draft</option>
                    </select>
                  )}
                </div>

                <button
                  type="button"
                  className="oc-send-btn"
                  disabled={loading || !composerText.trim()}
                  onClick={submitPrompt}
                >
                  <span>{loading ? 'Working...' : 'Send'}</span>
                  <span>↵</span>
                </button>
              </div>
            </div>
          </div>
        </div>
      </main>

      {/* ====================================================================
          3. ON-DEMAND STUDIO INSPECTOR (Closed by default for simplicity)
          ==================================================================== */}
      {inspectorOpen && (
        <aside className="oc-inspector">
          <div className="oc-inspector-tabs">
            {(studioMode === 'game'
              ? [
                  { id: 'parts', label: 'World' },
                  { id: 'swarm', label: 'Swarm' },
                  { id: 'json', label: 'Spec' },
                ]
              : [
                  { id: 'parts', label: 'Topo' },
                  { id: 'material', label: '4K PBR' },
                  { id: 'image', label: 'M-View' },
                  { id: 'print', label: 'Print' },
                  { id: 'rig', label: 'Rig 24' },
                  { id: 'plugins', label: 'Plugins' },
                  { id: 'swarm', label: 'Swarm' },
                ]
            ).map((t) => (
              <button
                key={t.id}
                className={`oc-insp-tab ${inspectorTab === t.id ? 'active' : ''}`}
                onClick={() => setInspectorTab(t.id)}
              >
                {t.label}
              </button>
            ))}
          </div>

          <div className="oc-panel-body">
            {studioMode === 'game' ? (
              inspectorTab === 'swarm' ? (
                <SwarmPanel
                  busy={loading}
                  stageMode={studioMode}
                  setStageMode={setStudioMode}
                  currentAsset={activeAsset}
                  currentGame={activeGame}
                  onRunModelDetail={async (detailPrompt) => {
                    if (!activeAsset) return;
                    setLoading(true);
                    setLoadingMsg('Adding detail pass...');
                    try {
                      const r = await fetch('/api/swarm/detail', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ asset_id: activeAsset.id, instruction: detailPrompt }),
                      });
                      const d = await r.json();
                      if (!d.error) syncActiveAsset(d);
                    } finally {
                      setLoading(false);
                    }
                  }}
                  onCreateGame={(p) => generateGameFromPrompt(p, false)}
                  onMutateGame={(p) => generateGameFromPrompt(p, true)}
                  onExportGameHtml={() => downloadExport('html')}
                  onExportGameCs={() => downloadExport('game_cs')}
                />
              ) : inspectorTab === 'json' ? (
                <div className="field-group">
                  <div className="field-label">
                    <span>Game World JSON</span>
                  </div>
                  <textarea
                    rows={22}
                    className="textarea-input"
                    style={{ fontSize: 11 }}
                    value={bpJsonText}
                    onChange={(e) => setBpJsonText(e.target.value)}
                  />
                </div>
              ) : (
                <div className="field-group">
                  <div className="field-label">
                    <span>Active Game World</span>
                  </div>
                  {activeGame ? (
                    <>
                      <div style={{ fontWeight: 600 }}>{activeGame.title}</div>
                      <div style={{ fontSize: 12, color: 'var(--ink-secondary)' }}>
                        {activeGame.subtitle || activeGame.objective}
                      </div>
                      <button className="oc-btn oc-btn-primary" onClick={() => downloadExport('html')}>
                        Export Standalone HTML5 Game
                      </button>
                    </>
                  ) : (
                    <div style={{ color: 'var(--ink-muted)' }}>Generate a game from the prompt bar first.</div>
                  )}
                </div>
              )
            ) : (
              <>
                {inspectorTab === 'parts' && (
                  <>
                    <div className="stat-grid-2">
                      <div className="stat-box">
                        <div className="k">{activeAsset?.quad_mode ? 'Quads / Tris' : 'Triangles'}</div>
                        <div className="v">
                          {activeAsset?.quad_mode && activeAsset?.quad_count > 0
                            ? `${activeAsset.quad_count.toLocaleString()}q`
                            : trisCount.toLocaleString()}
                        </div>
                      </div>
                      <div className="stat-box">
                        <div className="k">Size (mm)</div>
                        <div className="v" style={{ fontSize: 11 }}>
                          {dim.x || 0}×{dim.y || 0}×{dim.z || 0}
                        </div>
                      </div>
                    </div>

                    {/* Smart Quad Remesh & Exact Target Polycount (Garland-Heckbert QEM) */}
                    <div className="field-group">
                      <div className="field-label">
                        <span>Smart Topology & Quad Remesh</span>
                        <span style={{ fontFamily: 'var(--font-mono)', fontSize: 10.5 }}>
                          {Number(remeshTargetFaces).toLocaleString()} faces
                        </span>
                      </div>
                      <div className="row-2">
                        <button
                          className={`oc-btn oc-btn-sm ${remeshQuadMode ? 'oc-btn-primary' : ''}`}
                          onClick={() => setRemeshQuadMode(true)}
                        >
                          Quad-Dominant
                        </button>
                        <button
                          className={`oc-btn oc-btn-sm ${!remeshQuadMode ? 'oc-btn-primary' : ''}`}
                          onClick={() => setRemeshQuadMode(false)}
                        >
                          Triangles
                        </button>
                      </div>
                      <input
                        type="range"
                        min={500}
                        max={120000}
                        step={500}
                        value={remeshTargetFaces}
                        onChange={(e) => setRemeshTargetFaces(Number(e.target.value))}
                      />
                      <div style={{ display: 'flex', gap: 4 }}>
                        {[1500, 8000, 25000, 60000].map((cnt) => (
                          <button
                            key={cnt}
                            className="oc-btn oc-btn-sm"
                            style={{ flex: 1, fontSize: 10.5 }}
                            onClick={() => setRemeshTargetFaces(cnt)}
                          >
                            {cnt >= 1000 ? `${cnt / 1000}k` : cnt}
                          </button>
                        ))}
                      </div>
                      <button className="oc-btn oc-btn-primary" disabled={loading} onClick={runSmartRemesh}>
                        Run Smart {remeshQuadMode ? 'Quad ' : ''}Remesh
                      </button>
                      <button className="oc-btn" disabled={loading} onClick={() => importMeshInputRef.current?.click()}>
                        + Import External Mesh (.GLB / .OBJ / .STL / .PLY)
                      </button>
                    </div>

                    <div className="field-group">
                      <div className="field-label">
                        <span>Sub-Meshes ({parts.length})</span>
                        {selectedPartIdx >= 0 && (
                          <button className="oc-icon-btn" onClick={() => setSelectedPartIdx(-1)}>
                            All
                          </button>
                        )}
                      </div>
                      <div className="part-list">
                        {parts.map((pt, idx) => {
                          const swatch =
                            typeof pt.color === 'string'
                              ? pt.color
                              : Array.isArray(pt.color)
                              ? `rgb(${Math.round(pt.color[0] * 255)}, ${Math.round(pt.color[1] * 255)}, ${Math.round(
                                  pt.color[2] * 255
                                )})`
                              : '#94a3b8';
                          return (
                            <div
                              key={pt.id || idx}
                              className={`part-row ${selectedPartIdx === idx ? 'selected' : ''}`}
                              onClick={() => setSelectedPartIdx(idx === selectedPartIdx ? -1 : idx)}
                            >
                              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                                <span className="swatch-dot" style={{ background: swatch }} />
                                <span>{pt.name || pt.id}</span>
                              </div>
                              <span style={{ fontSize: 11, color: 'var(--ink-muted)' }}>{pt.primitive || 'mesh'}</span>
                            </div>
                          );
                        })}
                      </div>
                    </div>
                  </>
                )}

                {inspectorTab === 'material' && (
                  <>
                    <div className="field-group">
                      <div className="field-label">
                        <span>HD PBR Bake Resolution</span>
                        <span style={{ fontFamily: 'var(--font-mono)', fontSize: 10.5 }}>{texResolution}×{texResolution}</span>
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 5 }}>
                        {[
                          { r: 256, lbl: '256' },
                          { r: 1024, lbl: '1K' },
                          { r: 2048, lbl: '2K HD' },
                          { r: 4096, lbl: '4K Ultra' },
                        ].map((opt) => (
                          <button
                            key={opt.r}
                            className={`oc-btn oc-btn-sm ${texResolution === opt.r ? 'oc-btn-primary' : ''}`}
                            onClick={() => setTexResolution(opt.r)}
                          >
                            {opt.lbl}
                          </button>
                        ))}
                      </div>
                    </div>

                    <div className="field-group">
                      <div className="field-label">
                        <span>AI Prompt-to-PBR Texture</span>
                      </div>
                      <input
                        type="text"
                        className="text-input"
                        placeholder="e.g. battle-worn gold armor, anime cel, organic skin..."
                        value={aiTexPrompt}
                        onChange={(e) => setAiTexPrompt(e.target.value)}
                      />
                      <button className="oc-btn oc-btn-primary" disabled={loading || !aiTexPrompt.trim()} onClick={runAiTexture}>
                        Synthesize {texResolution >= 4096 ? '4K' : texResolution >= 2048 ? '2K' : '1K'} PBR Texture
                      </button>
                    </div>

                    <div className="field-group">
                      <div className="field-label">
                        <span>3D Region Brush Healer</span>
                        <button
                          className={`oc-btn oc-btn-sm ${regionBrushActive ? 'oc-btn-primary' : ''}`}
                          onClick={() => setRegionBrushActive((v) => !v)}
                        >
                          {regionBrushActive ? 'Brush ON (Click 3D)' : 'Enable Brush'}
                        </button>
                      </div>
                      {regionBrushActive && (
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                          <input
                            type="color"
                            value={regionBrushColor}
                            onChange={(e) => setRegionBrushColor(e.target.value)}
                            style={{ width: 34, height: 28, border: 'none', background: 'transparent', cursor: 'pointer' }}
                          />
                          <input
                            type="range"
                            min={0.1}
                            max={0.9}
                            step={0.05}
                            value={regionBrushRadius}
                            onChange={(e) => setRegionBrushRadius(Number(e.target.value))}
                            style={{ flex: 1 }}
                          />
                          <span style={{ fontSize: 10.5, fontFamily: 'var(--font-mono)' }}>r={regionBrushRadius}</span>
                        </div>
                      )}
                    </div>

                    <div className="field-group">
                      <div className="field-label">
                        <span>PBR & Stylized Finishes</span>
                        <span style={{ textTransform: 'none', fontWeight: 400 }}>
                          {selectedPartIdx >= 0 ? `Part #${selectedPartIdx + 1}` : 'Whole Model'}
                        </span>
                      </div>
                      <div className="chip-grid">
                        {MATERIAL_PRESETS.map((m) => (
                          <button
                            key={m.id}
                            className="chip"
                            disabled={loading}
                            onClick={() => applyMaterial(m)}
                          >
                            {m.label}
                          </button>
                        ))}
                      </div>
                    </div>
                  </>
                )}

                {inspectorTab === 'image' && (
                  <>
                    <div className="field-group">
                      <div className="field-label">
                        <span>Multi-View Photo-to-3D (1–4 Views)</span>
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 6 }}>
                        {[
                          { key: 'front', label: 'Front (Req)' },
                          { key: 'side', label: 'Right / Side' },
                          { key: 'back', label: 'Back View' },
                          { key: 'top', label: 'Top View' },
                        ].map((slot) => (
                          <label
                            key={slot.key}
                            className="oc-btn oc-btn-sm"
                            style={{ justifyContent: 'space-between', cursor: 'pointer', overflow: 'hidden' }}
                          >
                            <span>{mvFiles[slot.key] ? `✓ ${slot.key}` : slot.label}</span>
                            <input
                              type="file"
                              accept="image/png,image/jpeg,image/bmp"
                              style={{ display: 'none' }}
                              onChange={(e) => {
                                const f = e.target.files?.[0] || null;
                                setMvFiles((prev) => ({ ...prev, [slot.key]: f }));
                              }}
                            />
                          </label>
                        ))}
                      </div>
                      <button
                        className="oc-btn oc-btn-primary"
                        disabled={loading || (!mvFiles.front && !mvFiles.side)}
                        onClick={handleMultiViewGenerate}
                      >
                        Carve Multi-View 3D + Project RGB
                      </button>
                    </div>

                    <div className="field-group">
                      <div className="field-label">
                        <span>Single Image to 3D</span>
                      </div>
                      <select
                        className="select-input"
                        value={imgMode}
                        onChange={(e) => setImgMode(e.target.value)}
                      >
                        <option value="volumetric">Volumetric Silhouette 3D</option>
                        <option value="relief">2.5D Relief</option>
                        <option value="lithophane">Backlit Lithophane</option>
                      </select>
                      <button className="oc-btn" onClick={() => quickImageInputRef.current?.click()}>
                        Choose PNG / JPEG Image...
                      </button>
                    </div>
                    <div className="field-group">
                      <div className="field-label">
                        <span>Sketch to 3D</span>
                      </div>
                      <SketchPad busy={loading} onGenerateFromSketch={handleSketchDataUrl} />
                    </div>
                  </>
                )}

                {inspectorTab === 'print' && (
                  <>
                    <div className="field-group">
                      <div className="field-label">
                        <span>Exact CAD Dimensions (mm)</span>
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 6 }}>
                        <input
                          type="number"
                          className="text-input"
                          placeholder="X mm"
                          value={cadMmX}
                          onChange={(e) => setCadMmX(e.target.value)}
                        />
                        <input
                          type="number"
                          className="text-input"
                          placeholder="Y mm"
                          value={cadMmY}
                          onChange={(e) => setCadMmY(e.target.value)}
                        />
                        <input
                          type="number"
                          className="text-input"
                          placeholder="Z mm"
                          value={cadMmZ}
                          onChange={(e) => setCadMmZ(e.target.value)}
                        />
                      </div>
                      <button className="oc-btn oc-btn-primary" onClick={applyCadScale}>
                        Apply Exact Millimeters
                      </button>
                    </div>
                    <button className="oc-btn" onClick={repairManifold}>
                      Repair Watertight Manifold
                    </button>
                    <button className="oc-btn" onClick={splitWithPins}>
                      Auto-Split + 0.25mm Alignment Pins
                    </button>
                    <div className="field-group" style={{ marginTop: 6 }}>
                      <div className="field-label">
                        <span>1-Click Send to Slicer (.3MF)</span>
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 6 }}>
                        {[
                          { id: 'bambu', name: 'Bambu Studio' },
                          { id: 'orca', name: 'OrcaSlicer' },
                          { id: 'prusa', name: 'PrusaSlicer' },
                          { id: 'cura', name: 'UltiMaker Cura' },
                          { id: 'elegoo', name: 'ElegooSlicer' },
                          { id: 'creality', name: 'Creality Print' },
                        ].map((sl) => (
                          <button key={sl.id} className="oc-btn oc-btn-sm" onClick={() => launchDesktopSlicer(sl.id)}>
                            ↗ {sl.name}
                          </button>
                        ))}
                      </div>
                    </div>
                  </>
                )}

                {inspectorTab === 'rig' && (
                  <>
                    <div className="field-group">
                      <div className="field-label">
                        <span>Volumetric 4-Bone Auto-Rigger</span>
                      </div>
                      <select
                        className="select-input"
                        value={rigTemplate}
                        onChange={(e) => setRigTemplate(e.target.value)}
                      >
                        <option value="auto">Auto-Detect Skeleton</option>
                        <option value="biped">Humanoid Biped (17 Bones)</option>
                        <option value="winged">Winged Hero / Dragon (21 Bones)</option>
                        <option value="quadruped">Quadruped Creature (14 Bones)</option>
                        <option value="vehicle">Vehicle Chassis + 4 Wheels</option>
                      </select>
                      <button className="oc-btn oc-btn-primary" disabled={loading} onClick={() => runAutoRig(selectedClipId)}>
                        Bind Skin + Bake 24 Motion Clips
                      </button>
                      <div className="row-2">
                        <button className="oc-btn oc-btn-sm" onClick={() => downloadExport('fbx')}>
                          Export Rigged .FBX
                        </button>
                        <button className="oc-btn oc-btn-sm" onClick={() => downloadExport('glb')}>
                          Export Animated .GLB
                        </button>
                      </div>
                    </div>

                    <div className="field-group">
                      <div className="field-label">
                        <span>24-Clip Motion Preset Library</span>
                        <button className="oc-icon-btn" onClick={() => setAnimMode('none')}>
                          Rest Pose
                        </button>
                      </div>
                      <div style={{ display: 'flex', flexDirection: 'column', gap: 4, maxHeight: 290, overflowY: 'auto' }}>
                        {(motionLibrary.length > 0
                          ? motionLibrary
                          : [
                              { id: 'idle_breathe', name: 'Idle Breathing', category: 'Locomotion' },
                              { id: 'walk_standard', name: 'Natural Walk Cycle', category: 'Locomotion' },
                              { id: 'run_sprint', name: 'Athletic Sprint Run', category: 'Locomotion' },
                              { id: 'sword_slash', name: 'Katana / Sword Slash', category: 'Combat' },
                              { id: 'wing_flap_soar', name: 'Dragon Wing Flap', category: 'Creature' },
                            ]
                        ).map((clip) => (
                          <button
                            key={clip.id}
                            className={`oc-btn oc-btn-sm ${animMode === clip.id ? 'oc-btn-primary' : ''}`}
                            style={{ justifyContent: 'space-between' }}
                            onClick={() => {
                              setSelectedClipId(clip.id);
                              setAnimMode(clip.id);
                            }}
                          >
                            <span>{clip.name}</span>
                            <span style={{ fontSize: 10, opacity: 0.7 }}>{clip.category}</span>
                          </button>
                        ))}
                      </div>
                    </div>
                  </>
                )}

                {inspectorTab === 'plugins' && (
                  <div className="field-group">
                    <div className="field-label">
                      <span>Official Zero-Python Live-Link Plugins</span>
                    </div>
                    <div style={{ fontSize: 11.5, color: 'var(--ink-secondary)', lineHeight: 1.45 }}>
                      Stream Quad-Remeshed, 4K PBR, Skinned `.FBX` / `.GLB` / `.USDZ` assets straight into your engine:
                    </div>
                    {[
                      { slug: 'unity', title: 'Unity 6 / 2022 LTS Plugin', tech: 'Pure C# EditorWindow (.cs)' },
                      { slug: 'unreal', title: 'Unreal Engine 5.4 / 5.5', tech: 'Native C++ EditorSubsystem' },
                      { slug: 'godot', title: 'Godot 4.x Dock Plugin', tech: 'Pure GDScript (@tool)' },
                      { slug: 'roblox', title: 'Roblox Studio Plugin', tech: 'Pure Luau Widget (.lua)' },
                      { slug: 'blender', title: 'Blender / Maya / C4D Bridge', tech: 'Compiled C++20 Watch-Daemon' },
                    ].map((pl) => (
                      <div
                        key={pl.slug}
                        style={{
                          padding: '8px 10px',
                          borderRadius: 8,
                          border: '1px solid var(--border-subtle)',
                          background: 'var(--bg-surface)',
                          display: 'flex',
                          justifyContent: 'space-between',
                          alignItems: 'center',
                          gap: 8,
                        }}
                      >
                        <div>
                          <div style={{ fontSize: 12, fontWeight: 600 }}>{pl.title}</div>
                          <div style={{ fontSize: 10.5, color: 'var(--ink-muted)' }}>{pl.tech}</div>
                        </div>
                        <button className="oc-btn oc-btn-sm oc-btn-primary" onClick={() => downloadPluginZip(pl.slug)}>
                          Download .zip
                        </button>
                      </div>
                    ))}
                  </div>
                )}

                {inspectorTab === 'swarm' && (
                  <SwarmPanel
                    busy={loading}
                    stageMode={studioMode}
                    setStageMode={setStudioMode}
                    currentAsset={activeAsset}
                    currentGame={activeGame}
                    onRunModelDetail={async (detailPrompt) => {
                      if (!activeAsset) return;
                      setLoading(true);
                      setLoadingMsg('Adding detail pass...');
                      try {
                        const r = await fetch('/api/swarm/detail', {
                          method: 'POST',
                          headers: { 'Content-Type': 'application/json' },
                          body: JSON.stringify({ asset_id: activeAsset.id, instruction: detailPrompt }),
                        });
                        const d = await r.json();
                        if (!d.error) syncActiveAsset(d);
                      } finally {
                        setLoading(false);
                      }
                    }}
                    onCreateGame={(p) => generateGameFromPrompt(p, false)}
                    onMutateGame={(p) => generateGameFromPrompt(p, true)}
                    onExportGameHtml={() => downloadExport('html')}
                    onExportGameCs={() => downloadExport('game_cs')}
                  />
                )}

                {inspectorTab === 'json' && (
                  <div className="field-group">
                    <div className="field-label">
                      <span>Parametric Blueprint JSON</span>
                      <button className="oc-btn oc-btn-sm oc-btn-primary" onClick={recompileBlueprint}>
                        Recompile
                      </button>
                    </div>
                    <textarea
                      rows={22}
                      className="textarea-input"
                      style={{ fontSize: 11 }}
                      value={bpJsonText}
                      onChange={(e) => setBpJsonText(e.target.value)}
                    />
                  </div>
                )}
              </>
            )}
          </div>
        </aside>
      )}

      <HardwareModal
        open={settingsOpen}
        onClose={() => {
          setSettingsOpen(false);
          refreshProviderCounts();
        }}
        hardware={hardware}
        routerCfg={routerCfg}
        onSaveRouter={saveRouterSettings}
        onRescan={rescanHardware}
        gridRes={gridRes}
        setGridRes={setGridRes}
        theme={theme}
        onToggleTheme={toggleTheme}
        initialSection={settingsSection}
      />

      <CommandPalette open={paletteOpen} onClose={() => setPaletteOpen(false)} commands={paletteCommands} />
    </div>
  );
}
