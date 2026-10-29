# Changelog

All notable changes to **Falcon AI Studio** are documented in this file.

## [4.0.0] - 2026-10-29

### Added — Pure C++20 Neural 3D Diffusion Bridge + CGTrader Corpus-Trained Catmull-Clark Master Sculptor
- **Pure C++20 Neural 3D Diffusion Bridge (`cpp_engine/include/neural_3d_bridge.hpp`)**:
  - Added cross-platform `libcurl` (Linux) & `WinHTTP` (Windows) Gradio v4/v5 SSE client connecting directly to **Tencent Hunyuan3D-2.0**, **VAST-AI TripoSG 1.5B**, **Stability AI Stable Fast 3D (`SF3D`)**, and local `:7860`/`:8188` neural 3D servers, with automatic zero-key studio concept generation and seamless failover to the CGTrader-Trained Catmull-Clark Master Sculptor.
- **Pure C++20 CGTrader Marketplace Corpus Analyzer & 250-Epoch Prior Trainer (`bin/falcon_corpus_trainer[.exe]`)**:
  - Added `data/cgtrader_marketplace_corpus.json` (`12` CGTrader `CGT Standard` categories) + `cpp_engine/src/falcon_corpus_trainer.cpp` streaming and analyzing `8` open-licensed `.GLB` reference models in RAM (`143,428` vertices, `193,956` triangles; **`0` mesh bytes persisted or copied**), converging 250 epochs (`Loss: 0.3147 -> 0.0185`) into `cpp_engine/include/cgtrader_learned_priors.hpp` and `data/falcon_cgtrader_learned_weights.json`.
- **True Catmull-Clark Quad Subdivision Master Sculptor (`cpp_engine/include/subdiv_sculptor.hpp`)**:
  - Implemented `catmull_clark_step` with DeRose semi-sharp edge creases (`0.0`–`1.0`) and 100% pure 4-gon quad cages (`0` N-gons, 2-manifold watertight) across `character_wizard_robed` (`22,144` quads), `character_knight_warrior` (`12,608` quads), `creature_dragon_wyvern` (`16,608` quads), `car_supercar_gt` (`16,424` quads), and `furniture_designer_chair` (`2,432` quads), plus new `automotive_clearcoat`, `satin_plate`, and `tinted_glass` PBR finishes.
- **OpenCode Studio v4.0 Neural Selector & Inspector Priors Telemetry (`frontend/src/App.jsx`, `SwarmPanel.jsx`)**:
  - Added live engine mode selector (`Auto (CGT + Swarm)`, `CGT Master Subdiv`, `Hunyuan3D-2 Neural`, `TripoSG 1.5B Neural`, `SF3D Quad Neural`) and `GET /api/neural/status` + `POST /api/neural/train-priors` live re-calibration in the Inspector.

---

## [3.0.0] - 2026-10-29

### Added — Rebuilt Live WebGL 3D Laser-Sintering `Setup.exe` UI/UX
- **Per-Pixel WebGL 3D Clearcoat PBR & Laser-Sintering Viewport (`desktop/installer/setup.html`)**:
  - Rebuilt the Windows/Desktop installer from the ground up in the **OpenCode `v3.0` Obsidian & Daylight** design language (`1080×680` frameless window, zero scrollbars, zero text clipping).
  - Embedded a zero-dependency **WebGL 3D fragment shader** featuring 4 smooth high-poly sculpted showcase meshes (`Aero Widebody GT Hypercar`, `Articulated Cyber Mecha`, `SIREN Anatomical Bust`, and `Valkyrie Starfighter`), drag-to-orbit + scroll-to-zoom camera, `Laser` / `Quad` / `Solid` shading modes, and Dark/Daylight theme toggle.
  - **Live Z-Slice Materialization**: Synchronizes the 3D laser scan plane (`uLaserY`) directly with `installer_engine.js` extraction progress (`VFS`, `CORE`, `AI-POOL`, `PLUGINS`, `3D-PACK`, `SHORTCUT`), plus **Initial Workspace View** handoff (`/?launch=model|game|providers`) and **1-Click Portable Mode**.

