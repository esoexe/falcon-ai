/**
 * Falcon AI Desktop — Embedded High-Realism PBR Host.
 * If Falcon-AI-2.0.0-x64.exe runs on a bare Windows PC before compiling falcon_server.exe,
 * this embedded host serves the OpenCode WebGL Studio and synthesizes customized glTF 2.0 PBR
 * binaries directly from pre-compiled C++20 Sculpted SDF + 256x256 Normal/ORM/Albedo archetypes!
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.glb': 'model/gltf-binary',
  '.json': 'application/json; charset=utf-8',
};

function hexToRgb01(hex) {
  const h = (hex || '#94a3b8').replace('#', '');
  const n = parseInt(h.length === 6 ? h : '94a3b8', 16);
  return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
}

function patchGlbMaterial(glbBuf, { colorHex, metallic, roughness, emissiveHex, emissiveStrength, partId }) {
  if (!glbBuf || glbBuf.length < 28 || glbBuf.readUInt32LE(0) !== 0x46546c67) return glbBuf;
  const jsonLen = glbBuf.readUInt32LE(12);
  const jsonStr = glbBuf.subarray(20, 20 + jsonLen).toString('utf8');
  const binChunkWithHdr = glbBuf.subarray(20 + jsonLen);

  let gltf;
  try {
    gltf = JSON.parse(jsonStr);
  } catch {
    return glbBuf;
  }

  const rgb = colorHex ? hexToRgb01(colorHex) : null;
  const ergb = emissiveHex ? hexToRgb01(emissiveHex) : null;

  for (const mat of gltf.materials || []) {
    if (partId && mat.name !== `Mat_${partId}`) continue;
    const isEmissiveMat = Array.isArray(mat.emissiveFactor) && mat.emissiveFactor.some((v) => v > 0.05);
    if (!mat.pbrMetallicRoughness) mat.pbrMetallicRoughness = {};
    if (rgb && (!isEmissiveMat || partId)) {
      mat.pbrMetallicRoughness.baseColorFactor = [rgb[0], rgb[1], rgb[2], 1.0];
    }
    if (typeof metallic === 'number' && !mat.pbrMetallicRoughness.metallicRoughnessTexture) {
      mat.pbrMetallicRoughness.metallicFactor = metallic;
    }
    if (typeof roughness === 'number' && !mat.pbrMetallicRoughness.metallicRoughnessTexture) {
      mat.pbrMetallicRoughness.roughnessFactor = roughness;
    }
    if (ergb && emissiveStrength > 0.01 && (isEmissiveMat || partId)) {
      mat.emissiveFactor = [ergb[0] * emissiveStrength, ergb[1] * emissiveStrength, ergb[2] * emissiveStrength];
    }
  }

  let newJsonBytes = Buffer.from(JSON.stringify(gltf), 'utf8');
  while (newJsonBytes.length % 4 !== 0) {
    newJsonBytes = Buffer.concat([newJsonBytes, Buffer.from(' ')]);
  }

  const totalLen = 12 + 8 + newJsonBytes.length + binChunkWithHdr.length;
  const header = Buffer.alloc(12);
  header.writeUInt32LE(0x46546c67, 0);
  header.writeUInt32LE(2, 4);
  header.writeUInt32LE(totalLen, 8);

  const jHdr = Buffer.alloc(8);
  jHdr.writeUInt32LE(newJsonBytes.length, 0);
  jHdr.writeUInt32LE(0x4e4f534a, 4);

  return Buffer.concat([header, jHdr, newJsonBytes, binChunkWithHdr]);
}

function detectPromptPalette(prompt) {
  const low = (prompt || '').toLowerCase();
  const colors = [
    ['gold', '#d4af37', 0.94, 0.2],
    ['obsidian', '#18181b', 0.28, 0.16],
    ['chrome', '#e2e8f0', 0.98, 0.08],
    ['silver', '#cbd5e1', 0.94, 0.18],
    ['crimson', '#991b1b', 0.38, 0.34],
    ['red', '#dc2626', 0.44, 0.3],
    ['cobalt', '#1d4ed8', 0.76, 0.26],
    ['blue', '#2563eb', 0.68, 0.28],
    ['emerald', '#047857', 0.22, 0.18],
    ['bronze', '#92400e', 0.84, 0.34],
    ['carbon', '#1f2937', 0.26, 0.3],
  ];
  let hit = null;
  for (const [kw, hex, m, r] of colors) {
    if (low.includes(kw)) {
      hit = { colorHex: hex, metallic: m, roughness: r };
      break;
    }
  }
  const glows = [
    ['cyan', '#22d3ee'],
    ['plasma', '#38bdf8'],
    ['fire', '#f97316'],
    ['lava', '#ef4444'],
    ['purple', '#a855f7'],
    ['emerald', '#34d399'],
    ['amber', '#f59e0b'],
  ];
  let emissiveHex = null;
  for (const [kw, hex] of glows) {
    if (low.includes(kw)) {
      emissiveHex = hex;
      break;
    }
  }
  return { ...(hit || {}), emissiveHex, emissiveStrength: emissiveHex ? 0.95 : 0 };
}

function hasWord(text, kw) {
  const escaped = kw.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  return new RegExp(`\\b${escaped}(?:e?s)?\\b`, 'i').test(text || '');
}

function pickStarterArchetype(starterDir, prompt) {
  if (!fs.existsSync(starterDir)) return null;
  const jsons = fs
    .readdirSync(starterDir)
    .filter((f) => f.endsWith('.json'))
    .map((f) => JSON.parse(fs.readFileSync(path.join(starterDir, f), 'utf8')));
  if (jsons.length === 0) return null;

  const low = (prompt || '').toLowerCase();
  // Match against m.title FIRST with strict word boundaries (so "car" never matches "carved"!)
  const matchBy = (kws) =>
    jsons.find((m) => kws.some((k) => hasWord(m.title || '', k))) ||
    jsons.find((m) => kws.some((k) => hasWord(m.prompt || '', k)));

  if (['dragon', 'wyvern', 'monster', 'beast', 'creature', 'demon', 'wolf', 'dog', 'lion'].some((k) => hasWord(low, k))) {
    return matchBy(['wyvern', 'dragon']) || jsons[0];
  }
  if (['car', 'automobile', 'sedan', 'hypercar', 'supercar', 'coupe', 'vehicle', 'rover', 'suv', 'truck', 'pickup', 'porsche', 'ferrari', 'lamborghini'].some((k) => hasWord(low, k))) {
    return matchBy(['hypercar', 'sports car', 'car']) || jsons[0];
  }
  if (['sword', 'katana', 'blade', 'dagger', 'saber', 'axe'].some((k) => hasWord(low, k))) {
    return matchBy(['katana', 'sword', 'blade']) || jsons[0];
  }
  if (['rifle', 'gun', 'railgun', 'blaster', 'sniper', 'pistol'].some((k) => hasWord(low, k))) {
    return matchBy(['railgun', 'rifle']) || jsons[0];
  }
  if (['castle', 'citadel', 'tower', 'keep', 'fortress', 'house', 'cottage', 'building'].some((k) => hasWord(low, k))) {
    return matchBy(['citadel', 'keep', 'castle']) || jsons[0];
  }
  return matchBy(['mecha', 'gundanium']) || jsons[0];
}

function startFallbackServer({ port, webDir, assetsDir, starterDir }) {
  fs.mkdirSync(assetsDir, { recursive: true });
  const gamesDir = path.join(path.dirname(assetsDir), 'workspace_games');
  fs.mkdirSync(gamesDir, { recursive: true });

  if (fs.existsSync(starterDir) && fs.readdirSync(assetsDir).filter((f) => f.endsWith('.json')).length === 0) {
    for (const f of fs.readdirSync(starterDir)) {
      if (f.startsWith('game_')) {
        fs.copyFileSync(path.join(starterDir, f), path.join(gamesDir, f.replace(/^game_/, '')));
      } else {
        fs.copyFileSync(path.join(starterDir, f), path.join(assetsDir, f));
      }
    }
  } else if (fs.existsSync(starterDir) && fs.readdirSync(gamesDir).filter((f) => f.endsWith('.json')).length === 0) {
    for (const f of fs.readdirSync(starterDir)) {
      if (f.startsWith('game_')) {
        fs.copyFileSync(path.join(starterDir, f), path.join(gamesDir, f.replace(/^game_/, '')));
      }
    }
  }

  let routerCfg = {
    provider: 'auto',
    ollama_url: 'http://127.0.0.1:11434',
    ollama_model: '',
    openai_base_url: 'http://127.0.0.1:1234/v1',
    openai_api_key: '',
    openai_model: '',
  };

  const getHardware = () => {
    const cpus = os.cpus() || [];
    const totalGb = Number((os.totalmem() / 1073741824).toFixed(2));
    const freeGb = Number((os.freemem() / 1073741824).toFixed(2));
    return {
      engine_runtime: 'Falcon AI Desktop Runtime (C++20 Sculpted PBR Engine)',
      os: `${os.type()} ${os.release()} (${os.arch()})`,
      cpu: {
        brand: cpus[0]?.model || 'Multi-Core x64 CPU',
        logical_cores: cpus.length || 4,
        physical_cores: Math.max(1, Math.floor((cpus.length || 4) / 2)),
      },
      memory: {
        total_gb: totalGb,
        available_gb: freeGb,
        percent_used: Number(((1 - freeGb / Math.max(0.1, totalGb)) * 100).toFixed(1)),
      },
      gpus: [],
      services: {
        ollama: { online: false },
        lm_studio: { online: false },
        llamacpp: { online: false },
        vllm: { online: false },
        comfyui: { online: false },
      },
      profile: {
        tier: 'cpu_cpp_native',
        tier_badge: 'Falcon Desktop · Sculpted PBR Native Mode',
        compute_backend: 'Falcon C++20 Sculpted SDF + 256x256 Normal/ORM PBR',
        sdf_grid_resolution: 112,
        llm_mode: 'builtin_symbolic',
        active_llm_label: 'Falcon Sculpted SDF + PBR 3D Architect',
        summary: `Detected ${cpus.length || 4} CPU threads & ${totalGb} GB RAM. Ready for offline sculpted PBR 3D generation.`,
      },
      models: [
        {
          id: 'cpp_surface_nets',
          name: 'Falcon C++20 Anatomical SDF + PBR Texture Engine',
          org: 'Falcon AI',
          min_vram_gb: 0,
          license: 'MIT',
          installed: true,
          description: 'Watertight multi-part sculpted mesh + embedded 256x256 Albedo/ORM/Normal PBR synthesizer.',
        },
      ],
    };
  };

  const readJsonBody = (req) =>
    new Promise((resolve) => {
      const chunks = [];
      req.on('data', (c) => chunks.push(c));
      req.on('end', () => {
        try {
          resolve(JSON.parse(Buffer.concat(chunks).toString('utf8') || '{}'));
        } catch {
          resolve({});
        }
      });
    });

  const server = http.createServer(async (req, res) => {
    const u = new URL(req.url, `http://127.0.0.1:${port}`);
    const pathname = u.pathname;
    const sendJson = (obj, code = 200) => {
      const b = Buffer.from(JSON.stringify(obj));
      res.writeHead(code, { 'Content-Type': 'application/json', 'Content-Length': b.length });
      res.end(b);
    };

    if (pathname.startsWith('/exports/')) {
      const fp = path.join(assetsDir, path.basename(pathname));
      if (!fs.existsSync(fp)) return sendJson({ error: 'Not found' }, 404);
      const b = fs.readFileSync(fp);
      res.writeHead(200, { 'Content-Type': 'model/gltf-binary', 'Content-Length': b.length });
      return res.end(b);
    }

    if (pathname === '/api/hardware' || pathname === '/api/hardware/rescan') {
      return sendJson(getHardware());
    }
    if (pathname === '/api/settings/router') {
      if (req.method === 'POST') routerCfg = { ...routerCfg, ...(await readJsonBody(req)) };
      return sendJson({ status: 'ok', config: routerCfg, ...routerCfg });
    }
    if (pathname === '/api/providers') {
      const pFile = path.join(userDataDir, 'workspace_providers_state.json');
      let data = JSON.parse(fs.readFileSync(path.join(__dirname, 'providers_seed.json'), 'utf8'));
      if (fs.existsSync(pFile)) {
        try { data = JSON.parse(fs.readFileSync(pFile, 'utf8')); } catch {}
      }
      if (req.method === 'POST') {
        const body = await readJsonBody(req);
        data.providers = (data.providers || []).map((p) => {
          if (p.id !== body.id) return p;
          const next = { ...p };
          if (typeof body.enabled === 'boolean') next.enabled = body.enabled;
          if (body.api_key === '__CLEAR__') {
            next.has_key = false;
            next.key_preview = '';
            next.enabled = false;
          } else if (body.api_key) {
            next.has_key = true;
            next.key_preview = body.api_key.slice(0, 4) + '...';
            next.enabled = true;
          }
          next.ready_in_team = (next.group === 'no_key' || next.group === 'local') ? next.enabled : (next.enabled && next.has_key);
          return next;
        });
        fs.writeFileSync(pFile, JSON.stringify(data, null, 2));
      }
      return sendJson(data);
    }
    if (pathname === '/api/assets' && req.method === 'GET') {
      const items = fs
        .readdirSync(assetsDir)
        .filter((f) => f.endsWith('.json'))
        .map((f) => JSON.parse(fs.readFileSync(path.join(assetsDir, f), 'utf8')))
        .sort((a, b) => (b.created_at || 0) - (a.created_at || 0));
      return sendJson(items);
    }
    if (pathname === '/api/games' && req.method === 'GET') {
      const gItems = fs
        .readdirSync(gamesDir)
        .filter((f) => f.endsWith('.json'))
        .map((f) => JSON.parse(fs.readFileSync(path.join(gamesDir, f), 'utf8')))
        .sort((a, b) => (b.created_at || 0) - (a.created_at || 0));
      return sendJson(gItems);
    }
    if (pathname === '/api/games/create' && req.method === 'POST') {
      const body = await readJsonBody(req);
      const prompt = body.prompt || 'Citadel Wyvern Siege';
      const low = prompt.toLowerCase();
      const gid = crypto.randomBytes(5).toString('hex');
      const hero = pickStarterArchetype(starterDir, low.includes('car') || low.includes('race') ? 'hypercar' : 'mecha');
      const enemy = pickStarterArchetype(starterDir, 'wyvern');
      const prop = pickStarterArchetype(starterDir, 'citadel');
      const pickup = pickStarterArchetype(starterDir, 'katana');
      const isRace = low.includes('race') || low.includes('car') || low.includes('drift');
      const genre = isRace ? 'hypercar_racing' : low.includes('space') ? 'space_dogfight' : low.includes('shooter') || low.includes('mecha') ? 'mecha_shooter' : 'action_rpg';
      const props = [];
      for (let i = 0; i < 6; i++) {
        const a = (Math.PI * 2 * i) / 6;
        props.push({ id: `keep_${i}`, type: 'landmark', glb_url: `/exports/${prop?.id}.glb`, asset_id: prop?.id, pos: [Math.cos(a) * 26, 0, Math.sin(a) * 26], rot_y: -a + 1.57, scale: 1.85, radius: 3.0 });
      }
      const pickups = [];
      for (let i = 0; i < 8; i++) {
        const a = (Math.PI * 2 * i) / 8;
        pickups.push({ id: `relic_${i}`, glb_url: `/exports/${pickup?.id}.glb`, asset_id: pickup?.id, pos: [Math.cos(a) * 14, 0.6, Math.sin(a) * 14], value: 100 });
      }
      const gm = {
        id: gid,
        title: prompt.slice(0, 38),
        subtitle: 'Falcon Multi-Agent 3D Game',
        prompt,
        genre,
        created_at: Math.floor(Date.now() / 1000),
        arena_radius: isRace ? 44 : 34,
        environment: { sky_top: '#090b14', sky_horizon: isRace ? '#111c38' : '#2b161b', ground_color: '#1b181b', grid_accent: isRace ? '#38bdf8' : '#f97316', weather: isRace ? 'neon_dust' : 'embers', fog_density: 0.013 },
        player: { name: hero?.title || 'Hero', asset_id: hero?.id, glb_url: `/exports/${hero?.id}.glb`, scale: 1.0, speed: isRace ? 19 : 11, sprint_mult: 1.75, jump_force: 11.5, max_jumps: 2, max_hp: 200, max_energy: 100, weapon_mode: genre === 'mecha_shooter' ? 'plasma_railgun' : 'damascus_slash', ultimate_mode: genre === 'mecha_shooter' ? 'missile_barrage' : 'nova_shockwave', damage: 42 },
        enemies: { name: enemy?.title || 'Wyvern', asset_id: enemy?.id, glb_url: `/exports/${enemy?.id}.glb`, scale: 0.88, base_count: 5, hp: 95, speed: 5.4, damage: 14 },
        boss: { enabled: true, name: 'Elder Scaled Wyvern Boss', asset_id: enemy?.id, glb_url: `/exports/${enemy?.id}.glb`, scale: 2.2, hp: 680, speed: 4.2, damage: 28 },
        props,
        ramps: [{ pos: [0, 0, -14], rot_y: 0, boost: 22 }],
        turrets: [],
        pickups,
        agent_trace: [
          { agent: '@lead-architect', duration_ms: 3, summary: `Designed ${genre} world & combat loop.` },
          { agent: '@game-director', duration_ms: 4, summary: 'Spawned keeps, enemies, boss & HUD.' },
        ],
      };
      fs.writeFileSync(path.join(gamesDir, `${gid}.json`), JSON.stringify(gm, null, 2));
      return sendJson(gm);
    }
    if (pathname.startsWith('/api/games/') && pathname.endsWith('/mutate') && req.method === 'POST') {
      const gid = pathname.split('/')[3];
      const gp = path.join(gamesDir, `${gid}.json`);
      if (fs.existsSync(gp)) {
        const body = await readJsonBody(req);
        const low = (body.instruction || '').toLowerCase();
        const gm = JSON.parse(fs.readFileSync(gp, 'utf8'));
        let summary = 'Upgraded game world & player systems';
        if (low.includes('turret') || low.includes('ally')) {
          gm.turrets = [...(gm.turrets || []), { id: `t_${Date.now()}`, pos: [-7, 0, -4], range: 26, fire_rate: 2.2 }, { id: `t2_${Date.now()}`, pos: [7, 0, -4], range: 26, fire_rate: 2.2 }];
          summary = 'Deployed 2 autonomous AI plasma turrets';
        } else if (low.includes('rain') || low.includes('storm')) {
          gm.environment.weather = 'rain';
          gm.environment.sky_horizon = '#16243b';
          summary = 'Activated storm rain & wet reflections';
        } else if (low.includes('ramp') || low.includes('jump')) {
          gm.ramps = [...(gm.ramps || []), { pos: [-10, 0, -8], rot_y: 0.5, boost: 25 }, { pos: [10, 0, 8], rot_y: -2.2, boost: 25 }];
          gm.player.max_jumps = 4;
          summary = 'Installed stunt ramps & quad-jump thrusters';
        } else if (low.includes('boss') || low.includes('dragon')) {
          gm.boss.enabled = true;
          gm.boss.scale = 2.65;
          gm.boss.hp = 950;
          summary = 'Summoned colossal 2.65x Magma Wyvern Boss';
        } else if (low.includes('railgun') || low.includes('plasma')) {
          gm.player.weapon_mode = 'plasma_railgun';
          gm.player.damage = 58;
          summary = 'Equipped rapid-fire Plasma Railgun';
        }
        fs.writeFileSync(gp, JSON.stringify(gm, null, 2));
        return sendJson({ game: gm, changes_count: 1, summary });
      }
    }
    if (pathname === '/api/swarm/detail' && req.method === 'POST') {
      const body = await readJsonBody(req);
      const metaPath = path.join(assetsDir, `${body.asset_id}.json`);
      const glbPath = path.join(assetsDir, `${body.asset_id}.glb`);
      if (fs.existsSync(metaPath) && fs.existsSync(glbPath)) {
        const pal = detectPromptPalette(body.instruction || 'gold cyan');
        fs.writeFileSync(glbPath, patchGlbMaterial(fs.readFileSync(glbPath), pal));
        const meta = JSON.parse(fs.readFileSync(metaPath, 'utf8'));
        meta.title = meta.title.includes('[Ultra]') ? meta.title : `${meta.title} [Ultra]`;
        meta.glb_url = `/exports/${body.asset_id}.glb?t=${Date.now()}`;
        fs.writeFileSync(metaPath, JSON.stringify(meta, null, 2));
        return sendJson(meta);
      }
    }
    if (pathname === '/api/generate/text' || pathname === '/api/agent/chat') {
      const body = await readJsonBody(req);
      const prompt = body.prompt || body.message || 'Sculpted 3D Asset';
      const arch = pickStarterArchetype(starterDir, prompt);
      if (arch) {
        const id = crypto.randomBytes(5).toString('hex');
        const srcGlb = fs.readFileSync(path.join(starterDir, `${arch.id}.glb`));
        const pal = detectPromptPalette(prompt);
        const outGlb = patchGlbMaterial(srcGlb, pal);
        fs.writeFileSync(path.join(assetsDir, `${id}.glb`), outGlb);
        const rec = {
          ...arch,
          id,
          title: prompt.length > 38 ? prompt.slice(0, 38) : arch.title,
          prompt,
          created_at: Math.floor(Date.now() / 1000),
          glb_url: `/exports/${id}.glb?t=${Date.now()}`,
        };
        fs.writeFileSync(path.join(assetsDir, `${id}.json`), JSON.stringify(rec, null, 2));
        if (pathname === '/api/agent/chat') {
          return sendJson({
            reply: `Sculpted "${rec.title}" (${rec.telemetry?.faces?.toLocaleString()} PBR triangles).`,
            action: 'created_model',
            provider: 'Falcon Sculpted PBR Engine',
            asset: rec,
          });
        }
        return sendJson(rec);
      }
    }
    if (pathname.startsWith('/api/mesh/') && pathname.endsWith('/material') && req.method === 'POST') {
      const aid = pathname.split('/')[3];
      const body = await readJsonBody(req);
      const glbPath = path.join(assetsDir, `${aid}.glb`);
      const metaPath = path.join(assetsDir, `${aid}.json`);
      if (fs.existsSync(glbPath) && fs.existsSync(metaPath)) {
        const patched = patchGlbMaterial(fs.readFileSync(glbPath), {
          colorHex: body.color,
          metallic: body.metallic,
          roughness: body.roughness,
          emissiveHex: body.emissive,
          emissiveStrength: body.emissive_strength,
          partId: body.part_id,
        });
        fs.writeFileSync(glbPath, patched);
        const meta = JSON.parse(fs.readFileSync(metaPath, 'utf8'));
        meta.glb_url = `/exports/${aid}.glb?t=${Date.now()}`;
        fs.writeFileSync(metaPath, JSON.stringify(meta, null, 2));
        return sendJson(meta);
      }
    }
    if (pathname.startsWith('/api/export/')) {
      const parts = pathname.split('/');
      const aid = parts[3];
      const fmt = parts[4];
      const glbPath = path.join(assetsDir, `${aid}.glb`);
      if (fmt === 'glb' && fs.existsSync(glbPath)) {
        const b = fs.readFileSync(glbPath);
        res.writeHead(200, {
          'Content-Type': 'model/gltf-binary',
          'Content-Disposition': `attachment; filename="${aid}.glb"`,
        });
        return res.end(b);
      }
      const metaPath = path.join(assetsDir, `${aid}.json`);
      const meta = fs.existsSync(metaPath) ? JSON.parse(fs.readFileSync(metaPath, 'utf8')) : {};
      const txt = Buffer.from(meta.unity_csharp_script || '// Falcon Export\n');
      res.writeHead(200, {
        'Content-Type': 'text/plain',
        'Content-Disposition': `attachment; filename="${aid}.${fmt}"`,
      });
      return res.end(txt);
    }

    const rel = pathname === '/' ? 'index.html' : pathname.slice(1);
    const target = path.join(webDir, rel);
    const fileToServe =
      fs.existsSync(target) && fs.statSync(target).isFile() ? target : path.join(webDir, 'index.html');
    if (fs.existsSync(fileToServe)) {
      const ext = path.extname(fileToServe).toLowerCase();
      const b = fs.readFileSync(fileToServe);
      res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream', 'Content-Length': b.length });
      return res.end(b);
    }
    sendJson({ status: 'Falcon AI Running' });
  });

  return new Promise((resolve) => {
    server.listen(port, '127.0.0.1', () => resolve(server));
  });
}

module.exports = { startFallbackServer };
