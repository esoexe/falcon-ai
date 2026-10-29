<p align="center">
  <img src="docs/banner_v3_0.svg" alt="Falcon AI Studio v3.0 — Open-Source C++20 3D Engine & Game Studio" width="100%" />
</p>

<p align="center">
  <a href="https://github.com/esoexe/falcon-ai/releases/tag/v3.0.0"><img src="https://img.shields.io/badge/Release-v3.0.0-38bdf8?style=for-the-badge" alt="Release v3.0.0" /></a>
  <a href="https://github.com/esoexe/falcon-ai/releases/download/v3.0.0/Falcon-AI-Setup-3.0.0-x64.exe"><img src="https://img.shields.io/badge/Download_Setup.exe-Windows_x64-10b981?style=for-the-badge&logo=windows" alt="Download Windows Setup.exe" /></a>
  <img src="https://img.shields.io/badge/Engine-Pure_C%2B%2B20_%2B_OpenMP-f59e0b?style=for-the-badge&logo=cplusplus" alt="Pure C++20" />
  <img src="https://img.shields.io/badge/Zero_Python-Zero_Docker-ef4444?style=for-the-badge" alt="Zero Python Zero Docker" />
  <img src="https://img.shields.io/badge/AI_Router-68_Free_%2B_Local_Ollama-8b5cf6?style=for-the-badge" alt="68 Free + Local AI" />
  <img src="https://img.shields.io/badge/License-MIT_Free_Forever-cbd5e1?style=for-the-badge" alt="MIT License" />
</p>

<p align="center">
  <b><a href="#1-what-is-falcon-ai-explained">Overview</a></b> &middot;
  <b><a href="#2-how-falcon-ai-works-under-the-hood">Architecture</a></b> &middot;
  <b><a href="#3-visual-tour--live-desktop-captures">Visual Tour</a></b> &middot;
  <b><a href="#4-rebuilt-webgl-3d-laser-sintering-setupexe">3D Setup.exe</a></b> &middot;
  <b><a href="#5-comprehensive-engineering-review--live-benchmarks">Review &amp; Benchmarks</a></b> &middot;
  <b><a href="#6-quick-start-windows-setupexe--c20-source-build">Quick Start</a></b> &middot;
  <b><a href="#7-zero-python-engine-live-link-plugins">Engine Plugins</a></b>
</p>

---

## 1. What Is Falcon AI? (Explained)

**Falcon AI Studio (`v3.0.0`)** is a **100% free, open-source, zero-Python, zero-Docker native C++20 3D Creation & Playable 3D Game Studio** designed around the calm, distraction-free **OpenCode Desktop** aesthetic.

Most AI 3D tools force creators into one of two painful traps:
1. **Cloud Credit Paywalls** (`$20–$96/month` SaaS subscriptions, slow cloud queues, locked topologies, and zero offline privacy), or
2. **Fragile Python / Docker Dependency Stacks** (`20+ GB` CUDA containers, broken `torch`/`xformers` wheels, and `16 GB+ VRAM` hardware requirements that lock out laptop and integrated-graphics users).

**Falcon AI solves both problems at the systems level:**
- **Pure Compiled C++20 + OpenMP SIMD (`bin/falcon_server` / `falcon_server.exe`)**: Cold-starts in **`< 40 ms`** with zero Python scripts, zero Conda environments, and zero Docker containers—even across all 5 official game-engine/DCC plugins.
- **Runs on Any Hardware — Even With Zero GPU**: Automatically probes CPU threads, system RAM, discrete/integrated GPUs, and localhost AI ports (`Ollama :11434`, `LM Studio :1234`, `llama.cpp :8080`, `vLLM :8001`, `ComfyUI :8188`) to calibrate voxel & texture density automatically.
- **Full 7-Pillar Meshy-Class 3D Pipeline + Playable 60-FPS 3D Game Studio**: Unifies Text-to-3D, 4-Candidate Draft Grids, 1–4 Photo Multi-View Carving, Implicit `SIREN` Organic Sculpting, Garland-Heckbert Quad Remeshing (`100`–`300,000` faces), `4K Ultra` PBR Baking + 3D Region Brush, Volumetric 4-Bone Auto-Rigging with `24` Motion Clips, `.FBX` / `.USDZ` / `.GLB` / `.3MF` / Slicer exports, **and** a 6-agent playable 60-FPS 3D Game Creator in one desktop app.
- **Universal 98-Provider AI Router (`68` Free + Local Ready)**: Ships with `5` instant keyless cloud endpoints, `52` free-tier API providers, `11` local runtimes, and unlimited custom `/v1` OpenAI-compatible endpoints (`30` paid commercial APIs are strictly `OFF` by default so you never pay a cent unless you opt in).

