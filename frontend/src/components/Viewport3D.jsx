import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { RoomEnvironment } from 'three/examples/jsm/environments/RoomEnvironment.js';

const LIGHTING_PRESETS = {
  studio: { key: 0xfff7ed, keyInt: 2.6, fill: 0x93c5fd, fillInt: 1.15, rim: 0xe2e8f0, rimInt: 1.45, envInt: 1.28 },
  cyberpunk: { key: 0x22d3ee, keyInt: 2.9, fill: 0xec4899, fillInt: 1.85, rim: 0xa855f7, rimInt: 2.3, envInt: 0.90 },
  forge: { key: 0xfbbf24, keyInt: 3.0, fill: 0xf97316, fillInt: 1.4, rim: 0x38bdf8, rimInt: 1.15, envInt: 1.10 },
  showroom: { key: 0xffffff, keyInt: 3.2, fill: 0xe2e8f0, fillInt: 1.6, rim: 0x93c5fd, rimInt: 1.35, envInt: 1.45 },
};

function makeOverhangMaterial() {
  return new THREE.ShaderMaterial({
    uniforms: {},
    vertexShader: `
      varying vec3 vWorldNormal;
      varying vec3 vWorldPos;
      void main() {
        vec4 wp = modelMatrix * vec4(position, 1.0);
        vWorldPos = wp.xyz;
        vWorldNormal = normalize(mat3(modelMatrix) * normal);
        gl_Position = projectionMatrix * viewMatrix * wp;
      }
    `,
    fragmentShader: `
      varying vec3 vWorldNormal;
      varying vec3 vWorldPos;
      void main() {
        vec3 n = normalize(vWorldNormal);
        float light = 0.35 + 0.65 * max(dot(n, normalize(vec3(0.4, 0.8, 0.5))), 0.0);
        // Steep downward overhang > 45 deg above Z/Y bed
        bool isOverhang = (n.y < -0.7071) && (vWorldPos.y > 0.06);
        vec3 safeCol = vec3(0.12, 0.72, 0.52) * light;
        vec3 warnCol = vec3(0.95, 0.24, 0.22) * (0.55 + 0.45 * light);
        gl_FragColor = vec4(isOverhang ? warnCol : safeCol, 1.0);
      }
    `,
    side: THREE.DoubleSide,
  });
}

