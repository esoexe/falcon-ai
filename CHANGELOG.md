# Changelog

All notable changes to **Falcon AI** are documented in this file.

## [2.1.0] - 2026-10-29

### Added
- **Minimalist OpenCode Desktop UI/UX (`frontend/src/App.jsx`, `frontend/src/styles.css`)**:
  - Redesigned the entire Studio interface around OpenCode Desktop's calm, distraction-free monochrome layout (`#121010` warm obsidian dark & `#fcfafa` warm paper light themes).
  - Clean 2-pane default workspace: left session rail (`3D Models` / `3D Games`, `+ New`, active model/router dock) + full-bleed 3D/Game canvas + centered floating **OpenCode Prompt Composer** (`Build / Edit / Game / Plan` mode pill, live `Provider / Router` picker pill, `+ Image` 1-click upload, quality selector, and `/` slash commands).
  - Converted the Studio Inspector (`Parts`, `Material`, `Image/Sketch`, `Print/CAD`, `Rig`, `Swarm`, `JSON`) into an on-demand slide-in drawer (`Ctrl+I`, closed by default) and consolidated exports into a single `Export ▾` menu (`.glb`, `.3mf`, `.stl`, `.obj`, `.cs`, `.html`).
- **Universal Provider & Custom Endpoint Settings (`HardwareModal.jsx` & `provider_catalog.hpp`)**:
  - Dedicated 2-column **Settings & Providers** modal (`Ctrl+,`, `/providers`, sidebar dock, or composer pill).
  - **Custom OpenAI-Compatible Providers (`add_custom` / `delete_custom`)**: Add unlimited custom `/v1` endpoints (`Name`, `Base URL`, `API Key`, `Model ID`) persisted in `workspace_providers.json` and prioritized in multi-agent routing.
  - **1-Click Quick Connect & Searchable 98-Provider Pool**: Inline API key + editable model ID setup across `68` free providers (`5` No-Key, `52` Free-Key, `11` Local) and `30` opt-in paid providers.
  - **Local AI Runtime Hub**: Dedicated local configuration & live port scanner for `Ollama` (`:11434`), `LM Studio` (`:1234`), `llama.cpp` (`:8080`), `vLLM` (`:8001`), and `ComfyUI` (`:8188`).
- **Updated Windows Installer (`Falcon-AI-Setup-2.1.0-x64.exe`)**:
  - Updated live-animated 60-FPS 3D Hologram Setup Wizard and bundled static Windows x64 `falcon_server.exe` + embedded `v2.1` OpenCode Web VFS.

## [2.0.0] - 2026-10-28

### Added
- **6-Agent Autonomous 3D Swarm (`agent_swarm.hpp`)**: Collaborating `@lead-architect`, `@kitbash-detailer`, `@sculptor-csg`, `@material-pbr`, `@rig-kinematics`, and `@qa-inspector` pipeline with per-agent execution trace and dynamic natural-language kitbashing.
- **Playable 60-FPS 3D Game Creator Engine (`GameViewport.jsx` & `game_engine_builder.hpp`)**: In-app 3D game creation, live gameplay (`action_rpg`, `mecha_shooter`, `hypercar_racing`, `space_dogfight`), live world mutation, standalone `.HTML` game export, and Unity C# (`.cs`) export.
- **98-Provider AI Registry (`provider_catalog.hpp`)**: 68 free providers (`5` No-Key Instant-On, `52` Free-Key, `11` Local On-Computer) + `30` Paid providers (strictly OFF by default).
- **Embedded 256×256 glTF 2.0 PBR Textures (`pbr_textures.hpp`)**: Native C++20 Albedo, ORM, and Tangent-Space Normal synthesis across 13 material finishes + 2-pass smoothed Laplacian cavity AO.
- **Pure Dual-Platform C++20 Backend (`bin/falcon_server` & `bin/falcon_server.exe`)**: Native Linux POSIX + static Windows x64 (`Winsock2`, `WinHTTP` TLS, `GDI+`, static `zlib` PNG encoder, and `OpenMP` parallel Surface-Nets) with zero Python and zero Docker dependencies.
- **Word-Boundary Prompt Router & Keyless Cloud AI Blueprint Synthesizer (`llm_router.hpp`)**: Exact word-boundary matching, sculpted vehicle/creature/prop archetypes, and zero-key cloud AI blueprint synthesis.