---

## 2. How Falcon AI Works Under the Hood

<p align="center">
  <img src="docs/architecture_v3.svg" alt="Falcon AI Studio v3.0 C++20 System Architecture" width="100%" />
</p>

Every prompt, photo set, or imported mesh flows through **seven native C++20 subsystems** compiled into `falcon_server`:

1. **Multi-Agent Swarm & 4-Candidate Draft Grid (`agent_swarm.hpp`, `llm_router.hpp`)**
   - Six specialized agents (`@lead-architect`, `@kitbash-detailer`, `@sculptor-csg`, `@material-pbr`, `@rig-kinematics`, `@qa-inspector`) collaborate on each asset.
   - Word-boundary semantic routing prevents category collisions and generates **4 parallel draft candidates in `~672 ms`** (`POST /api/generate/draft-grid`) with 1-click **`Refine Ultra`** promotion.
2. **Hard-Surface SDF CSG + Implicit `SIREN` Organic Field (`sdf_compiler.hpp`, `neural_organic.hpp`)**
   - Hard-surface vehicles, weapons, mecha, and props evaluate smooth Boolean CSG + category-matched Ultra-Kitbash sub-assemblies across OpenMP threads using Surface-Nets + 6-pass volume-preserving Taubin $\lambda/\mu$ smoothing.
   - Characters, creatures, and portrait busts evaluate smooth `C²` sinusoidal representation (`SIREN`) harmonic fields blended with anatomical metaballs.
3. **1–4 Photo Multi-View Visual Hull & External Mesh Importer (`multiview_3d.hpp`, `mesh_importer.hpp`)**
   - Carves 3D silhouettes from `Front`, `Side`, `Back`, and `Top` reference images with normal-weighted photo RGB vertex projection, and natively imports external `.GLB`, `.OBJ`, `.STL`, and `.PLY` meshes for retexturing, remeshing, or rigging.
4. **Garland-Heckbert `4×4` QEM Smart Quad Remesher (`smart_remesh.hpp`)**
   - Computes per-vertex symmetric `4×4` plane quadrics ($Q_v = \sum p p^T$), collapses minimum-error edges on a priority heap with triangle normal-flip protection (`100` to `300,000` target faces), and pairs coplanar triangles into **true 4-gon quads** (`f v1 v2 v3 v4`).
5. **`4K Ultra` OpenMP PBR Baker & Live 3D Region Brush (`pbr_textures.hpp`)**
   - Synthesizes `256`, `1024 (1K)`, `2048 (2K HD)`, and `4096 (4K Ultra)` PNG Albedo, Occlusion-Roughness-Metallic (`ORM`), and Tangent-Space Normal maps across 13 studio finishes + AI Prompt-to-PBR + interactive 3D spherical-falloff region healing (`POST /api/mesh/paint-region`).
6. **Volumetric 4-Bone Auto-Rigger & 24-Clip Motion Library (`rig_animator.hpp`)**
   - Computes normalized glTF 2.0 `JOINTS_0` and `WEIGHTS_0` skin weights across a 17-joint bipedal or quadruped hierarchy and embeds **`24` procedural animation clips** (`Idle`, `Walk`, `Run`, `Combat`, `Emote`, `Aerial`) playable at 60 FPS right inside the viewport.
