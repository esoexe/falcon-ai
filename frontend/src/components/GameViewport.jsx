import React, { useEffect, useRef, useState } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { RoomEnvironment } from 'three/examples/jsm/environments/RoomEnvironment.js';

// Zero-dependency offline WebAudio synthesizer for crisp game SFX
function playSynthSfx(kind, enabled) {
  if (!enabled || typeof window === 'undefined') return;
  try {
    const Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx) return;
    if (!window.__falconAudioCtx) window.__falconAudioCtx = new Ctx();
    const ctx = window.__falconAudioCtx;
    if (ctx.state === 'suspended') ctx.resume();

    const now = ctx.currentTime;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.connect(gain);
    gain.connect(ctx.destination);

    if (kind === 'slash') {
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(420, now);
      osc.frequency.exponentialRampToValueAtTime(95, now + 0.14);
      gain.gain.setValueAtTime(0.08, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.15);
      osc.start(now);
      osc.stop(now + 0.15);
    } else if (kind === 'laser') {
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(780, now);
      osc.frequency.exponentialRampToValueAtTime(180, now + 0.11);
      gain.gain.setValueAtTime(0.07, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.12);
      osc.start(now);
      osc.stop(now + 0.12);
    } else if (kind === 'ult') {
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(140, now);
      osc.frequency.exponentialRampToValueAtTime(620, now + 0.32);
      gain.gain.setValueAtTime(0.11, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.35);
      osc.start(now);
      osc.stop(now + 0.35);
    } else if (kind === 'pickup') {
      osc.type = 'sine';
      osc.frequency.setValueAtTime(520, now);
      osc.frequency.exponentialRampToValueAtTime(1040, now + 0.14);
      gain.gain.setValueAtTime(0.07, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.15);
      osc.start(now);
      osc.stop(now + 0.15);
    } else if (kind === 'hit') {
      osc.type = 'square';
      osc.frequency.setValueAtTime(160, now);
      osc.frequency.exponentialRampToValueAtTime(55, now + 0.09);
      gain.gain.setValueAtTime(0.06, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.1);
      osc.start(now);
      osc.stop(now + 0.1);
    }
  } catch {
    // Ignore audio context restrictions
  }
}

