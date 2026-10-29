# Falcon AI Studio `v3.0.0` — Complete C++20 System Architecture

Falcon AI replaces Python inference stacks and Docker container orchestration with a **single compiled C++20 binary (`falcon_server` / `falcon_server.exe`)**, a **compiled C++20 DCC Watch-Folder Daemon (`falcon_dcc_bridge` / `falcon_dcc_bridge.exe`)**, an **OpenCode Desktop React + Three.js WebGL2 Studio**, **5 Zero-Python Engine Live-Link Plugins**, and a **Per-Pixel WebGL 3D Laser-Sintering `Setup.exe`**.

<p align="center">
  <img src="docs/architecture_v3.svg" alt="Falcon AI v3.0 C++20 Architecture Diagram" width="100%" />
</p>

---

## 1. End-to-End C++20 Multi-Agent & 7-Pillar 3D Pipeline

```text
Text Prompt / 1–4 Photos / Sketch / External Mesh (.GLB/.OBJ/.STL/.PLY)
        │
        ▼
┌────────────────────────────────────────────────────────────────────────────┐
│ 1. @lead-architect (llm_router.hpp + provider_catalog.hpp)                 │
│    • Word-boundary prompt classifier + 98-Provider AI Router               │
│    • 68 Free Providers (5 Keyless, 52 Free-Key, 11 Local Ollama/LM Studio) │
│    • 4-Candidate Parallel Fast Draft Grid (POST /api/generate/draft-grid)  │
└───────┬────────────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────────────┐
│ 2. @sculptor-csg & @siren-organic (sdf_compiler.hpp + neural_organic.hpp)  │
│    • Hard-Surface CSG + Ultra-Kitbash Sub-Assemblies (agent_swarm.hpp)     │
│    • Implicit C² SIREN Harmonic + Anatomical Metaball Sculptor             │
│    • 1–4 Photo Multi-View Visual Hull + RGB Projection (multiview_3d.hpp)  │
│    • OpenMP 104³–136³ Surface-Nets + Volume-Preserving Taubin Smoothing    │
└───────┬────────────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────────────┐
│ 3. @smart-topology (smart_remesh.hpp + mesh_importer.hpp)                  │
│    • Native Importer for .GLB, .OBJ, Binary/ASCII .STL, and .PLY           │
│    • Garland-Heckbert 4×4 Quadric Error Metric (QEM) Edge-Collapse         │
│    • Target 100 – 300,000 faces + Coplanar True 4-Gon Quad Retopology      │
└───────┬────────────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────────────┐
│ 4. @material-pbr (pbr_textures.hpp)                                        │
│    • OpenMP 256 / 1024 (1K) / 2048 (2K) / 4096 (4K) Albedo, ORM & Normal   │
│    • 13 Studio PBR Finishes + AI Prompt-to-PBR + 2-Pass Cavity AO          │
│    • Interactive 3D Spherical-Falloff Region Brush (/api/mesh/paint-region)│
└───────┬────────────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────────────┐
│ 5. @rig-kinematics & @pro-exporters (rig_animator.hpp + exporters_pro.hpp) │
│    • Volumetric 4-Bone (JOINTS_0 / WEIGHTS_0) Bipedal & Quadruped Skinning │
│    • 24 Embedded Procedural Motion Clips (Idle, Walk, Combat, Dance, Fly)  │
│    • Exports: .FBX (7.4), .USDZ (64-Byte Aligned), .GLB, .OBJ, .3MF, .STL  │
│    • 1-Click Slicer URI Bridge + 5 Zero-Python Game Engine/DCC Plugins     │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core C++20 Header Map (`cpp_engine/include/`)

| Header / Source | Responsibility & Key Algorithms |
| :--- | :--- |
| `http_server.hpp` & `json.hpp` | Dual-platform (`POSIX` + `Winsock2`) multi-threaded HTTP/1.1 server & recursive-descent JSON parser/serializer with zero external library bloat. |
| `hardware.hpp` | Probes logical CPU threads, system RAM, discrete/integrated GPUs, and live localhost AI ports (`:11434`, `:1234`, `:8080`, `:8001`, `:8188`) to auto-tune SDF voxel resolution (`72³`–`136³`) even with **0 GPU**. |
| `provider_catalog.hpp` & `llm_router.hpp` | Manages `98` cataloged providers (`68` free/local + `30` opt-in paid + unlimited custom `/v1` OpenAI-compatible endpoints) and enforces exact word-boundary category routing. |
| `agent_swarm.hpp` | Orchestrates the 6-agent 3D pipeline (`@lead-architect`, `@kitbash-detailer`, `@sculptor-csg`, `@material-pbr`, `@rig-kinematics`, `@qa-inspector`) and category-safe Ultra-Kitbash detailing. |
| `sdf_compiler.hpp` & `marching_cubes.hpp` | Evaluates analytical & smooth-Boolean CSG Signed Distance Fields across OpenMP threads, extracts closed 2-manifold isosurfaces via Surface-Nets, and applies Taubin $\lambda/\mu$ smoothing. |
| `neural_organic.hpp` | Evaluates smooth `C²` sinusoidal representation (`SIREN`) harmonic fields blended with anatomical metaballs for organic characters, creatures, and museum portrait busts. |
| `multiview_3d.hpp` & `image_to_3d.hpp` | Reconstructs 3D meshes from `1–4` reference views (`Front`, `Side`, `Back`, `Top`) using orthographic visual-hull carving and normal-weighted RGB texture projection. |
| `mesh_importer.hpp` | Parses external `.GLB` (glTF 2.0 binary), `.OBJ`, binary/ASCII `.STL`, and `.PLY` meshes into Falcon's unified `SceneMesh` structure for retexturing, quad remeshing, or rigging. |
| `smart_remesh.hpp` | Implements **Garland-Heckbert `4×4` Quadric Error Metrics (`QEM`)** with priority-heap edge collapses, normal-inversion guards, and coplanar triangle pairing into true 4-gon quads (`QuadMeshData`). |
| `pbr_textures.hpp` | Synthesizes embedded PNG Albedo, Occlusion-Roughness-Metallic (`ORM`), and Tangent-Space Normal maps up to **`4096×4096` (`4K Ultra`)** + localized 3D spherical-falloff region painting. |
| `rig_animator.hpp` | Computes volumetric 4-bone (`JOINTS_0` / normalized `WEIGHTS_0`) skin weights for bipedal/quadruped templates and embeds **`24` categorized keyframe animation clips** directly into glTF 2.0 / `.FBX`. |
| `exporters.hpp` & `exporters_pro.hpp` | Emits spec-compliant binary `.GLB` (normalized `COLOR_0` & `WEIGHTS_0`), Autodesk `.FBX` `7.4` (`Skin` + `Cluster` + `AnimationStack`), **64-byte aligned uncompressed `.USDZ`** for Apple Vision Pro / iOS AR, quad-preserving `.OBJ`, `.3MF`, `.STL`, `.PLY`, Unity `.CS`, and Slicer URI descriptors. |
| `game_engine_builder.hpp` | 6-agent 3D game world synthesizer producing playable 60-FPS browser/desktop games (`action_rpg`, `mecha_shooter`, `hypercar_racing`, `space_dogfight`) with live prompt mutation and standalone `.HTML` / `.CS` export. |
| `src/falcon_dcc_bridge.cpp` | Standalone compiled C++20 watch-folder hot-sync daemon (`bin/falcon_dcc_bridge[.exe]`) connecting Blender, Maya, Cinema 4D, and Houdini with **zero Python scripts**. |

---

## 3. Dual-Mode Electron Desktop Shell & WebGL 3D Laser-Sintering `Setup.exe`

- **Single Portable/Setup Executable (`Falcon-AI-Setup-3.0.0-x64.exe`)**:
  - On first launch (or `--setup`), opens `desktop/installer/setup.html` inside a frameless **`1080×680` OpenCode Studio window**.
  - Renders a **zero-dependency WebGL Clearcoat PBR + Z-Plane Laser-Sintering Shader** (`GT Hypercar`, `Cyber Mecha`, `SIREN Bust`, `Valkyrie Starfighter`) that solidifies in real time from `Y = 0mm` to `100%` as `desktop/electron/installer_engine.js` deploys `falcon_server.exe`, `falcon_dcc_bridge.exe`, AI provider state, starter `.GLB`s, playable 3D games, shortcuts, and a clean 1-click uninstaller.
  - Supports **1-Click Portable Mode** (zero file copy) and **Initial Workspace Selection** (`3D Model Studio`, `Playable 3D Game Studio`, or `AI Providers & Ollama`).
