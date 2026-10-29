# Changelog

All notable changes to **Falcon AI** are documented in this file.

## [2.0.0] - 2026-10-28

### Added
- **6-Agent Autonomous 3D Swarm (`agent_swarm.hpp`)**: Collaborating `@lead-architect`, `@kitbash-detailer`, `@sculptor-csg`, `@material-pbr`, `@rig-kinematics`, and `@qa-inspector` pipeline with per-agent execution trace and dynamic natural-language "add anything needed" kitbashing.
- **Playable 60-FPS 3D Game Creator Engine (`GameViewport.jsx` & `game_engine_builder.hpp`)**: In-app 3D game creation, live gameplay (`action_rpg`, `mecha_shooter`, `hypercar_racing`, `space_dogfight`), live world mutation (AI turrets, bosses, ramps, storm rain), standalone Base64-embedded `.HTML` game export, and Unity C# (`.cs`) export.
- **98-Provider AI Registry (`provider_catalog.hpp`)**: 68 free providers (`5` No-Key Instant-On, `52` Free-Key, `11` Local On-Computer) + `30` Paid providers (strictly OFF by default).
- **Embedded 256×256 glTF 2.0 PBR Textures (`pbr_textures.hpp`)**: Native C++20 `libpng` Albedo, ORM, and Tangent-Space Normal synthesis across 13 material finishes + 2-pass smoothed Laplacian cavity AO.
- **Live-Animated 3D Holographic Windows Installer (`Falcon-AI-Setup-2.0.0-x64.exe`)**: Frameless 60-FPS interactive 3D hologram & laser-slicer setup wizard with hardware auto-detection, module selection, Desktop/Start Menu `.lnk` creation, clean HKCU uninstaller registration, and instant Portable Mode.
- **Pure C++20 Backend & Zero-Container Daemon (`falcon_server`, `falcon_ctl`)**: Complete removal of Python and container dependencies in favor of compiled C++20 OpenMP geometry synthesis and zlib-9 embedded WebGL VFS.