### Added — Full 7-Pillar Meshy AI Parity Engine (`C++20` + `WebGL2`)
- **4-Candidate Parallel Fast Draft Grid & Implicit `SIREN` Organic Sculptor (`neural_organic.hpp`)**:
  - `POST /api/generate/draft-grid` generates 4 varied candidate previews in `~670 ms` with 1-click `Refine Ultra` promotion.
  - Added smooth `C²` sinusoidal harmonic + anatomical metaball sculpting for organic busts, characters, and creatures.
- **1–4 Photo Multi-View Visual Hull & External Mesh Importer (`multiview_3d.hpp`, `mesh_importer.hpp`)**:
  - `POST /api/generate/multiview` carves 3D geometry from `Front`, `Side`, `Back`, and `Top` photos with normal-weighted RGB projection.
  - `POST /api/mesh/import` imports external `.GLB`, `.OBJ`, `.STL`, and `.PLY` meshes for retexturing, quad remeshing, or auto-rigging.
- **Garland-Heckbert `4×4` QEM Smart Quad Remesher (`smart_remesh.hpp`)**:
  - `POST /api/mesh/remesh` decimates or retopologizes meshes anywhere from `100` to `300,000` target faces and extracts true 4-gon quad topology (`QuadMeshData` + quad-preserving `.OBJ` export).
- **`4K Ultra` OpenMP PBR Texture Baker & Live 3D Region Brush (`pbr_textures.hpp`)**:
  - Upgraded procedural PBR baking to `256`, `1024 (1K)`, `2048 (2K HD)`, and `4096 (4K Ultra)` Albedo/ORM/Tangent-Normal maps + AI Prompt-to-PBR (`POST /api/mesh/retexture`) and interactive 3D spherical-falloff region healing (`POST /api/mesh/paint-region`).
- **Volumetric 4-Bone Auto-Rigger & 24-Clip Motion Library (`rig_animator.hpp`)**:
  - `POST /api/mesh/rig-animate` computes normalized `JOINTS_0` / `WEIGHTS_0` skinning across 17 joints (bipedal & quadruped) and embeds `24` categorized motion clips (`Locomotion`, `Combat`, `Emote`, `Aerial`) playable directly inside `Viewport3D.jsx`.
- **Autodesk `.FBX` 7.4, 64-Byte Aligned Apple `.USDZ`, Slicer Bridge & 5 Zero-Python Engine Plugins (`exporters_pro.hpp`, `plugins/`)**:
  - Added native `.FBX` 7.4 (`Skin` + `AnimationStack`), VisionOS/iOS 64-byte aligned `.USDZ`, 1-click URI launch for 6 slicers (`Bambu`, `Orca`, `Prusa`, `Cura`, `Elegoo`, `Creality`), and 5 official Zero-Python Live-Link plugins (`Unity C#`, `UE5 C++`, `Godot 4 GDScript`, `Roblox Luau`, and compiled `bin/falcon_dcc_bridge[.exe]`).

---

## [2.1.0] - 2026-10-29

### Added
- **Minimalist OpenCode Desktop UI/UX (`frontend/src/App.jsx`, `frontend/src/styles.css`)**:
  - Redesigned the Studio interface around OpenCode Desktop's calm monochrome layout, on-demand slide-in Inspector drawer (`Ctrl+I`), and floating Prompt Composer.
- **Universal Provider & Custom Endpoint Settings (`HardwareModal.jsx` & `provider_catalog.hpp`)**:
  - Unified `Settings & Providers` modal (`Ctrl+,`) supporting `68` free/local providers, `30` opt-in paid providers, unlimited custom `/v1` OpenAI-compatible endpoints, and live local port scanning (`Ollama`, `LM Studio`, `llama.cpp`, `vLLM`, `ComfyUI`).

---

## [2.0.0] - 2026-10-28

### Added
- **6-Agent Autonomous 3D Swarm (`agent_swarm.hpp`)** & **Playable 60-FPS 3D Game Creator (`game_engine_builder.hpp`)**.
- **Pure Dual-Platform C++20 Backend (`bin/falcon_server` & `bin/falcon_server.exe`)** with zero Python and zero Docker dependencies.
