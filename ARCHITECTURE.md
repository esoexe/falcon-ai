# Falcon AI v2.0 — System Architecture

Falcon AI replaces Python inference pipelines and container orchestration with a **single compiled C++20 binary (`falcon_server`)** paired with a **Three.js WebGL2 Studio & 60-FPS Game Engine** and a **Dual-Mode Live-Animated Electron Desktop Shell**.

---

## 1. Pure C++20 Multi-Agent 3D Synthesis Pipeline

```text
User Prompt / Sketch / Blueprint
        │
        ▼
┌────────────────────────────────────────────────────────────────────┐
│ @lead-architect (llm_router.hpp + provider_catalog.hpp)            │
│ Routes across Local LLMs / 68 Free Providers / C++20 Architect     │
└───────┬────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────┐
│ @kitbash-detailer (agent_swarm.hpp)                                │
│ Attaches category-matched or user-requested sub-assemblies         │
│ (Picatinny rails, hydraulic pistons, Aegis shields, crystal spires)│
└───────┬────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────┐
│ @sculptor-csg (sdf_compiler.hpp + marching_cubes.hpp)              │
│ Parallel OpenMP 104³–136³ SDF evaluation, smooth Boolean CSG,      │
│ Surface-Nets extraction & 6-pass volume-preserving Taubin filter   │
└───────┬────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────┐
│ @material-pbr (pbr_textures.hpp + exporters.hpp)                   │
│ Synthesizes 256×256 PNG Albedo, ORM & Tangent-Space Normal maps    │
│ + 2-pass smoothed Laplacian cavity AO into binary glTF 2.0 (.GLB)  │
└───────┬────────────────────────────────────────────────────────────┘
        ▼
┌────────────────────────────────────────────────────────────────────┐
│ @rig-kinematics & @qa-inspector (mesh.hpp)                         │
│ 15-bone joint hierarchy + 2-manifold watertight verification       │
└────────────────────────────────────────────────────────────────────┘
```

## 2. Watertightness & Sharp Hard-Surface Shading Invariant

Slicers (`.3MF` / `.STL` for Bambu Studio, PrusaSlicer, OrcaSlicer) require strictly welded, closed 2-manifold meshes (`watertight: true`), whereas real-time PBR rendering (`.GLB` in WebGL2 / Unity / Unreal) requires split per-face UVs and crease normals along sharp bevels.

Falcon AI achieves **both simultaneously**:
1. All internal C++20 `Mesh` primitives (`sdf_compiler.hpp`, `marching_cubes.hpp`) clamp outer voxel boundaries (`> 0`) and maintain welded vertex topology.
2. `export_glb_bytes()` (`exporters.hpp`) splits `TEXCOORD_0` box-projected UVs and `> 70°` hard creases (`dot < 0.34`) **only inside the serialized `.GLB` stream**.
3. `load_glb_bytes()` runs spatial hash welding (`weld_vertices(1e-5f)`) when reading `.GLB` files back for CAD millimeter scaling, Boolean pin-splitting, or `.3MF`/`.STL` export.

## 3. Playable 60-FPS 3D Game Creator (`game_engine_builder.hpp` + `GameViewport.jsx`)

- Casts actual workspace `.GLB` models (title-first matching with automatic 6-Agent Swarm synthesis fallback) into Hero, Enemy Swarm, Sector Boss, Architectural Landmarks, and Pickups.
- Automatically detects Zero-GPU / software WebGL rasterizers (`SwiftShader`, `llvmpipe`, `Basic Render`) and adapts shadow-map / pixel-ratio settings for smooth frame rates on any laptop.
- Exports self-contained single-file HTML5 games (`GET /api/games/{id}/export/html`) by Base64-embedding every `.GLB` asset directly into the exported `.html` document.

## 4. Live-Animated 3D Setup Engine (`desktop/installer/setup.html` + `installer_engine.js`)

Instead of legacy static Win32 NSIS dialogs, `Falcon-AI-Setup-2.0.0-x64.exe` launches a frameless `1040×650` OpenCode window driven by a zero-dependency **60-FPS 3D Hologram & Laser-Slicer Canvas**:
- As `installer_engine.js` unpacks the runtime to `%LOCALAPPDATA%\Programs\FalconAI`, seeds `.GLB` showcase assets, configures the 68 free AI providers, writes `.lnk` shortcuts via `WScript.Shell`, and registers a clean user-space uninstaller in `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\FalconAI`, the 3D hologram laser-slices upward from `0% -> 100%`, converting cyan wireframe facets into solid lit geometry in real time.