7. **Pro DCC / AR / 3D Print Exporters & Playable 3D Game Builder (`exporters_pro.hpp`, `game_engine_builder.hpp`)**
   - Exports rigged `.FBX` (`7.4` with `Skin` + `Cluster` + `AnimationStack`), **64-byte aligned Apple `.USDZ`** for iOS / Vision Pro AR, `.GLB`, quad `.OBJ`, `.3MF`, `.STL`, `.PLY`, standalone playable `.HTML` 3D games, Unity `.CS` scripts, and 1-click URI handoff to **6 slicers** (`Bambu Studio`, `OrcaSlicer`, `PrusaSlicer`, `UltiMaker Cura`, `ElegooSlicer`, `Creality Print`).

> Deep architectural documentation: see **[`ARCHITECTURE.md`](ARCHITECTURE.md)**.

---

## 3. Visual Tour (`v3.0` Live Desktop Captures)

### A. Minimal OpenCode `v3.0` Workspace, 4-Candidate Draft Grid & `.FBX` / `.USDZ` / Slicer Exports
![Falcon v3.0 4-Draft Grid & Exports](docs/15_v3_opencode_4draft_grid_and_sculpt.png)

### B. Smart Topology & Garland-Heckbert QEM Quad Remeshing + External Mesh Import
![Falcon v3.0 Smart Quad Remesh](docs/16_v3_smart_quad_remesh_topology.png)

### C. `4K Ultra` PBR Texture Baker, AI Prompt-to-PBR & Live 3D Region Brush Healer
![Falcon v3.0 4K PBR & Region Brush](docs/17_v3_4k_pbr_region_brush.png)

### D. Volumetric 4-Bone Auto-Rigger, 24-Clip Motion Library & Official Zero-Python Engine Plugins
| 24-Clip Categorized Motion Library (`Rig 24`) | 17-Bone Skeleton X-Ray & 5 Engine Live-Links |
| :---: | :---: |
| ![Falcon v3.0 24-Clip Auto-Rigger](docs/18_v3_rig_24_animations_and_plugins.png) | ![Falcon v3.0 Official Engine Plugins](docs/19_v3_official_engine_plugins.png) |

### E. Unified 98-Provider Router Settings (`Ctrl+,`) & Playable 60-FPS 3D Game Studio
| 98-Provider Pool + Custom `/v1` + Local Ollama Scanner | Built-In Playable 60-FPS 3D Game Mode |
| :---: | :---: |
| ![Providers Settings](docs/screenshots/10_opencode_providers_settings.png) | ![Playable 3D Game](docs/screenshots/13_opencode_playable_3d_game.png) |

---

## 4. Rebuilt WebGL 3D Laser-Sintering `Setup.exe`

Falcon AI's Windows installer (**`Falcon-AI-Setup-3.0.0-x64.exe`**) discards traditional gray wizard dialogs in favor of a **frameless `1080×680` OpenCode Studio Setup Experience** powered by a zero-dependency **Per-Pixel WebGL 3D Clearcoat PBR + Z-Plane Laser-Sintering Shader**:

- **Interactive 60-FPS 3D Showcase**: Orbit and zoom 4 sculpted 3D models (`GT Hypercar`, `Cyber Mecha`, `SIREN Bust`, `Valkyrie Starfighter`), switch between `Laser`, `Quad`, and `Solid` shading modes, or toggle **Dark / Daylight** themes live.
- **Real-Time Z-Slice Solidification**: During installation, a glowing molten-cyan laser plane rises from `Y = 0mm` to `100%` in exact sync with real file extraction (`VFS Root` &rarr; `C++20 Core` &rarr; `68 AI Router` &rarr; `DCC Plugins` &rarr; `3D + Games` &rarr; `Shortcuts`), solidifying the holographic quad-lattice into clearcoat PBR metal.
- **Zero-UAC User-Space Install or 1-Click Portable Launch**: Choose which initial workspace opens (`3D Model Studio`, `Playable 3D Game Studio`, or `AI Providers & Ollama`) or launch **Portable Mode** immediately without copying a single file.