export default function Viewport3D({
  glbUrl,
  shadingMode = 'pbr',
  lightingPreset = 'studio',
  showGrid = true,
  autoRotate = false,
  selectedPartId = null,
  onSelectPart,
  regionBrushActive = false,
  onPaintRegionPoint = null,
  rigData = null,
  showRig = false,
  activeAnimation = 'rest',
  cameraCommand = null,
  theme = 'dark',
}) {
  const mountRef = useRef(null);
  const brushRef = useRef({ active: false, cb: null });
  brushRef.current = { active: regionBrushActive, cb: onPaintRegionPoint };
  const stateRef = useRef({
    scene: null,
    camera: null,
    renderer: null,
    controls: null,
    modelRoot: null,
    mixer: null,
    gltfClips: [],
    wireOverlayGroup: null,
    rigGroup: null,
    gridHelper: null,
    lights: {},
    originalMaterials: new Map(),
    animMode: 'rest',
    rigJointMeshes: [],
    rigBoneLines: [],
    baseJoints: [],
  });

  // Initialize Three.js Studio Scene once
  useEffect(() => {
    const container = mountRef.current;
    if (!container) return;

    const scene = new THREE.Scene();

    const camera = new THREE.PerspectiveCamera(
      42,
      Math.max(1, container.clientWidth) / Math.max(1, container.clientHeight),
      0.05,
      200
    );
    camera.position.set(3.4, 2.5, 4.2);

    const renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      preserveDrawingBuffer: true,
    });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.setSize(container.clientWidth, container.clientHeight);
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.12;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    container.innerHTML = '';
    container.appendChild(renderer.domElement);

    // Offline Studio Environment Reflections via RoomEnvironment
    const pmrem = new THREE.PMREMGenerator(renderer);
    const envTex = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
    scene.environment = envTex;
    scene.environmentIntensity = 0.95;

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.06;
    controls.target.set(0, 1.0, 0);

    // OpenCode Monochrome Architectural Floor Grid + Shadow Catcher
    const grid = new THREE.GridHelper(12, 24, 0x6e6868, 0x2c2727);
    grid.material.opacity = 0.55;
    grid.material.transparent = true;
    scene.add(grid);

    const shadowPlane = new THREE.Mesh(
      new THREE.PlaneGeometry(14, 14),
      new THREE.ShadowMaterial({ opacity: 0.32 })
    );
    shadowPlane.rotation.x = -Math.PI / 2;
    shadowPlane.position.y = -0.002;
    shadowPlane.receiveShadow = true;
    scene.add(shadowPlane);

    // Three-point Studio Lights with High-Resolution Contact Shadows
    const keyLight = new THREE.DirectionalLight(0xfff7ed, 2.6);
    keyLight.position.set(4.2, 6.8, 4.8);
    keyLight.castShadow = true;
    keyLight.shadow.mapSize.set(2048, 2048);
    keyLight.shadow.bias = -0.0004;
    keyLight.shadow.normalBias = 0.018;
    keyLight.shadow.camera.near = 0.5;
    keyLight.shadow.camera.far = 20;
    keyLight.shadow.camera.left = -3.8;
    keyLight.shadow.camera.right = 3.8;
    keyLight.shadow.camera.top = 3.8;
    keyLight.shadow.camera.bottom = -3.8;
    scene.add(keyLight);

    const fillLight = new THREE.DirectionalLight(0x93c5fd, 1.15);
    fillLight.position.set(-5.0, 3.2, 2.8);
    scene.add(fillLight);

    const rimLight = new THREE.DirectionalLight(0xe2e8f0, 1.45);
    rimLight.position.set(-0.5, 4.8, -5.5);
    scene.add(rimLight);

    const modelRoot = new THREE.Group();
    scene.add(modelRoot);

    const wireOverlayGroup = new THREE.Group();
    scene.add(wireOverlayGroup);

    const rigGroup = new THREE.Group();
    scene.add(rigGroup);

    const st = stateRef.current;
    st.scene = scene;
    st.camera = camera;
    st.renderer = renderer;
    st.controls = controls;
    st.modelRoot = modelRoot;
    st.wireOverlayGroup = wireOverlayGroup;
    st.rigGroup = rigGroup;
    st.gridHelper = grid;
    st.lights = { keyLight, fillLight, rimLight };

    // Raycast click to pick semantic sub-part
    const raycaster = new THREE.Raycaster();
    const pointer = new THREE.Vector2();
    let downPos = { x: 0, y: 0 };

    const onPointerDown = (e) => {
      downPos = { x: e.clientX, y: e.clientY };
    };
    const onPointerUp = (e) => {
      if (Math.hypot(e.clientX - downPos.x, e.clientY - downPos.y) > 5) return;
      if (!st.modelRoot || !onSelectPart) return;
      const rect = renderer.domElement.getBoundingClientRect();
      pointer.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      pointer.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
      raycaster.setFromCamera(pointer, camera);
      const hits = raycaster.intersectObjects(st.modelRoot.children, true);
      if (hits.length > 0) {
        if (brushRef.current.active && brushRef.current.cb) {
          const hp = hits[0].point;
          brushRef.current.cb([hp.x, hp.y, hp.z]);
          return;
        }
        if (onSelectPart) {
          let obj = hits[0].object;
          const partName = obj.name || (obj.parent && obj.parent.name) || null;
          onSelectPart(partName);
        }
      }
    };
    renderer.domElement.addEventListener('pointerdown', onPointerDown);
    renderer.domElement.addEventListener('pointerup', onPointerUp);

    // ResizeObserver for crisp responsiveness
    const ro = new ResizeObserver(() => {
      if (!container) return;
      const w = Math.max(1, container.clientWidth);
      const h = Math.max(1, container.clientHeight);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    });
    ro.observe(container);

    // Animation loop
    let rafId = 0;
    const clock = new THREE.Clock();
    const animate = () => {
      rafId = requestAnimationFrame(animate);
      const dt = Math.min(clock.getDelta(), 0.05);
      const t = clock.elapsedTime;
      const mode = st.animMode;

      if (st.mixer && st.gltfClips && st.gltfClips.length > 0 && mode !== 'rest') {
        st.mixer.update(dt);
      } else if (st.modelRoot) {
        // Reset base transforms before applying procedural clip
        st.modelRoot.position.set(0, 0, 0);
        st.modelRoot.rotation.set(0, 0, 0);
        st.modelRoot.scale.set(1, 1, 1);

        if (mode === 'idle_breathe') {
          const s = 1.0 + 0.025 * Math.sin(t * 2.4);
          st.modelRoot.scale.set(1.0, s, 1.0);
          st.modelRoot.rotation.y = 0.08 * Math.sin(t * 1.2);
        } else if (mode === 'walk_stride') {
          st.modelRoot.position.y = Math.abs(Math.sin(t * 5.2)) * 0.12;
          st.modelRoot.rotation.z = 0.06 * Math.sin(t * 5.2);
          st.modelRoot.rotation.y = 0.14 * Math.sin(t * 2.6);
        } else if (mode === 'hero_action') {
          st.modelRoot.rotation.y = Math.sin(t * 3.0) * 0.45;
          st.modelRoot.rotation.x = Math.sin(t * 6.0) * 0.10;
          st.modelRoot.position.y = Math.max(0, Math.sin(t * 3.0)) * 0.18;
        } else if (mode === 'hover_float') {
          st.modelRoot.position.y = 0.22 + 0.14 * Math.sin(t * 2.2);
          st.modelRoot.rotation.y = t * 0.65;
        } else if (mode === 'turntable') {
          st.modelRoot.rotation.y = t * 0.85;
        }

        // Sync wireframe overlay & rig group transform with animated modelRoot
        st.wireOverlayGroup.position.copy(st.modelRoot.position);
        st.wireOverlayGroup.rotation.copy(st.modelRoot.rotation);
        st.wireOverlayGroup.scale.copy(st.modelRoot.scale);

        st.rigGroup.position.copy(st.modelRoot.position);
        st.rigGroup.rotation.copy(st.modelRoot.rotation);
        st.rigGroup.scale.copy(st.modelRoot.scale);
      }

      controls.update();
      renderer.render(scene, camera);
    };
    animate();

    return () => {
      cancelAnimationFrame(rafId);
      ro.disconnect();
      renderer.domElement.removeEventListener('pointerdown', onPointerDown);
      renderer.domElement.removeEventListener('pointerup', onPointerUp);
      pmrem.dispose();
      renderer.dispose();
    };
  }, []);

  // Load GLB model whenever glbUrl changes
  useEffect(() => {
    const st = stateRef.current;
    if (!glbUrl || !st.modelRoot) return;

    const loader = new GLTFLoader();
    loader.load(
      glbUrl,
      (gltf) => {
        // Clear previous model
        while (st.modelRoot.children.length > 0) {
          st.modelRoot.remove(st.modelRoot.children[0]);
        }
        st.originalMaterials.clear();

        const maxAniso = st.renderer.capabilities.getMaxAnisotropy();
        const root = gltf.scene;
        root.traverse((child) => {
          if (child.isMesh) {
            child.castShadow = true;
            child.receiveShadow = true;
            if (child.geometry) {
              if (!child.geometry.attributes.normal) {
                child.geometry.computeVertexNormals();
              }
              if (child.geometry.attributes.uv && !child.geometry.attributes.uv1) {
                child.geometry.setAttribute('uv1', child.geometry.attributes.uv);
              }
            }
            const mat = child.material;
            if (mat) {
              ['map', 'normalMap', 'roughnessMap', 'metalnessMap', 'aoMap'].forEach((tk) => {
                if (mat[tk]) {
                  mat[tk].anisotropy = maxAniso;
                  mat[tk].needsUpdate = true;
                }
              });
              if (mat.normalMap && mat.normalScale) {
                mat.normalScale.set(1.32, 1.32);
              }
              mat.envMapIntensity = 1.32;
              mat.needsUpdate = true;
            }
            st.originalMaterials.set(child.uuid, child.material);
          }
        });

        st.modelRoot.add(root);

        // Bind Khronos glTF 2.0 Skeletal AnimationMixer if rigged clips are embedded!
        st.mixer = null;
        st.gltfClips = gltf.animations || [];
        if (st.gltfClips.length > 0) {
          st.mixer = new THREE.AnimationMixer(root);
          const match = st.gltfClips.find((c) => c.name && c.name.startsWith(st.animMode)) || st.gltfClips[0];
          if (match) {
            const act = st.mixer.clipAction(match);
            act.play();
          }
        }

        // Fit camera smoothly to bounding box
        const box = new THREE.Box3().setFromObject(root);
        if (!box.isEmpty()) {
          const size = box.getSize(new THREE.Vector3());
          const center = box.getCenter(new THREE.Vector3());
          const maxDim = Math.max(size.x, size.y, size.z, 0.8);
          st.controls.target.copy(center);
          st.camera.position.set(
            center.x + maxDim * 1.28,
            center.y + maxDim * 0.82,
            center.z + maxDim * 1.48
          );
          st.controls.update();
        }

        // Apply active shading mode right away
        applyShadingToScene(st, shadingMode, selectedPartId);
      },
      undefined,
      (err) => {
        console.error('GLTF Load error:', err);
      }
    );
  }, [glbUrl]);

  // Apply shading mode & part selection highlight
  useEffect(() => {
    const st = stateRef.current;
    if (!st.modelRoot) return;
    applyShadingToScene(st, shadingMode, selectedPartId);
  }, [shadingMode, selectedPartId]);

  // Apply lighting preset, grid, autoRotate, animation
  useEffect(() => {
    const st = stateRef.current;
    if (!st.scene) return;
    const p = LIGHTING_PRESETS[lightingPreset] || LIGHTING_PRESETS.studio;
    st.lights.keyLight.color.setHex(p.key);
    st.lights.keyLight.intensity = p.keyInt;
    st.lights.fillLight.color.setHex(p.fill);
    st.lights.fillLight.intensity = p.fillInt;
    st.lights.rimLight.color.setHex(p.rim);
    st.lights.rimLight.intensity = p.rimInt;
    st.scene.environmentIntensity = p.envInt;
    if (st.gridHelper) st.gridHelper.visible = showGrid;
    if (st.controls) st.controls.autoRotate = autoRotate;
    st.animMode = activeAnimation;
    if (st.mixer && st.gltfClips && st.gltfClips.length > 0 && activeAnimation !== 'rest') {
      const match = st.gltfClips.find((c) => c.name && c.name.startsWith(activeAnimation)) || st.gltfClips[0];
      if (match) {
        st.mixer.stopAllAction();
        st.mixer.clipAction(match).reset().play();
      }
    }
  }, [lightingPreset, showGrid, autoRotate, activeAnimation]);

  // Build 3D Bone & Joint Overlay when rigData or showRig changes
  useEffect(() => {
    const st = stateRef.current;
    if (!st.rigGroup) return;
    while (st.rigGroup.children.length > 0) {
      st.rigGroup.remove(st.rigGroup.children[0]);
    }
    if (!rigData || !rigData.joints || (!showRig && shadingMode !== 'xray')) return;

    const jointMap = {};
    const jointMat = new THREE.MeshBasicMaterial({ color: 0x38bdf8, depthTest: false });
    const rootMat = new THREE.MeshBasicMaterial({ color: 0xf59e0b, depthTest: false });

    rigData.joints.forEach((j) => {
      const pos = new THREE.Vector3(j.pos[0], j.pos[1], j.pos[2]);
      jointMap[j.id] = pos;
      const sph = new THREE.Mesh(
        new THREE.SphereGeometry(j.parent ? 0.042 : 0.062, 14, 14),
        j.parent ? jointMat : rootMat
      );
      sph.position.copy(pos);
      sph.renderOrder = 999;
      st.rigGroup.add(sph);
    });

    const lineMat = new THREE.LineBasicMaterial({ color: 0x22d3ee, depthTest: false, linewidth: 2 });
    rigData.joints.forEach((j) => {
      if (j.parent && jointMap[j.parent]) {
        const pts = [jointMap[j.parent], jointMap[j.id]];
        const geom = new THREE.BufferGeometry().setFromPoints(pts);
        const line = new THREE.Line(geom, lineMat);
        line.renderOrder = 998;
        st.rigGroup.add(line);
      }
    });
  }, [rigData, showRig, shadingMode]);

  // Camera snap & PNG screenshot commands
  useEffect(() => {
    const st = stateRef.current;
    if (!cameraCommand || !st.camera || !st.controls) return;
    const target = st.controls.target.clone();
    const dist = st.camera.position.distanceTo(target) || 4.0;

    if (cameraCommand.type === 'front') {
      st.camera.position.set(target.x, target.y + dist * 0.15, target.z + dist);
    } else if (cameraCommand.type === 'side') {
      st.camera.position.set(target.x + dist, target.y + dist * 0.15, target.z);
    } else if (cameraCommand.type === 'top') {
      st.camera.position.set(target.x, target.y + dist, target.z + 0.01);
    } else if (cameraCommand.type === 'iso') {
      st.camera.position.set(target.x + dist * 0.72, target.y + dist * 0.52, target.z + dist * 0.78);
    } else if (cameraCommand.type === 'screenshot') {
      st.renderer.render(st.scene, st.camera);
      const url = st.renderer.domElement.toDataURL('image/png');
      const a = document.createElement('a');
      a.href = url;
      a.download = 'openmesh_studio_render.png';
      a.click();
    }
    st.controls.update();
  }, [cameraCommand]);

  return <div ref={mountRef} style={{ width: '100%', height: '100%' }} />;
}