export default function GameViewport({ game, onExportGameHtml, onExportGameCs, onQuickMutate }) {
  const mountRef = useRef(null);
  const radarRef = useRef(null);
  const [playMode, setPlayMode] = useState(true); // true = Playable 3rd-Person, false = Free Orbit World Inspector
  const [sfxOn, setSfxOn] = useState(true);
  const [hud, setHud] = useState({
    hp: 200,
    maxHp: 200,
    energy: 100,
    score: 0,
    wave: 1,
    kills: 0,
    bossHp: 650,
    bossMaxHp: 650,
    bossName: 'Apex Boss',
    bossAlive: true,
    speedKmh: 0,
    banner: '',
  });

  const playModeRef = useRef(playMode);
  const sfxRef = useRef(sfxOn);
  useEffect(() => {
    playModeRef.current = playMode;
  }, [playMode]);
  useEffect(() => {
    sfxRef.current = sfxOn;
  }, [sfxOn]);

  const actionsRef = useRef({ firePrimary: null, fireUlt: null, resetRun: null });

  useEffect(() => {
    const container = mountRef.current;
    if (!container || !game) return;

    const env = game.environment || {};
    const pCfg = game.player || {};
    const eCfg = game.enemies || {};
    const bCfg = game.boss || {};
    const arenaR = Number(game.arena_radius) || 34;

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(env.sky_horizon || '#1b1520');
    scene.fog = new THREE.FogExp2(env.sky_horizon || '#1b1520', Number(env.fog_density) || 0.013);

    const camera = new THREE.PerspectiveCamera(
      54,
      Math.max(1, container.clientWidth) / Math.max(1, container.clientHeight),
      0.1,
      320
    );
    camera.position.set(0, 8, 16);

    // Auto-detect Zero-GPU / Software WebGL (SwiftShader, llvmpipe, Basic Render) for smooth FPS
    let isSoftwareGL = false;
    try {
      const probeCanvas = document.createElement('canvas');
      const pgl = probeCanvas.getContext('webgl2') || probeCanvas.getContext('webgl');
      if (pgl) {
        const dbg = pgl.getExtension('WEBGL_debug_renderer_info');
        const rStr = dbg ? String(pgl.getParameter(dbg.UNMASKED_RENDERER_WEBGL)) : String(pgl.getParameter(pgl.RENDERER));
        isSoftwareGL = /swiftshader|llvmpipe|software|basic render|microsoft basic/i.test(rStr);
      }
    } catch {
      // ignore probe errors
    }

    const renderer = new THREE.WebGLRenderer({ antialias: !isSoftwareGL, powerPreference: 'high-performance' });
    renderer.setPixelRatio(isSoftwareGL ? 1 : Math.min(window.devicePixelRatio || 1, 2));
    renderer.setSize(container.clientWidth, container.clientHeight);
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.22;
    renderer.shadowMap.enabled = !isSoftwareGL;
    if (!isSoftwareGL) {
      renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    }
    container.innerHTML = '';
    container.appendChild(renderer.domElement);

    let pmrem = null;
    if (!isSoftwareGL) {
      pmrem = new THREE.PMREMGenerator(renderer);
      scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
      scene.environmentIntensity = 1.15;
    }

    const orbit = new OrbitControls(camera, renderer.domElement);
    orbit.enableDamping = true;
    orbit.enabled = !playModeRef.current;

    // Studio + Dramatic Arena Lighting
    const hemi = new THREE.HemisphereLight(0xffffff, 0x1e1b2e, isSoftwareGL ? 1.65 : 1.15);
    scene.add(hemi);

    const sun = new THREE.DirectionalLight(0xfff3e0, 2.7);
    sun.position.set(22, 36, 18);
    sun.castShadow = !isSoftwareGL;
    if (!isSoftwareGL) {
      sun.shadow.mapSize.set(2048, 2048);
    }
    sun.shadow.camera.near = 2;
    sun.shadow.camera.far = 110;
    const sBound = arenaR * 1.15;
    sun.shadow.camera.left = -sBound;
    sun.shadow.camera.right = sBound;
    sun.shadow.camera.top = sBound;
    sun.shadow.camera.bottom = -sBound;
    scene.add(sun);

    const accentHex = new THREE.Color(env.grid_accent || '#22d3ee');
    const centerLight = new THREE.PointLight(accentHex, 3.2, 32);
    centerLight.position.set(0, 4.5, 0);
    scene.add(centerLight);

    // Sculpted Arena Floor + Glowing Tactical Grid + Boundary Ring
    const groundMat = new THREE.MeshStandardMaterial({
      color: new THREE.Color(env.ground_color || '#1a181b'),
      roughness: env.weather === 'rain' ? 0.24 : 0.78,
      metalness: env.weather === 'rain' ? 0.55 : 0.16,
    });
    const ground = new THREE.Mesh(new THREE.CircleGeometry(arenaR * 1.65, 72), groundMat);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    scene.add(ground);

    const grid = new THREE.GridHelper(arenaR * 2.2, 48, accentHex, new THREE.Color(0x2d2729));
    grid.position.y = 0.02;
    grid.material.opacity = 0.48;
    grid.material.transparent = true;
    scene.add(grid);

    const ringMesh = new THREE.Mesh(
      new THREE.TorusGeometry(arenaR, 0.18, 12, 96),
      new THREE.MeshBasicMaterial({ color: accentHex })
    );
    ringMesh.rotation.x = Math.PI / 2;
    ringMesh.position.y = 0.15;
    scene.add(ringMesh);

    // Weather Particle System (650 particles)
    const pCount = 650;
    const pPos = new Float32Array(pCount * 3);
    for (let i = 0; i < pCount; i++) {
      pPos[i * 3 + 0] = (Math.random() - 0.5) * arenaR * 2.4;
      pPos[i * 3 + 1] = Math.random() * 26;
      pPos[i * 3 + 2] = (Math.random() - 0.5) * arenaR * 2.4;
    }
    const pGeo = new THREE.BufferGeometry();
    pGeo.setAttribute('position', new THREE.BufferAttribute(pPos, 3));
    const weatherColor =
      env.weather === 'embers'
        ? 0xf97316
        : env.weather === 'rain'
        ? 0x7dd3fc
        : env.weather === 'snow'
        ? 0xf8fafc
        : 0x22d3ee;
    const pMat = new THREE.PointsMaterial({
      color: weatherColor,
      size: env.weather === 'rain' ? 0.18 : 0.24,
      transparent: true,
      opacity: 0.78,
    });
    const weatherPts = new THREE.Points(pGeo, pMat);
    scene.add(weatherPts);

    // GLB Cache Loader with Auto-Normalization to Unit Height
    const loader = new GLTFLoader();
    const maxAniso = renderer.capabilities.getMaxAnisotropy();
    const glbCache = new Map();

    const loadNormalizedModel = (url, targetHeight = 2.4) => {
      if (!url) {
        return Promise.resolve(
          new THREE.Mesh(new THREE.OctahedronGeometry(0.9, 1), new THREE.MeshStandardMaterial({ color: accentHex }))
        );
      }
      if (glbCache.has(url)) {
        return Promise.resolve(glbCache.get(url).clone(true));
      }
      return new Promise((resolve) => {
        loader.load(
          url,
          (gltf) => {
            const wrapper = new THREE.Group();
            const raw = gltf.scene;
            raw.traverse((ch) => {
              if (ch.isMesh) {
                ch.castShadow = true;
                ch.receiveShadow = true;
                if (ch.geometry?.attributes?.uv && !ch.geometry.attributes.uv1) {
                  ch.geometry.setAttribute('uv1', ch.geometry.attributes.uv);
                }
                const m = ch.material;
                if (m) {
                  ['map', 'normalMap', 'roughnessMap', 'metalnessMap', 'aoMap'].forEach((k) => {
                    if (m[k]) m[k].anisotropy = maxAniso;
                  });
                  m.envMapIntensity = 1.25;
                }
              }
            });
            const box = new THREE.Box3().setFromObject(raw);
            if (!box.isEmpty()) {
              const sz = box.getSize(new THREE.Vector3());
              const ctr = box.getCenter(new THREE.Vector3());
              const maxD = Math.max(sz.x, sz.y, sz.z, 0.1);
              const s = targetHeight / maxD;
              raw.scale.setScalar(s);
              raw.position.set(-ctr.x * s, -box.min.y * s, -ctr.z * s);
            }
            wrapper.add(raw);
            glbCache.set(url, wrapper);
            resolve(wrapper.clone(true));
          },
          undefined,
          () => {
            const fb = new THREE.Mesh(
              new THREE.BoxGeometry(1.2, targetHeight, 1.2),
              new THREE.MeshStandardMaterial({ color: accentHex })
            );
            fb.position.y = targetHeight * 0.5;
            resolve(fb);
          }
        );
      });
    };

    // World Entities State
    const st = {
      playerGroup: new THREE.Group(),
      playerVel: new THREE.Vector3(),
      jumpsUsed: 0,
      hp: Number(pCfg.max_hp) || 200,
      maxHp: Number(pCfg.max_hp) || 200,
      energy: 100,
      score: 0,
      wave: 1,
      kills: 0,
      overdriveTimer: 0,
      attackCooldown: 0,
      banner: `${game.title} — WAVE 1`,
      bannerTimer: 2.8,
      landmarks: [],
      ramps: [],
      turrets: [],
      pickups: [],
      enemies: [],
      boss: null,
      projectiles: [],
      fxMeshes: [],
      keys: {},
    };

    st.playerGroup.position.set(0, 0, 8);
    scene.add(st.playerGroup);

    // Player Ground Ring Indicator
    const playerHalo = new THREE.Mesh(
      new THREE.RingGeometry(1.05, 1.24, 36),
      new THREE.MeshBasicMaterial({ color: 0x30d158, side: THREE.DoubleSide, transparent: true, opacity: 0.7 })
    );
    playerHalo.rotation.x = -Math.PI / 2;
    playerHalo.position.y = 0.04;
    st.playerGroup.add(playerHalo);

    const playerBaseH = game.genre === 'hypercar_racing' ? 2.8 : 2.5;
    loadNormalizedModel(pCfg.glb_url, playerBaseH * (Number(pCfg.scale) || 1)).then((m) => {
      st.playerGroup.add(m);
    });

    // Landmarks / Architecture Props
    (game.props || []).forEach((pr) => {
      const g = new THREE.Group();
      g.position.set(pr.pos?.[0] || 0, 0, pr.pos?.[2] || 0);
      g.rotation.y = Number(pr.rot_y) || 0;
      scene.add(g);
      st.landmarks.push({ group: g, radius: Number(pr.radius) || 2.8 });
      loadNormalizedModel(pr.glb_url, 4.4 * (Number(pr.scale) || 1.5)).then((m) => g.add(m));
    });

    // Aero Launch Ramps
    (game.ramps || []).forEach((rm) => {
      const rg = new THREE.Group();
      rg.position.set(rm.pos?.[0] || 0, 0, rm.pos?.[2] || 0);
      rg.rotation.y = Number(rm.rot_y) || 0;
      const wedge = new THREE.Mesh(
        new THREE.BoxGeometry(3.6, 0.35, 4.8),
        new THREE.MeshStandardMaterial({
          color: 0x1e293b,
          emissive: accentHex,
          emissiveIntensity: 0.45,
          metalness: 0.8,
          roughness: 0.25,
        })
      );
      wedge.position.y = 0.55;
      wedge.rotation.x = 0.26;
      wedge.castShadow = true;
      rg.add(wedge);
      scene.add(rg);
      st.ramps.push({ group: rg, boost: Number(rm.boost) || 22 });
    });

    // Autonomous Allied AI Defense Turrets
    (game.turrets || []).forEach((tu) => {
      const tg = new THREE.Group();
      tg.position.set(tu.pos?.[0] || 0, 0, tu.pos?.[2] || 0);
      const base = new THREE.Mesh(
        new THREE.CylinderGeometry(0.65, 0.85, 1.1, 12),
        new THREE.MeshStandardMaterial({ color: 0x334155, metalness: 0.85, roughness: 0.25 })
      );
      base.position.y = 0.55;
      const head = new THREE.Mesh(
        new THREE.BoxGeometry(0.85, 0.55, 1.35),
        new THREE.MeshStandardMaterial({
          color: 0xcbd5e1,
          emissive: 0x22d3ee,
          emissiveIntensity: 0.35,
          metalness: 0.9,
          roughness: 0.2,
        })
      );
      head.position.y = 1.35;
      tg.add(base, head);
      scene.add(tg);
      st.turrets.push({
        group: tg,
        head,
        range: Number(tu.range) || 25,
        cooldown: Math.random() * 0.5,
        fireInterval: 1.0 / Math.max(0.4, Number(tu.fire_rate) || 1.8),
      });
    });

    // Collectible Relics
    (game.pickups || []).forEach((pk) => {
      const pg = new THREE.Group();
      pg.position.set(pk.pos?.[0] || 0, 0.55, pk.pos?.[2] || 0);
      scene.add(pg);
      loadNormalizedModel(pk.glb_url, 1.35).then((m) => pg.add(m));
      st.pickups.push({
        group: pg,
        baseY: 0.55,
        active: true,
        respawnTimer: 0,
        value: Number(pk.value) || 100,
      });
    });

    // Spawn Enemies Helper
    const spawnEnemyWave = (waveNum) => {
      const count = (Number(eCfg.base_count) || 5) + (waveNum - 1) * 2;
      loadNormalizedModel(eCfg.glb_url, 2.35 * (Number(eCfg.scale) || 1)).then((proto) => {
        for (let i = 0; i < count; i++) {
          const ang = (Math.PI * 2 * i) / count + Math.random() * 0.35;
          const dist = arenaR * (0.58 + 0.25 * Math.random());
          const eg = new THREE.Group();
          eg.position.set(Math.cos(ang) * dist, 0, Math.sin(ang) * dist);
          eg.add(proto.clone(true));

          const ring = new THREE.Mesh(
            new THREE.RingGeometry(0.95, 1.12, 24),
            new THREE.MeshBasicMaterial({ color: 0xff3b30, side: THREE.DoubleSide })
          );
          ring.rotation.x = -Math.PI / 2;
          ring.position.y = 0.04;
          eg.add(ring);

          scene.add(eg);
          const maxHp = (Number(eCfg.hp) || 95) * (1 + (waveNum - 1) * 0.22);
          st.enemies.push({
            group: eg,
            hp: maxHp,
            maxHp,
            speed: (Number(eCfg.speed) || 5.4) * (0.92 + Math.random() * 0.2),
            damage: Number(eCfg.damage) || 14,
            hitFlash: 0,
            knockVel: new THREE.Vector3(),
            phase: Math.random() * 6.28,
          });
        }
      });
    };
    spawnEnemyWave(1);

    // Spawn Colossal Boss
    if (bCfg.enabled !== false) {
      const bg = new THREE.Group();
      bg.position.set(0, 0, -arenaR * 0.58);
      const bossRing = new THREE.Mesh(
        new THREE.RingGeometry(2.4, 2.75, 48),
        new THREE.MeshBasicMaterial({ color: 0xff9f0a, side: THREE.DoubleSide })
      );
      bossRing.rotation.x = -Math.PI / 2;
      bossRing.position.y = 0.05;
      bg.add(bossRing);
      scene.add(bg);

      const bMax = Number(bCfg.hp) || 650;
      st.boss = {
        group: bg,
        name: bCfg.name || 'Apex Wyvern Boss',
        hp: bMax,
        maxHp: bMax,
        speed: Number(bCfg.speed) || 4.2,
        damage: Number(bCfg.damage) || 28,
        alive: true,
        chargeTimer: 4.0,
        knockVel: new THREE.Vector3(),
      };
      loadNormalizedModel(bCfg.glb_url, 2.6 * (Number(bCfg.scale) || 2.1)).then((m) => bg.add(m));
    }

    // FX Spawner (Expanding Shockwave Rings & Slash Arcs)
    const spawnShockRing = (pos, colorHex, maxScale = 6.5, duration = 0.35) => {
      const mesh = new THREE.Mesh(
        new THREE.RingGeometry(0.45, 0.95, 36),
        new THREE.MeshBasicMaterial({ color: colorHex, side: THREE.DoubleSide, transparent: true, opacity: 0.92 })
      );
      mesh.rotation.x = -Math.PI / 2;
      mesh.position.copy(pos);
      mesh.position.y += 0.35;
      scene.add(mesh);
      st.fxMeshes.push({ mesh, age: 0, duration, maxScale });
    };

    const damageTarget = (ent, dmg, pushDir, isBoss = false) => {
      ent.hp -= dmg;
      if (pushDir) ent.knockVel.addScaledVector(pushDir, isBoss ? 4.5 : 11.0);
      playSynthSfx('hit', sfxRef.current);
      spawnShockRing(ent.group.position, isBoss ? 0xff9f0a : 0x38bdf8, isBoss ? 3.4 : 2.1, 0.22);

      if (ent.hp <= 0) {
        spawnShockRing(ent.group.position, 0xff3b30, isBoss ? 11.0 : 4.5, 0.45);
        if (isBoss) {
          ent.alive = false;
          ent.group.visible = false;
          st.score += 2500;
          st.kills += 1;
          st.banner = `BOSS VANQUISHED (+2500 PTS) — NEXT WAVE INCOMING`;
          st.bannerTimer = 3.5;
        } else {
          scene.remove(ent.group);
          st.score += 150;
          st.kills += 1;
          st.energy = Math.min(100, st.energy + 14);
        }
      }
    };

    // Primary Attack (Damascus Melee Arc OR Plasma Railgun / Twin Lasers)
    const firePrimary = () => {
      if (st.attackCooldown > 0) return;
      const wMode = pCfg.weapon_mode || 'damascus_slash';
      const baseDmg = (Number(pCfg.damage) || 38) * (st.overdriveTimer > 0 ? 1.65 : 1.0);
      const forward = new THREE.Vector3(0, 0, 1).applyQuaternion(st.playerGroup.quaternion).normalize();

      if (wMode === 'damascus_slash' || wMode === 'pulse_emp') {
        st.attackCooldown = 0.24;
        playSynthSfx('slash', sfxRef.current);
        const arcCenter = st.playerGroup.position.clone().addScaledVector(forward, 2.2);
        spawnShockRing(arcCenter, accentHex, wMode === 'pulse_emp' ? 6.5 : 4.6, 0.26);

        const hitRadius = wMode === 'pulse_emp' ? 6.2 : 4.8;
        for (let i = st.enemies.length - 1; i >= 0; i--) {
          const e = st.enemies[i];
          if (e.group.position.distanceTo(arcCenter) <= hitRadius) {
            const push = e.group.position.clone().sub(st.playerGroup.position).setY(0).normalize();
            damageTarget(e, baseDmg, push, false);
            if (e.hp <= 0) st.enemies.splice(i, 1);
          }
        }
        if (st.boss && st.boss.alive && st.boss.group.position.distanceTo(arcCenter) <= hitRadius + 1.8) {
          const push = st.boss.group.position.clone().sub(st.playerGroup.position).setY(0).normalize();
          damageTarget(st.boss, baseDmg, push, true);
        }
      } else {
        st.attackCooldown = 0.14;
        playSynthSfx('laser', sfxRef.current);
        const offsets = wMode === 'twin_lasers' ? [-0.65, 0.65] : [0];
        offsets.forEach((ox) => {
          const right = new THREE.Vector3(1, 0, 0).applyQuaternion(st.playerGroup.quaternion);
          const bolt = new THREE.Mesh(
            new THREE.SphereGeometry(0.24, 10, 10),
            new THREE.MeshBasicMaterial({ color: accentHex })
          );
          bolt.position
            .copy(st.playerGroup.position)
            .add(new THREE.Vector3(0, 1.35, 0))
            .addScaledVector(right, ox)
            .addScaledVector(forward, 1.4);
          scene.add(bolt);
          st.projectiles.push({
            mesh: bolt,
            vel: forward.clone().multiplyScalar(44),
            dmg: baseDmg * (offsets.length > 1 ? 0.65 : 1.0),
            life: 1.5,
            homing: false,
          });
        });
      }
    };

    // Ultimate Ability ([E])
    const fireUlt = () => {
      if (st.energy < 45) return;
      st.energy = Math.max(0, st.energy - 50);
      playSynthSfx('ult', sfxRef.current);
      const uMode = pCfg.ultimate_mode || 'nova_shockwave';

      if (uMode === 'missile_barrage') {
        st.banner = 'ULTIMATE: HOMING PLASMA MISSILE SALVO!';
        st.bannerTimer = 1.8;
        for (let m = 0; m < 8; m++) {
          const ang = (Math.PI * 2 * m) / 8;
          const orb = new THREE.Mesh(
            new THREE.SphereGeometry(0.32, 12, 12),
            new THREE.MeshBasicMaterial({ color: 0xff9f0a })
          );
          orb.position.copy(st.playerGroup.position).add(new THREE.Vector3(0, 2.2, 0));
          scene.add(orb);
          st.projectiles.push({
            mesh: orb,
            vel: new THREE.Vector3(Math.cos(ang) * 14, 6, Math.sin(ang) * 14),
            dmg: (Number(pCfg.damage) || 38) * 1.55,
            life: 2.6,
            homing: true,
          });
        }
      } else if (uMode === 'overdrive_boost') {
        st.overdriveTimer = 5.5;
        st.banner = 'ULTIMATE: NITRO OVERDRIVE ENGAGED!';
        st.bannerTimer = 2.0;
        spawnShockRing(st.playerGroup.position, 0x38bdf8, 9.0, 0.45);
      } else {
        st.banner = 'ULTIMATE: SUPERNOVA SHOCKWAVE!';
        st.bannerTimer = 1.8;
        spawnShockRing(st.playerGroup.position, 0xf97316, 15.0, 0.5);
        for (let i = st.enemies.length - 1; i >= 0; i--) {
          const e = st.enemies[i];
          if (e.group.position.distanceTo(st.playerGroup.position) <= 15.5) {
            const push = e.group.position.clone().sub(st.playerGroup.position).setY(0).normalize();
            damageTarget(e, (Number(pCfg.damage) || 38) * 2.8, push, false);
            if (e.hp <= 0) st.enemies.splice(i, 1);
          }
        }
        if (st.boss && st.boss.alive && st.boss.group.position.distanceTo(st.playerGroup.position) <= 17.5) {
          const push = st.boss.group.position.clone().sub(st.playerGroup.position).setY(0).normalize();
          damageTarget(st.boss, (Number(pCfg.damage) || 38) * 2.6, push, true);
        }
      }
    };

    const resetRun = () => {
      st.hp = st.maxHp;
      st.energy = 100;
      st.playerGroup.position.set(0, 0, 8);
      st.playerVel.set(0, 0, 0);
      st.banner = 'SYSTEMS RESTORED — READY';
      st.bannerTimer = 2.0;
    };

    actionsRef.current = { firePrimary, fireUlt, resetRun };

    // Input Listeners (Ignored when typing inside <input> or <textarea>!)
    const isTypingInInput = (el) =>
      el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA' || el.tagName === 'SELECT' || el.isContentEditable);

    const onKeyDown = (e) => {
      if (isTypingInInput(document.activeElement)) return;
      st.keys[e.code] = true;
      if (e.code === 'Space' && playModeRef.current) {
        e.preventDefault();
        const maxJumps = Number(pCfg.max_jumps) || 2;
        if (st.jumpsUsed < maxJumps) {
          st.playerVel.y = Number(pCfg.jump_force) || 11.5;
          st.jumpsUsed += 1;
          spawnShockRing(st.playerGroup.position, 0x30d158, 2.2, 0.2);
        }
      } else if (e.code === 'KeyF') {
        firePrimary();
      } else if (e.code === 'KeyE') {
        fireUlt();
      }
    };
    const onKeyUp = (e) => {
      st.keys[e.code] = false;
    };
    const onPointerDown = (e) => {
      if (!playModeRef.current) return;
      if (e.button === 2) fireUlt();
      else firePrimary();
    };
    const onContext = (e) => {
      if (playModeRef.current) e.preventDefault();
    };

    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    renderer.domElement.addEventListener('pointerdown', onPointerDown);
    renderer.domElement.addEventListener('contextmenu', onContext);

    const ro = new ResizeObserver(() => {
      if (!container) return;
      const w = Math.max(1, container.clientWidth);
      const h = Math.max(1, container.clientHeight);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    });
    ro.observe(container);

    // 60 FPS Game Loop
    let rafId = 0;
    let hudTick = 0;
    const clock = new THREE.Clock();

    const animate = () => {
      rafId = requestAnimationFrame(animate);
      const dt = Math.min(0.05, clock.getDelta());
      const elapsed = clock.elapsedTime;
      orbit.enabled = !playModeRef.current;

      if (playModeRef.current) {
        st.attackCooldown = Math.max(0, st.attackCooldown - dt);
        st.overdriveTimer = Math.max(0, st.overdriveTimer - dt);
        st.bannerTimer = Math.max(0, st.bannerTimer - dt);
        st.energy = Math.min(100, st.energy + 7.5 * dt);

        // 1. Player Movement / Hypercar Drift / Starfighter Flight
        const ix = (st.keys['KeyD'] || st.keys['ArrowRight'] ? 1 : 0) - (st.keys['KeyA'] || st.keys['ArrowLeft'] ? 1 : 0);
        const iz = (st.keys['KeyS'] || st.keys['ArrowDown'] ? 1 : 0) - (st.keys['KeyW'] || st.keys['ArrowUp'] ? 1 : 0);
        const sprinting = Boolean(st.keys['ShiftLeft'] || st.keys['ShiftRight'] || st.overdriveTimer > 0);
        const baseSpd = (Number(pCfg.speed) || 11.0) * (sprinting ? Number(pCfg.sprint_mult) || 1.75 : 1.0);

        if (sprinting && (ix !== 0 || iz !== 0)) {
          st.energy = Math.max(0, st.energy - 11 * dt);
        }

        if (game.genre === 'hypercar_racing') {
          const throttle = -iz;
          const steer = -ix;
          const curForward = new THREE.Vector3(0, 0, 1).applyQuaternion(st.playerGroup.quaternion);
          const curSpd = st.playerVel.x * curForward.x + st.playerVel.z * curForward.z;
          if (Math.abs(steer) > 0.01) {
            st.playerGroup.rotation.y += steer * 2.5 * dt * (Math.abs(curSpd) > 1 ? Math.sign(curSpd) || 1 : 1);
          }
          const nextForward = new THREE.Vector3(0, 0, 1).applyQuaternion(st.playerGroup.quaternion);
          st.playerVel.x = THREE.MathUtils.lerp(st.playerVel.x, nextForward.x * throttle * baseSpd, 4.5 * dt);
          st.playerVel.z = THREE.MathUtils.lerp(st.playerVel.z, nextForward.z * throttle * baseSpd, 4.5 * dt);
        } else {
          const moveDir = new THREE.Vector3(ix, 0, iz);
          if (moveDir.lengthSq() > 0.01) {
            moveDir.normalize();
            st.playerVel.x = THREE.MathUtils.lerp(st.playerVel.x, moveDir.x * baseSpd, 10 * dt);
            st.playerVel.z = THREE.MathUtils.lerp(st.playerVel.z, moveDir.z * baseSpd, 10 * dt);
            const targetYaw = Math.atan2(moveDir.x, moveDir.z);
            let diff = targetYaw - st.playerGroup.rotation.y;
            while (diff > Math.PI) diff -= Math.PI * 2;
            while (diff < -Math.PI) diff += Math.PI * 2;
            st.playerGroup.rotation.y += diff * Math.min(1, 14 * dt);
          } else {
            st.playerVel.x *= Math.max(0, 1 - 10 * dt);
            st.playerVel.z *= Math.max(0, 1 - 10 * dt);
          }
        }

        const floorY = game.genre === 'space_dogfight' ? 1.8 : 0.0;
        st.playerVel.y -= 26.0 * dt;
        st.playerGroup.position.addScaledVector(st.playerVel, dt);
        if (st.playerGroup.position.y <= floorY) {
          st.playerGroup.position.y = floorY;
          st.playerVel.y = 0;
          st.jumpsUsed = 0;
        }

        // Clamp Arena Boundary & Landmark Colliders
        const p2d = Math.hypot(st.playerGroup.position.x, st.playerGroup.position.z);
        if (p2d > arenaR - 1.2) {
          const s = (arenaR - 1.2) / p2d;
          st.playerGroup.position.x *= s;
          st.playerGroup.position.z *= s;
        }
        st.landmarks.forEach((lm) => {
          const dx = st.playerGroup.position.x - lm.group.position.x;
          const dz = st.playerGroup.position.z - lm.group.position.z;
          const d = Math.hypot(dx, dz);
          const minR = lm.radius + 0.9;
          if (d < minR && d > 0.001 && st.playerGroup.position.y < 3.6) {
            st.playerGroup.position.x = lm.group.position.x + (dx / d) * minR;
            st.playerGroup.position.z = lm.group.position.z + (dz / d) * minR;
          }
        });

        // Aero Launch Ramps
        st.ramps.forEach((rm) => {
          if (st.playerGroup.position.distanceTo(rm.group.position) < 2.8 && st.playerGroup.position.y < 1.8) {
            st.playerVel.y = rm.boost * 0.72;
            const fwd = new THREE.Vector3(0, 0, 1).applyQuaternion(st.playerGroup.quaternion);
            st.playerVel.x += fwd.x * 12;
            st.playerVel.z += fwd.z * 12;
            playSynthSfx('ult', sfxRef.current);
          }
        });

        // Collectibles
        st.pickups.forEach((pk, idx) => {
          if (!pk.active) {
            pk.respawnTimer -= dt;
            if (pk.respawnTimer <= 0) {
              pk.active = true;
              pk.group.visible = true;
            }
            return;
          }
          pk.group.rotation.y += 2.2 * dt;
          pk.group.position.y = pk.baseY + Math.sin(elapsed * 3.0 + idx) * 0.22;
          if (pk.group.position.distanceTo(st.playerGroup.position) < 2.1) {
            pk.active = false;
            pk.group.visible = false;
            pk.respawnTimer = 9.0;
            st.score += pk.value;
            st.hp = Math.min(st.maxHp, st.hp + 25);
            st.energy = Math.min(100, st.energy + 30);
            playSynthSfx('pickup', sfxRef.current);
            spawnShockRing(pk.group.position, 0x30d158, 3.0, 0.28);
          }
        });

        // Autonomous Allied AI Turrets
        st.turrets.forEach((tu) => {
          tu.cooldown -= dt;
          let nearest = null;
          let bestD = tu.range;
          st.enemies.forEach((e) => {
            const d = e.group.position.distanceTo(tu.group.position);
            if (d < bestD) {
              bestD = d;
              nearest = e.group;
            }
          });
          if (!nearest && st.boss && st.boss.alive) {
            const d = st.boss.group.position.distanceTo(tu.group.position);
            if (d < bestD) nearest = st.boss.group;
          }
          if (nearest) {
            const dir = nearest.position.clone().sub(tu.group.position);
            tu.head.rotation.y = Math.atan2(dir.x, dir.z);
            if (tu.cooldown <= 0) {
              tu.cooldown = tu.fireInterval;
              const bolt = new THREE.Mesh(
                new THREE.SphereGeometry(0.2, 8, 8),
                new THREE.MeshBasicMaterial({ color: 0x22d3ee })
              );
              bolt.position.copy(tu.group.position).add(new THREE.Vector3(0, 1.4, 0));
              scene.add(bolt);
              st.projectiles.push({
                mesh: bolt,
                vel: dir.normalize().multiplyScalar(38),
                dmg: 24,
                life: 1.2,
                homing: false,
              });
            }
          }
        });

        // Projectiles & Homing Missiles
        for (let i = st.projectiles.length - 1; i >= 0; i--) {
          const pr = st.projectiles[i];
          if (pr.homing) {
            const target = st.enemies[0]?.group || (st.boss?.alive ? st.boss.group : null);
            if (target) {
              const desired = target.position.clone().add(new THREE.Vector3(0, 1.2, 0)).sub(pr.mesh.position).normalize().multiplyScalar(32);
              pr.vel.lerp(desired, 6 * dt);
            }
          }
          pr.mesh.position.addScaledVector(pr.vel, dt);
          pr.life -= dt;

          let consumed = false;
          for (let j = st.enemies.length - 1; j >= 0; j--) {
            const e = st.enemies[j];
            if (e.group.position.clone().add(new THREE.Vector3(0, 1.1, 0)).distanceTo(pr.mesh.position) < 1.85) {
              damageTarget(e, pr.dmg, pr.vel.clone().setY(0).normalize(), false);
              if (e.hp <= 0) st.enemies.splice(j, 1);
              consumed = true;
              break;
            }
          }
          if (!consumed && st.boss && st.boss.alive) {
            if (st.boss.group.position.clone().add(new THREE.Vector3(0, 2.0, 0)).distanceTo(pr.mesh.position) < 3.2) {
              damageTarget(st.boss, pr.dmg, pr.vel.clone().setY(0).normalize(), true);
              consumed = true;
            }
          }
          if (consumed || pr.life <= 0) {
            scene.remove(pr.mesh);
            st.projectiles.splice(i, 1);
          }
        }

        // Enemy AI Chase & Escalating Wave Spawner
        if (st.enemies.length === 0) {
          st.wave += 1;
          st.banner = `WAVE ${st.wave} DEPLOYED — REINFORCEMENTS INBOUND`;
          st.bannerTimer = 2.6;
          spawnEnemyWave(st.wave);
        }

        st.enemies.forEach((e) => {
          e.group.position.addScaledVector(e.knockVel, dt);
          e.knockVel.multiplyScalar(Math.max(0, 1 - 8 * dt));
          const toPlayer = st.playerGroup.position.clone().sub(e.group.position);
          toPlayer.y = 0;
          const dist = toPlayer.length();
          if (dist > 1.75) {
            toPlayer.normalize();
            // Subtle flank weave
            const perp = new THREE.Vector3(-toPlayer.z, 0, toPlayer.x).multiplyScalar(Math.sin(elapsed * 2.2 + e.phase) * 0.35);
            const move = toPlayer.add(perp).normalize();
            e.group.position.addScaledVector(move, e.speed * dt);
            e.group.rotation.y = Math.atan2(move.x, move.z);
          } else if (st.overdriveTimer <= 0) {
            st.hp = Math.max(0, st.hp - e.damage * dt);
            if (st.hp <= 0) resetRun();
          }
        });

        // Colossal Boss AI
        if (st.boss && st.boss.alive) {
          const b = st.boss;
          b.group.position.addScaledVector(b.knockVel, dt);
          b.knockVel.multiplyScalar(Math.max(0, 1 - 6 * dt));
          b.chargeTimer -= dt;
          const toP = st.playerGroup.position.clone().sub(b.group.position).setY(0);
          const d = toP.length();
          const bSpd = b.chargeTimer < 0.8 ? b.speed * 2.4 : b.speed;
          if (b.chargeTimer <= 0) b.chargeTimer = 5.0;
          if (d > 3.1) {
            toP.normalize();
            b.group.position.addScaledVector(toP, bSpd * dt);
            b.group.rotation.y = Math.atan2(toP.x, toP.z);
          } else if (st.overdriveTimer <= 0) {
            st.hp = Math.max(0, st.hp - b.damage * dt);
            if (st.hp <= 0) resetRun();
          }
        }

        // Smooth 3rd-Person Chase Camera
        const back = new THREE.Vector3(0, 0, -1).applyQuaternion(st.playerGroup.quaternion);
        const camOffset =
          game.genre === 'hypercar_racing'
            ? back.clone().multiplyScalar(9.5).add(new THREE.Vector3(0, 4.4, 0))
            : new THREE.Vector3(0, 8.2, 13.8);
        const desiredCam = st.playerGroup.position.clone().add(camOffset);
        camera.position.lerp(desiredCam, Math.min(1, 7.5 * dt));
        camera.lookAt(st.playerGroup.position.clone().add(new THREE.Vector3(0, 1.6, 0)));
      } else {
        orbit.update();
      }

      // Animate FX Rings & Weather Particles
      for (let i = st.fxMeshes.length - 1; i >= 0; i--) {
        const fx = st.fxMeshes[i];
        fx.age += dt;
        const u = fx.age / fx.duration;
        if (u >= 1) {
          scene.remove(fx.mesh);
          st.fxMeshes.splice(i, 1);
        } else {
          fx.mesh.scale.setScalar(1 + u * fx.maxScale);
          fx.mesh.material.opacity = 0.9 * (1 - u);
        }
      }

      const arr = pGeo.attributes.position.array;
      const wy = env.weather === 'embers' ? 3.6 : env.weather === 'rain' ? -18.0 : -2.6;
      for (let i = 0; i < pCount; i++) {
        arr[i * 3 + 1] += wy * dt;
        if (arr[i * 3 + 1] > 26) arr[i * 3 + 1] = 0.2;
        if (arr[i * 3 + 1] < 0.1) arr[i * 3 + 1] = 25.5;
      }
      pGeo.attributes.position.needsUpdate = true;

      // Update Tactical Radar Minimap Canvas & React HUD (throttled ~12Hz for React state)
      if (radarRef.current) {
        const rc = radarRef.current.getContext('2d');
        const rw = radarRef.current.width;
        rc.clearRect(0, 0, rw, rw);
        rc.fillStyle = 'rgba(14, 12, 12, 0.88)';
        rc.fillRect(0, 0, rw, rw);
        rc.strokeStyle = 'rgba(255,255,255,0.18)';
        rc.beginPath();
        rc.arc(rw / 2, rw / 2, rw / 2 - 4, 0, Math.PI * 2);
        rc.stroke();

        const mapScale = (rw * 0.44) / arenaR;
        const plot = (x, z, col, r = 2.5) => {
          rc.fillStyle = col;
          rc.beginPath();
          rc.arc(rw / 2 + x * mapScale, rw / 2 + z * mapScale, r, 0, Math.PI * 2);
          rc.fill();
        };
        st.pickups.forEach((pk) => pk.active && plot(pk.group.position.x, pk.group.position.z, '#30d158', 2));
        st.turrets.forEach((tu) => plot(tu.group.position.x, tu.group.position.z, '#22d3ee', 2.8));
        st.enemies.forEach((e) => plot(e.group.position.x, e.group.position.z, '#ff3b30', 2.6));
        if (st.boss && st.boss.alive) plot(st.boss.group.position.x, st.boss.group.position.z, '#ff9f0a', 4.5);
        plot(st.playerGroup.position.x, st.playerGroup.position.z, '#fdfcfc', 3.5);
      }

      hudTick += dt;
      if (hudTick > 0.08) {
        hudTick = 0;
        const spdKmh = Math.round(Math.hypot(st.playerVel.x, st.playerVel.z) * 6.2);
        setHud({
          hp: Math.round(st.hp),
          maxHp: st.maxHp,
          energy: Math.round(st.energy),
          score: st.score,
          wave: st.wave,
          kills: st.kills,
          bossHp: st.boss ? Math.max(0, Math.round(st.boss.hp)) : 0,
          bossMaxHp: st.boss ? st.boss.maxHp : 1,
          bossName: st.boss ? st.boss.name : '',
          bossAlive: Boolean(st.boss && st.boss.alive),
          speedKmh: spdKmh,
          banner: st.bannerTimer > 0 ? st.banner : '',
        });
      }

      renderer.render(scene, camera);
    };
    animate();

    return () => {
      cancelAnimationFrame(rafId);
      ro.disconnect();
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      renderer.domElement.removeEventListener('pointerdown', onPointerDown);
      renderer.domElement.removeEventListener('contextmenu', onContext);
      if (pmrem) pmrem.dispose();
      renderer.dispose();
    };
  }, [game]);

  if (!game) return null;

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden' }}>
      <div ref={mountRef} style={{ width: '100%', height: '100%' }} />

      {/* OpenCode Monospace Game HUD Overlay */}
      <div
        style={{
          position: 'absolute',
          inset: 0,
          pointerEvents: 'none',
          padding: 12,
          display: 'flex',
          flexDirection: 'column',
          justifyContent: 'space-between',
        }}
      >
        {/* Top Row: Player Vitals | Boss Health & Wave | Tactical Radar & Controls */}
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', gap: 10 }}>
          {/* Player Status Box */}
          <div
            style={{
              pointerEvents: 'auto',
              background: 'rgba(19, 16, 16, 0.90)',
              border: '1px solid var(--hairline-strong)',
              borderRadius: 4,
              padding: '8px 10px',
              minWidth: 225,
            }}
          >
            <div style={{ fontSize: 11, fontWeight: 600, display: 'flex', justifyContent: 'space-between' }}>
              <span>[HERO] {game.player?.name?.slice(0, 22)}</span>
              <span style={{ color: 'var(--dot-green)' }}>
                {hud.hp}/{hud.maxHp}
              </span>
            </div>
            <div style={{ height: 6, background: '#262121', marginTop: 5, borderRadius: 2 }}>
              <div
                style={{
                  height: '100%',
                  width: `${Math.max(0, Math.min(100, (hud.hp / Math.max(1, hud.maxHp)) * 100))}%`,
                  background: '#30d158',
                  transition: 'width 0.1s',
                }}
              />
            </div>

            <div style={{ fontSize: 10, color: 'var(--ink-muted)', marginTop: 6, display: 'flex', justifyContent: 'space-between' }}>
              <span>ENERGY / BOOST</span>
              <span>{hud.energy}%</span>
            </div>
            <div style={{ height: 5, background: '#262121', marginTop: 3, borderRadius: 2 }}>
              <div
                style={{
                  height: '100%',
                  width: `${hud.energy}%`,
                  background: '#007aff',
                  transition: 'width 0.1s',
                }}
              />
            </div>

            <div style={{ marginTop: 7, display: 'flex', gap: 5 }}>
              <button
                className="oc-btn oc-btn-sm"
                onClick={() => actionsRef.current.firePrimary?.()}
                title="Primary Attack (Left Click or F)"
              >
                [attack: F]
              </button>
              <button
                className="oc-btn oc-btn-sm"
                onClick={() => actionsRef.current.fireUlt?.()}
                disabled={hud.energy < 45}
                title="Ultimate Ability (E or Right Click)"
              >
                [ult: E]
              </button>
            </div>
          </div>

          {/* Center Boss Bar & Score Telemetry */}
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 6 }}>
            <div
              style={{
                background: 'rgba(19, 16, 16, 0.90)',
                border: '1px solid var(--hairline-strong)',
                borderRadius: 4,
                padding: '6px 14px',
                fontSize: 11.5,
                display: 'flex',
                gap: 14,
              }}
            >
              <span>
                WAVE <strong style={{ color: '#ff9f0a' }}>{hud.wave}</strong>
              </span>
              <span>
                SCORE <strong style={{ color: '#30d158' }}>{hud.score.toLocaleString()}</strong>
              </span>
              <span>KILLS {hud.kills}</span>
              {game.genre === 'hypercar_racing' && <span>{hud.speedKmh} KM/H</span>}
            </div>

            {hud.bossAlive && (
              <div
                style={{
                  background: 'rgba(19, 16, 16, 0.92)',
                  border: '1px solid rgba(255, 59, 48, 0.5)',
                  borderRadius: 4,
                  padding: '5px 12px',
                  width: 300,
                }}
              >
                <div style={{ fontSize: 10.5, display: 'flex', justifyContent: 'space-between', color: '#ff3b30' }}>
                  <span>[BOSS] {hud.bossName.toUpperCase()}</span>
                  <span>
                    {hud.bossHp}/{hud.bossMaxHp}
                  </span>
                </div>
                <div style={{ height: 6, background: '#2a1a1a', marginTop: 4, borderRadius: 2 }}>
                  <div
                    style={{
                      height: '100%',
                      width: `${Math.max(0, (hud.bossHp / Math.max(1, hud.bossMaxHp)) * 100)}%`,
                      background: '#ff3b30',
                    }}
                  />
                </div>
              </div>
            )}

            {hud.banner && (
              <div
                style={{
                  background: 'rgba(19, 16, 16, 0.94)',
                  border: '1px solid #ff9f0a',
                  color: '#ff9f0a',
                  padding: '4px 12px',
                  borderRadius: 4,
                  fontSize: 11,
                }}
              >
                {hud.banner}
              </div>
            )}
          </div>

          {/* Right Radar Minimap + Engine Controls */}
          <div style={{ pointerEvents: 'auto', display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: 6 }}>
            <canvas
              ref={radarRef}
              width={104}
              height={104}
              style={{
                borderRadius: 4,
                border: '1px solid var(--hairline-strong)',
              }}
            />
            <div style={{ display: 'flex', gap: 4 }}>
              <button
                className={`oc-btn oc-btn-sm ${playMode ? 'oc-btn-primary' : ''}`}
                onClick={() => setPlayMode((v) => !v)}
              >
                {playMode ? '[▶ playing]' : '[orbit inspect]'}
              </button>
              <button className="oc-btn oc-btn-sm" onClick={() => setSfxOn((v) => !v)}>
                {sfxOn ? 'sfx:on' : 'sfx:off'}
              </button>
            </div>
            <div style={{ display: 'flex', gap: 4 }}>
              <button className="oc-btn oc-btn-sm" onClick={onExportGameHtml} title="Export Standalone Playable HTML5 3D Game">
                [↓ .html game]
              </button>
              <button className="oc-btn oc-btn-sm" onClick={onExportGameCs} title="Export Unity C# Game Scripts">
                [↓ .cs]
              </button>
            </div>
          </div>
        </div>

        {/* Bottom Quick-Inject "Add Everything Needed" Agent Bar & Keybinds */}
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            gap: 8,
            flexWrap: 'wrap',
          }}
        >
          <div style={{ pointerEvents: 'auto', display: 'flex', gap: 5, flexWrap: 'wrap' }}>
            {[
              ['+ ally turrets', 'Add autonomous AI plasma defense turrets'],
              ['+ dragon boss', 'Summon colossal Elder Magma Wyvern boss'],
              ['+ launch ramps', 'Add aero stunt jump ramps and boost pads'],
              ['+ storm rain', 'Add storm rain and wet cyberpunk reflections'],
              ['+ citadel keeps', 'Add inner-ring medieval bastion fortresses'],
            ].map(([label, cmd]) => (
              <button
                key={label}
                className="oc-btn oc-btn-sm"
                style={{ background: 'rgba(19, 16, 16, 0.90)' }}
                onClick={() => onQuickMutate?.(cmd)}
              >
                [{label}]
              </button>
            ))}
          </div>

          <div
            style={{
              background: 'rgba(19, 16, 16, 0.88)',
              border: '1px solid var(--hairline)',
              borderRadius: 4,
              padding: '4px 10px',
              fontSize: 10.5,
              color: 'var(--ink-secondary)',
            }}
          >
            WASD move · SHIFT boost · SPACE double-jump · CLICK/F attack · E ultimate
          </div>
        </div>
      </div>
    </div>
  );
}