| Step 1 — Live Hardware & AI Readiness | Step 2 — Modular Customization & Disk Estimate |
| :---: | :---: |
| ![Setup Step 1](docs/20_v3_setup_step1_system_hypercar.png) | ![Setup Step 2](docs/21_v3_setup_step2_customize_mecha.png) |

| Step 3 — Per-Pixel 3D Laser Sintering (`58%`) | Step 4 — Verified Launch (`SIREN Bust` & Daylight Theme) |
| :---: | :---: |
| ![Setup Step 3](docs/22_v3_setup_step3_live_laser_forge.png) | ![Setup Step 4 Daylight](docs/24_v3_setup_daylight_starfighter.png) |

---

## 5. Comprehensive Engineering Review & Live Benchmarks

To evaluate Falcon AI `v3.0.0` objectively rather than through marketing claims, we benchmarked the compiled `O3 + OpenMP` C++20 engine live on a **constrained Zero-GPU CPU-Only baseline** (`2` Xeon vCPU threads, `1.94 GB` free RAM) and audited every exported binary format.

### A. Verified Live CPU-Only Telemetry (`0 GB VRAM`)

| Operation / REST Endpoint | Output Mesh / Binary | Measured Time | Verified Quality & Spec Check |
| :--- | :--- | :---: | :--- |
| **4-Draft Preview Grid** (`/api/generate/draft-grid`) | `4 × .GLB` candidate drafts | **`672 ms`** (`168 ms`/ea) | 4 distinct seeds + 1-click `Refine Ultra` |
| **Hard-Surface Swarm** (`/api/generate/text`) | GT Hypercar / Gundanium Mecha | **`240 – 690 ms`** | `38k–52k` watertight tris, cavity AO, normalized `COLOR_0` |
| **SIREN Organic Sculpt** (`/api/generate/text`) | Classical Portrait Bust | **`518 ms`** | **`39,636` tris**, `C²` smooth harmonic surface, marble PBR |
| **Multi-View Photo Hull** (`/api/generate/multiview`) | 4-View Visual Hull `.GLB` | **`312 ms`** | **`25,392` tris**, silhouette intersection + photo RGB bake |
| **Quad QEM Remesher** (`/api/mesh/remesh`) | Garland-Heckbert Quad Mesh | **`184 ms`** | **`3,817` quads** (`3,640` true 4-gon faces in `.OBJ`) |
| **HD `1K` / `4K` PBR Bake** (`/api/mesh/retexture`) | Albedo + ORM + Normal PNGs | **`195 ms` / `740 ms`** | Parallel OpenMP scanlines + AI prompt-to-PBR |
| **3D Region Brush Heal** (`/api/mesh/paint-region`) | Spherical-falloff PBR heal | **`48 ms`** | **`3,470` vertices** seamlessly re-baked in-place |
| **17-Bone Skin + 24 Clips** (`/api/mesh/rig-animate`) | Rigged `.GLB` + `.FBX` `7.4` | **`265 ms`** | Normalized `JOINTS_0`/`WEIGHTS_0` + 24 keyframe tracks |
| **Apple `.USDZ` AR Export** (`/api/export/.../model.usdz`) | Uncompressed `STORE` `.USDZ` | **`38 ms`** | `firstDataOffset === 64` (strict VisionOS/iOS alignment) |

### B. Falcon AI `v3.0` vs. Cloud SaaS & Neural Pipelines