function applyShadingToScene(st, shadingMode, selectedPartId) {
  while (st.wireOverlayGroup.children.length > 0) {
    st.wireOverlayGroup.remove(st.wireOverlayGroup.children[0]);
  }

  const clayMat = new THREE.MeshStandardMaterial({
    color: 0xd6d3d1,
    roughness: 0.58,
    metalness: 0.08,
    side: THREE.DoubleSide,
  });
  const wireMat = new THREE.MeshBasicMaterial({
    color: 0x38bdf8,
    wireframe: true,
  });
  const normalMat = new THREE.MeshNormalMaterial({ side: THREE.DoubleSide });
  const overhangMat = makeOverhangMaterial();
  const xrayMat = new THREE.MeshPhysicalMaterial({
    color: 0x64748b,
    roughness: 0.25,
    metalness: 0.1,
    transparent: true,
    opacity: 0.34,
    depthWrite: false,
  });

  st.modelRoot.traverse((child) => {
    if (!child.isMesh) return;
    const orig = st.originalMaterials.get(child.uuid);
    const nodeName = child.name || (child.parent && child.parent.name) || '';
    const isPicked = selectedPartId && (nodeName === selectedPartId || nodeName.startsWith(selectedPartId));

    if (shadingMode === 'clay') {
      child.material = new THREE.MeshStandardMaterial({
        color: 0xd6d0c8,
        roughness: 0.56,
        metalness: 0.06,
        normalMap: orig?.normalMap || null,
        normalScale: new THREE.Vector2(1.15, 1.15),
        aoMap: orig?.aoMap || null,
        vertexColors: Boolean(child.geometry?.attributes?.color),
        side: THREE.DoubleSide,
      });
    } else if (shadingMode === 'wireframe') {
      child.material = wireMat;
    } else if (shadingMode === 'normals') {
      child.material = normalMat;
    } else if (shadingMode === 'overhang') {
      child.material = overhangMat;
    } else if (shadingMode === 'xray') {
      child.material = xrayMat;
    } else {
      // pbr or pbr_wire
      if (orig) {
        const cloned = orig.clone();
        if (isPicked && cloned.emissive) {
          cloned.emissive = new THREE.Color(0x38bdf8);
          cloned.emissiveIntensity = Math.max(cloned.emissiveIntensity || 0, 0.38);
        }
        child.material = cloned;
      }
    }

    if (shadingMode === 'pbr_wire') {
      const overlay = new THREE.Mesh(
        child.geometry,
        new THREE.MeshBasicMaterial({
          color: 0x38bdf8,
          wireframe: true,
          transparent: true,
          opacity: 0.28,
        })
      );
      overlay.matrixAutoUpdate = false;
      child.updateWorldMatrix(true, false);
      overlay.matrix.copy(child.matrixWorld);
      st.wireOverlayGroup.add(overlay);
    }
  });
}
