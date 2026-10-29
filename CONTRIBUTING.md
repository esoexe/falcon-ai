# Contributing to Falcon AI

Thank you for contributing to **Falcon AI**! Our core engineering principles keep Falcon fast, accessible on zero-GPU hardware, and completely free:

1. **Zero Python & Zero Container Bloat**: All backend geometry, HTTP routing, image-to-3D EDT inflation, and PBR PNG texture synthesis are written in standard **C++20** (`cpp_engine/include/`).
2. **100% Watertight 2-Manifold Meshes**: Any new procedural or SDF primitive added to `sdf_compiler.hpp` must produce closed, outward-oriented triangle winding so `.3MF` and `.STL` 3D print exports pass manifold verification.
3. **OpenCode Design Fidelity**: UI components in `frontend/src/` use monospace typography (`IBM Plex Mono`), `1px` hairline borders, max `4px` radius, and zero drop-shadows or decorative gradients.
4. **Free-First AI Routing**: Paid providers in `provider_catalog.hpp` must always default to `default_enabled = false`.

---

## Adding New Features

### Adding a New C++20 Kitbash Primitive
1. Implement `inline Mesh make_<name>_mesh(...)` in `cpp_engine/include/sdf_compiler.hpp`.
2. Register the primitive string in `build_part_primitive(...)`.
3. Optionally expose a trigger rule in `apply_ultra_kitbash_details(...)` inside `cpp_engine/include/agent_swarm.hpp` and a quick chip in `frontend/src/components/SwarmPanel.jsx`.

### Adding a Procedural PBR Material Finish
1. Add your finish branch to `build_finish_textures(...)` in `cpp_engine/include/pbr_textures.hpp` (populating `albedo`, `orm`, and `height_map` for Sobel tangent-space normal generation).
2. Add the `<option>` to the PBR finish selector in `frontend/src/App.jsx`.

### Adding a Free AI Provider
1. Append an `AIProviderSpec` entry in `cpp_engine/include/provider_catalog.hpp` under `"no_key"`, `"free_key"`, or `"local"`.
2. Document its free rate limit and signup domain in `FREE_AI_PROVIDERS_CATALOG.md`.

---

## Pull Request Checklist
- [ ] `cd frontend && npm run build && cd .. && make -j2` compiles with **0 warnings / 0 errors** (`-Wall -Wextra`).
- [ ] Generated `.GLB` models report `watertight: true` via `/api/print/repair`.
- [ ] Works cleanly on CPU-only (`0 GB VRAM`) environments.