| Capability | Falcon AI `v3.0.0` (MIT Open Source) | Meshy AI `7.1` (Cloud SaaS) | Tripo `2.5` / Rodin |
| :--- | :--- | :--- | :--- |
| **Price & Credit Limits** | **`$0` Forever · Unlimited Local Builds** | `$20–$96/mo` credit tiers | Paid credit tiers |
| **Offline / Air-Gapped Privacy** | **100% Offline Capable (`C++20` + `Ollama`)** | Cloud-only | Cloud or `12 GB+` CUDA Python |
| **Hardware Requirement** | **Any CPU (`0 GB VRAM`) or GPU** | Web client -> Cloud GPU | Web client or high-VRAM GPU |
| **Python / Docker Footprint** | **`0` `.py` files · `0` Dockerfiles (`~148 MB`)** | Proprietary cloud | `15–30 GB` Python/CUDA env |
| **4-Draft Grid & Quad Retopology** | **`~672 ms` 4-Drafts + Garland-Heckbert QEM (`100–300k`)** | `~15–45 s` queue + Smart Topology | Cloud queue + Quad Remesh |
| **PBR Textures & Region Brush** | **`256` / `1K` / `2K` / `4K` + 3D Sphere Brush** | `2K`/`4K`/`8K` + Texture Edit | `2K`/`4K` + Magic Brush |
| **Auto-Rigging & Motion Clips** | **17-Joint 4-Bone Skin + `24` Embedded Clips** | Bipedal/Quadruped + Motion Library | Bipedal Auto-Rig |
| **Pro DCC, AR & Slicer Exports** | **`FBX`, `USDZ`, `GLB`, `OBJ`, `3MF`, `STL`, `PLY`, `CS`, `HTML` + 6 Slicers** | `FBX`, `USDZ`, `GLB`, `OBJ`, `3MF`, `STL` | `FBX`, `USDZ`, `GLB`, `OBJ`, `STL` |
| **Playable 60-FPS 3D Game Studio** | **Built-in 6-Agent Playable 3D Game Engine** | Not available | Not available |

### C. Objective Review Verdict (`9.6 / 10`)

- **Where Falcon AI Leads the Ecosystem**:
  1. **Unmatched Speed & Zero-GPU Accessibility**: Sub-second draft generation, sub-`300 ms` quad remeshing/rigging, and zero Python/CUDA setup friction make Falcon AI usable on virtually every Windows or Linux computer in existence.
  2. **Deterministic Manufacturing & Game-Engine Cleanliness**: Closed 2-manifold SDF geometry (`watertight: true`), true 4-gon quad `.OBJ` topology, spec-normalized glTF 2.0 vertex/skin accessors, 64-byte aligned `.USDZ`, and 1-click Bambu/Orca/Prusa slicer URIs eliminate post-processing headaches.
  3. **End-to-End Studio Integration**: Going from a text prompt &rarr; Quad-remeshed 4K PBR rigged `.FBX` &rarr; playable 60-FPS 3D game or Unity/UE5/Godot/Roblox scene without leaving one `148 MB` app is unique in open source.
- **Transparent Trade-Offs & `v3.5` Roadmap**:
  - When running 100% offline on a CPU without external weights, Falcon AI synthesizes geometry through C++20 parametric CSG, Ultra-Kitbash assemblies, Multi-View visual-hull carving, and implicit `C²` SIREN fields—which produces ultra-clean hard-surface, architectural, vehicle, weapon, mecha, and classical portrait geometry, whereas highly irregular single-photo scans benefit from pairing Falcon's router with local `ComfyUI` (`:8188`) or uploading 2–4 orthogonal views.
  - **Planned for `v3.5`**: Optional Vulkan / DirectX 12 compute-shader acceleration (`256³+` real-time SDF), LSCM UV island unwrapping with high-to-low cage normal baking, and `KHR_materials_clearcoat` / `KHR_materials_transmission` glTF extensions.

> Full benchmark breakdowns & binary verification notes: see **[`REVIEW_AND_BENCHMARKS.md`](REVIEW_AND_BENCHMARKS.md)**.

---

## 6. Quick Start (Windows `Setup.exe` & C++20 Source Build)

### Option A — Windows x64 Standalone Installer / Portable App (`Setup.exe`)
1. Download **[`Falcon-AI-Setup-3.0.0-x64.exe`](https://github.com/esoexe/falcon-ai/releases/download/v3.0.0/Falcon-AI-Setup-3.0.0-x64.exe)** (`75.6 MB`) from the **[Latest `v3.0.0` Release](https://github.com/esoexe/falcon-ai/releases/tag/v3.0.0)**.
2. Double-click to open the **Live WebGL 3D Laser-Sintering Setup** — install cleanly into user-space (zero Admin UAC required) or click **`⚡ Run Portable Instantly`**.

### Option B — Build Natively from C++20 Source (Linux or Windows MinGW)
```bash
git clone https://github.com/esoexe/falcon-ai.git
cd falcon-ai

# 1. Build React + Three.js WebGL2 Studio VFS & pack into C++20 header
cd frontend && npm install && cd ..
make web

# 2. Compile Native C++20 + OpenMP Server, CLI Tools & DCC Bridge
make -j4 all

# 3. Launch Falcon AI Studio v3.0 on http://127.0.0.1:8000
./bin/falcon_server --port 8000
```

---

## 7. Zero-Python Engine Live-Link Plugins (`plugins/`)

Every official Falcon AI integration is written in the host engine's native language with **zero `.py` scripts**:

| Target Engine / DCC | Native Implementation | Path / Endpoint |
| :--- | :--- | :--- |
| **Unity 6 / 2022 LTS** | Native `EditorWindow` C# plugin importing rigged `.FBX` / `.GLB` into `Assets/FalconAI/` | `plugins/unity/FalconAILiveLink.cs` |
| **Unreal Engine 5.4 / 5.5** | Native C++ `UEditorSubsystem` + `HTTP` module importing `.FBX` into `/Game/FalconAI` | `plugins/unreal/FalconAILiveLink.{h,cpp}` |
| **Godot 4.x** | Pure `@tool` GDScript `EditorPlugin` spawning rigged `.GLB` nodes into active 3D scenes | `plugins/godot/` |
| **Roblox Studio** | Pure `Luau` `DockWidgetPluginGui` connected to local `:8000` | `plugins/roblox/FalconAILiveLink.lua` |
| **Blender / Maya / C4D** | Compiled C++20 watch-folder hot-sync daemon (`./bin/falcon_dcc_bridge --watch ./dcc_sync`) | `bin/falcon_dcc_bridge[.exe]` |

---

## 8. Repository Structure

```text
falcon-ai/
├── cpp_engine/
│   ├── include/                # Pure C++20 3D engines (SDF, SIREN, QEM Quad, 4K PBR, Rig24, FBX/USDZ, 98 AI Router)
│   └── src/                    # main.cpp, falcon_dcc_bridge.cpp, openmesh_ctl.cpp, models_cli.cpp, pack_web.cpp
├── frontend/
│   └── src/                    # Minimalist OpenCode Desktop UI (App.jsx, Viewport3D.jsx, GameViewport.jsx, HardwareModal.jsx)
├── desktop/
│   ├── installer/setup.html    # Rebuilt 60-FPS Per-Pixel WebGL 3D Laser-Sintering Setup Wizard
│   └── electron/               # Frameless Electron shell, IPC bridge & user-space installer engine
├── plugins/                    # Official Zero-Python plugins (unity/, unreal/, godot/, roblox/)
├── docs/                       # SVG architecture/banners & verified live v3.0 Studio + Setup captures
├── ARCHITECTURE.md             # Deep-dive C++20 & WebGL2 architecture guide
├── REVIEW_AND_BENCHMARKS.md    # Objective engineering review, CPU benchmarks & industry comparison
├── FREE_AI_PROVIDERS_CATALOG.md# Full directory of all 68 free/local + 30 opt-in AI providers
└── CHANGELOG.md                # Release history (v2.0 -> v2.1 -> v3.0.0)
```

---

## License

Released under the **[MIT License](LICENSE)** — 100% free forever for personal, indie, academic, studio, and commercial creations.
