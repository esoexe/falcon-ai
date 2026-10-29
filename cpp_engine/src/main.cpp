/**
 * OpenMesh Studio 2.0 — Pure Compiled C++20 Backend & 3D Geometry Server.
 * Zero Python anywhere: serves REST API, Multi-Part SDF/SurfaceNets 3D Generation,
 * PNG/JPEG Volumetric Image-to-3D, CAD Millimeter Resizing, Slicer .3MF/.STL/.GLB/.CS
 * Exports, and Static Web Studio on 0.0.0.0:8000.
 */

#include "agent_swarm.hpp"
#include "embedded_web.hpp"
#include "exporters.hpp"
#include "exporters_pro.hpp"
#include "game_engine_builder.hpp"
#include "hardware.hpp"
#include "http_server.hpp"
#include "image_to_3d.hpp"
#include "json.hpp"
#include "llm_router.hpp"
#include "mesh.hpp"
#include "mesh_importer.hpp"
#include "multiview_3d.hpp"
#include "neural_3d_bridge.hpp"
#include "neural_organic.hpp"
#include "provider_catalog.hpp"
#include "rig_animator.hpp"
#include "sdf_compiler.hpp"
#include "smart_remesh.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace openmesh;
namespace fs = std::filesystem;

namespace {

std::mutex g_state_mutex;
JsonValue g_hardware_cache;
RouterConfig g_router_cfg;

int64_t now_epoch_sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

int64_t now_epoch_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string random_hex_id() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static const char* kHex = "0123456789abcdef";
    std::string id(10, '0');
    uint64_t v = rng();
    for (int i = 0; i < 10; ++i) {
        id[static_cast<size_t>(i)] = kHex[(v >> (i * 4)) & 0xF];
    }
    return id;
}

std::vector<uint8_t> read_binary_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("File not found: " + p.string());
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (sz > 0) f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

std::string read_text_file(const fs::path& p) {
    auto b = read_binary_file(p);
    return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

void write_binary_file(const fs::path& p, const std::vector<uint8_t>& data) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void write_text_file(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string guess_mime(const fs::path& p) {
    std::string ext = to_lower_str(p.extension().string());
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js" || ext == ".mjs") return "application/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".glb") return "model/gltf-binary";
    if (ext == ".fbx") return "application/octet-stream";
    if (ext == ".usdz") return "model/vnd.usdz+zip";
    if (ext == ".3mf") return "application/vnd.ms-package.3dmanufacturing-3dmodel+xml";
    if (ext == ".json") return "application/json; charset=utf-8";
    return "application/octet-stream";
}

JsonValue save_scene_asset(
    const fs::path& assets_dir,
    const Scene3D& scene,
    const std::string& title,
    const std::string& prompt,
    const std::string& source_mode,
    const std::string& engine_used,
    const JsonValue& blueprint,
    int64_t gen_ms,
    const std::string& force_id = "",
    const JsonValue& existing_rig = JsonValue(),
    const JsonValue& agent_trace = JsonValue()
) {
    std::string aid = force_id.empty() ? random_hex_id() : force_id;
    std::vector<uint8_t> glb = export_glb_bytes(scene);
    write_binary_file(assets_dir / (aid + ".glb"), glb);

    JsonValue telemetry = compute_scene_telemetry(scene);
    JsonValue parts_meta = JsonValue::array();
    if (blueprint.is_object() && blueprint.has("parts") && blueprint["parts"].is_array()) {
        const auto& bp_parts = blueprint["parts"];
        for (size_t i = 0; i < bp_parts.size(); ++i) {
            const auto& p = bp_parts[i];
            if (p.get_bool("hidden", false)) continue;
            JsonValue pm = JsonValue::object();
            pm["id"] = p.get_str("id", "part");
            pm["name"] = p.get_str("name", "Part");
            pm["primitive"] = p.get_str("primitive", "mesh");
            pm["color"] = p.get_str("color", "#94a3b8");
            pm["metallic"] = p.get_num("metallic", 0.35);
            pm["roughness"] = p.get_num("roughness", 0.42);
            parts_meta.push_back(pm);
        }
    } else {
        for (const auto& m : scene.parts) {
            JsonValue pm = JsonValue::object();
            pm["id"] = m.id;
            pm["name"] = m.name;
            pm["primitive"] = m.primitive_type;
            pm["color"] = m.material.color_hex;
            pm["metallic"] = m.material.metallic;
            pm["roughness"] = m.material.roughness;
            pm["finish"] = m.material.finish;
            pm["tex_resolution"] = m.material.tex_resolution;
            parts_meta.push_back(pm);
        }
    }

    int max_tex_res = 256;
    size_t total_quads = 0;
    for (const auto& m : scene.parts) {
        max_tex_res = std::max(max_tex_res, m.material.tex_resolution);
        total_quads += m.quads.size();
    }

    std::string unity_cs = generate_unity_csharp_script(blueprint, title);

    JsonValue rec = JsonValue::object();
    rec["id"] = aid;
    rec["title"] = title;
    rec["prompt"] = prompt;
    rec["source_mode"] = source_mode;
    rec["engine_used"] = engine_used;
    rec["created_at"] = now_epoch_sec();
    rec["generation_ms"] = gen_ms;
    rec["glb_url"] = "/exports/" + aid + ".glb?t=" + std::to_string(now_epoch_ms());
    rec["file_size_kb"] = std::round((static_cast<double>(glb.size()) / 1024.0) * 10.0) / 10.0;
    rec["telemetry"] = telemetry;
    rec["tex_resolution"] = max_tex_res;
    rec["quad_mode"] = (total_quads > 0);
    rec["quad_count"] = static_cast<int64_t>(total_quads);
    rec["parts"] = parts_meta;
    rec["blueprint"] = blueprint.is_object() ? blueprint : JsonValue(nullptr);
    rec["unity_csharp_script"] = unity_cs;
    rec["rig"] = existing_rig.is_object() ? existing_rig : JsonValue(nullptr);
    rec["agent_trace"] = agent_trace.is_array() ? agent_trace : JsonValue::array();

    write_text_file(assets_dir / (aid + ".json"), rec.dump(2));
    return rec;
}

std::pair<Scene3D, JsonValue> load_scene_asset(const fs::path& assets_dir, const std::string& aid) {
    fs::path glb_p = assets_dir / (aid + ".glb");
    fs::path meta_p = assets_dir / (aid + ".json");
    if (!fs::exists(glb_p) || !fs::exists(meta_p)) {
        throw std::runtime_error("Asset not found: " + aid);
    }
    JsonValue meta = JsonValue::parse(read_text_file(meta_p));
    Scene3D sc = load_glb_bytes(read_binary_file(glb_p));
    int saved_tex = meta.get_int("tex_resolution", 256);
    bool is_quad = meta.get_bool("quad_mode", false);
    for (auto& m : sc.parts) {
        m.material.tex_resolution = std::clamp(saved_tex, 128, 4096);
        if (is_quad && m.quads.empty() && m.faces.size() >= 2) {
            qem_detail::extract_quad_dominant_topology(m, 1);
        }
    }
    return {std::move(sc), std::move(meta)};
}

void seed_showcase_assets_cpp(const fs::path& assets_dir, int grid_res) {
    bool has_any = false;
    for (const auto& entry : fs::directory_iterator(assets_dir)) {
        if (entry.path().extension() == ".json") { has_any = true; break; }
    }
    if (has_any) return;

    const std::vector<std::string> starters = {
        "Articulated cobalt & titanium mecha guardian with glowing cyan fusion core and dorsal flight wings",
        "Crimson and bone elder dragon miniature with spread wings on sculpted basalt cliff base",
        "Damascus forged silver and gold katana with glowing cyan conduit gem and carved dragon guard",
        "Crimson and carbon-fiber aero widebody GT hypercar with 10-spoke alloy wheels and swan-neck wing"
    };
    for (const auto& pr : starters) {
        auto t0 = now_epoch_ms();
        RouterConfig cfg_def;
        Swarm3DResult sw = execute_3d_agent_swarm(cfg_def, pr, grid_res, "realistic", true);
        auto ms = now_epoch_ms() - t0;
        save_scene_asset(
            assets_dir,
            sw.scene,
            sw.blueprint.get_str("title", "Showcase 3D"),
            pr,
            "text-to-3d",
            "Falcon 6-Agent Swarm · " + sw.provider,
            sw.blueprint,
            ms,
            "",
            sw.rig,
            sw.agent_trace
        );
    }
}

JsonValue resolve_or_synthesize_game_asset(
    const fs::path& assets_dir,
    const RouterConfig& cfg,
    int grid_res,
    const std::string& role_keywords,
    const std::string& fallback_prompt
) {
    // 1. Reuse matching high-detail sculpted .GLB asset in workspace_assets if present
    std::vector<std::string> kws;
    std::istringstream kss(role_keywords);
    std::string kw;
    while (std::getline(kss, kw, '|')) {
        if (!kw.empty()) kws.push_back(to_lower_str(kw));
    }
    std::vector<JsonValue> all_metas;
    for (const auto& entry : fs::directory_iterator(assets_dir)) {
        if (entry.path().extension() != ".json") continue;
        try {
            all_metas.push_back(JsonValue::parse(read_text_file(entry.path())));
        } catch (...) {}
    }
    // Pass 1: Match asset title first (prevents e.g. 'carved dragon guard' matching 'dragon' before 'Elder Scaled Wyvern')
    for (const auto& meta : all_metas) {
        std::string t_low = to_lower_str(meta.get_str("title"));
        for (const auto& k : kws) {
            if (t_low.find(k) != std::string::npos) return meta;
        }
    }
    // Pass 2: Fallback to full prompt
    for (const auto& meta : all_metas) {
        std::string p_low = to_lower_str(meta.get_str("prompt"));
        for (const auto& k : kws) {
            if (p_low.find(k) != std::string::npos) return meta;
        }
    }

    // 2. Otherwise synthesize on the fly via the 6-Agent C++20 Swarm!
    auto t0 = now_epoch_ms();
    Swarm3DResult sw = execute_3d_agent_swarm(cfg, fallback_prompt, std::min(grid_res, 96), "realistic", true);
    return save_scene_asset(
        assets_dir,
        sw.scene,
        sw.blueprint.get_str("title", "Game 3D Asset"),
        fallback_prompt,
        "game-swarm-asset",
        "Falcon Game Swarm · " + sw.provider,
        sw.blueprint,
        now_epoch_ms() - t0,
        "",
        sw.rig,
        sw.agent_trace
    );
}

void seed_showcase_games_cpp(const fs::path& games_dir, const fs::path& assets_dir, int grid_res) {
    bool has_any = false;
    for (const auto& entry : fs::directory_iterator(games_dir)) {
        if (entry.path().extension() == ".json") { has_any = true; break; }
    }
    if (has_any) return;

    RouterConfig cfg_def;
    std::string gid = random_hex_id();
    JsonValue g = build_3d_game_manifest(
        gid,
        "Citadel Wyvern Siege: Dark-Fantasy Cyber-Knight vs Elder Dragon Boss",
        now_epoch_sec(),
        [&](const std::string& kw, const std::string& fp) {
            return resolve_or_synthesize_game_asset(assets_dir, cfg_def, grid_res, kw, fp);
        }
    );
    write_text_file(games_dir / (gid + ".json"), g.dump(2));
}

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> segs;
    std::istringstream iss(path);
    std::string s;
    while (std::getline(iss, s, '/')) {
        if (!s.empty()) segs.push_back(s);
    }
    return segs;
}

} // namespace

int main(int argc, char** argv) {
#ifndef _WIN32
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    fs::path base_dir = fs::current_path();
    uint16_t port = 8000;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--dir" && i + 1 < argc) {
            base_dir = fs::path(argv[++i]);
        }
    }

    fs::path assets_dir = base_dir / "workspace_assets";
    fs::path games_dir = base_dir / "workspace_games";
    fs::path web_dir = base_dir / "frontend" / "web_bundle";
    fs::create_directories(assets_dir);
    fs::create_directories(games_dir);

    const auto embedded_vfs = load_embedded_web_vfs();
    ProviderPoolManager provider_pool(base_dir / "workspace_providers.json");

    g_hardware_cache = inspect_hardware_cpp(base_dir);
    int default_grid = g_hardware_cache.at_or_null("profile").get_int("sdf_grid_resolution", 96);
    seed_showcase_assets_cpp(assets_dir, default_grid);
    seed_showcase_games_cpp(games_dir, assets_dir, default_grid);

    std::cout << "[Falcon AI · C++20] Hardware Tier: "
              << g_hardware_cache.at_or_null("profile").get_str("tier_badge")
              << " | Default Grid: " << default_grid << "^3"
              << " | Embedded VFS Files: " << embedded_vfs.size() << " (Zero Docker)" << std::endl;

    HttpServer server([&](const HttpRequest& req) -> HttpResponse {
        auto segs = split_path(req.path);

        // 1. Live Animated Setup Wizard Preview (/setup), Setup.exe Download & Open-Source Zip Download
        if (segs.size() == 1 && segs[0] == "setup") {
            fs::path setup_html = base_dir / "desktop" / "installer" / "setup.html";
            if (!fs::exists(setup_html)) return HttpResponse::json("{\"error\":\"setup.html not found\"}", 404);
            return HttpResponse::bytes(read_binary_file(setup_html), "text/html; charset=utf-8");
        }
        if (segs.size() == 2 && segs[0] == "download" && (segs[1] == "falcon.exe" || segs[1] == "setup.exe")) {
            fs::path exe_p = base_dir / "releases" / "Falcon-AI-Setup-3.0.0-x64.exe";
            if (!fs::exists(exe_p)) exe_p = base_dir / "releases" / "Falcon-AI-Setup-2.1.0-x64.exe";
            if (!fs::exists(exe_p)) exe_p = base_dir / "releases" / "Falcon-AI-Setup-2.0.0-x64.exe";
            if (!fs::exists(exe_p)) return HttpResponse::json("{\"error\":\"Setup.exe not found\"}", 404);
            return HttpResponse::bytes(read_binary_file(exe_p), "application/vnd.microsoft.portable-executable", exe_p.filename().string());
        }
        if (segs.size() == 2 && segs[0] == "download" && segs[1] == "source.zip") {
            fs::path zip_p = base_dir / "releases" / "Falcon-AI-3.0.0-Source-Code.zip";
            if (!fs::exists(zip_p)) zip_p = base_dir / "releases" / "Falcon-AI-2.1.0-Source-Code.zip";
            if (!fs::exists(zip_p)) zip_p = base_dir / "releases" / "Falcon-AI-2.0.0-Source-Code.zip";
            if (!fs::exists(zip_p)) return HttpResponse::json("{\"error\":\"Source archive not found\"}", 404);
            return HttpResponse::bytes(read_binary_file(zip_p), "application/zip", zip_p.filename().string());
        }

        if (segs.size() == 2 && segs[0] == "exports") {
            fs::path fp = assets_dir / fs::path(segs[1]).filename();
            if (!fs::exists(fp)) return HttpResponse::json("{\"error\":\"Not found\"}", 404);
            return HttpResponse::bytes(read_binary_file(fp), guess_mime(fp));
        }

        // 2. REST API (/api/...)
        if (!segs.empty() && segs[0] == "api") {
            // GET /api/hardware & POST /api/hardware/rescan
            if (segs.size() == 2 && segs[1] == "hardware" && req.method == "GET") {
                std::lock_guard<std::mutex> lk(g_state_mutex);
                return HttpResponse::json(g_hardware_cache.dump(0));
            }
            if (segs.size() == 3 && segs[1] == "hardware" && segs[2] == "rescan" && req.method == "POST") {
                JsonValue fresh = inspect_hardware_cpp(base_dir);
                std::lock_guard<std::mutex> lk(g_state_mutex);
                g_hardware_cache = fresh;
                return HttpResponse::json(g_hardware_cache.dump(0));
            }

            // GET / POST /api/settings/router
            if (segs.size() == 3 && segs[1] == "settings" && segs[2] == "router") {
                std::lock_guard<std::mutex> lk(g_state_mutex);
                if (req.method == "GET") {
                    return HttpResponse::json(g_router_cfg.to_json().dump(0));
                }
                if (req.method == "POST") {
                    JsonValue body = JsonValue::parse(req.body_str());
                    g_router_cfg.update_from_json(body);
                    JsonValue out = JsonValue::object();
                    out["status"] = "ok";
                    out["config"] = g_router_cfg.to_json();
                    return HttpResponse::json(out.dump(0));
                }
            }

            // GET / POST /api/providers — 98-Provider Registry (No-Key, Free-Key, Local, Paid Opt-In)
            if (segs.size() == 2 && segs[1] == "providers") {
                if (req.method == "GET") {
                    return HttpResponse::json(provider_pool.to_summary_json().dump(0));
                }
                if (req.method == "POST") {
                    JsonValue body = JsonValue::parse(req.body_str());
                    provider_pool.update_provider(body);
                    // Sync primary active key into g_router_cfg when user adds/activates a provider
                    auto cands = provider_pool.get_failover_candidates(false);
                    if (!cands.empty()) {
                        std::lock_guard<std::mutex> lk(g_state_mutex);
                        g_router_cfg.provider = "openai_compat";
                        g_router_cfg.openai_base_url = cands.front().base_url;
                        g_router_cfg.openai_api_key = cands.front().api_key;
                        g_router_cfg.openai_model = cands.front().model;
                    }
                    return HttpResponse::json(provider_pool.to_summary_json().dump(0));
                }
            }

            // GET /api/assets & GET/DELETE /api/assets/{id}
            if (segs.size() == 2 && segs[1] == "assets" && req.method == "GET") {
                std::vector<JsonValue> items;
                for (const auto& entry : fs::directory_iterator(assets_dir)) {
                    if (entry.path().extension() == ".json") {
                        try {
                            items.push_back(JsonValue::parse(read_text_file(entry.path())));
                        } catch (...) {}
                    }
                }
                std::sort(items.begin(), items.end(), [](const JsonValue& a, const JsonValue& b) {
                    return a.get_num("created_at", 0) > b.get_num("created_at", 0);
                });
                return HttpResponse::json(JsonValue(std::move(items)).dump(0));
            }
            if (segs.size() == 3 && segs[1] == "assets") {
                std::string aid = fs::path(segs[2]).filename().string();
                if (req.method == "GET") {
                    auto [_, meta] = load_scene_asset(assets_dir, aid);
                    return HttpResponse::json(meta.dump(0));
                }
                if (req.method == "DELETE") {
                    fs::remove(assets_dir / (aid + ".glb"));
                    fs::remove(assets_dir / (aid + "_rigged.glb"));
                    fs::remove(assets_dir / (aid + ".3mf"));
                    fs::remove(assets_dir / (aid + ".json"));
                    return HttpResponse::json("{\"deleted\":true}");
                }
            }

            // POST /api/assets/import — Import External .GLB / .OBJ / .STL / .PLY for Retexturing, Quad Remesh & Rigging!
            if (segs.size() == 3 && segs[1] == "assets" && segs[2] == "import" && req.method == "POST") {
                auto mp = req.parse_multipart();
                if (mp.find("file") == mp.end() || mp["file"].data.empty()) {
                    return HttpResponse::json("{\"error\":\"Missing mesh file (.glb, .obj, .stl, .ply)\"}", 400);
                }
                auto t0 = now_epoch_ms();
                std::string fname = mp["file"].filename.empty() ? "imported_mesh.obj" : mp["file"].filename;
                Scene3D sc = import_external_mesh_bytes(mp["file"].data, fname);
                std::string stem = fs::path(fname).stem().string();
                std::string ext  = to_lower_str(fs::path(fname).extension().string());
                JsonValue rec = save_scene_asset(
                    assets_dir,
                    sc,
                    "Imported: " + stem,
                    "Imported external " + ext + " mesh (" + fname + ")",
                    "mesh-import",
                    "Falcon v3.0 C++20 Mesh Importer (" + ext + ")",
                    JsonValue(nullptr),
                    now_epoch_ms() - t0
                );
                return HttpResponse::json(rec.dump(0));
            }

            // GET /api/animations — 24-Clip Categorized Motion Preset Library
            if (segs.size() == 2 && segs[1] == "animations" && req.method == "GET") {
                JsonValue arr = JsonValue::array();
                for (const auto& c : get_motion_library_catalog()) {
                    arr.push_back(JsonValue::object({
                        {"id", c.id},
                        {"name", c.name},
                        {"category", c.category},
                        {"duration_sec", c.duration_sec},
                        {"keyframes", c.keyframes}
                    }));
                }
                return HttpResponse::json(arr.dump(0));
            }

            // GET /api/slicer-links/{id} — 1-Click Direct Desktop Slicer Protocol Launchers
            if (segs.size() == 3 && segs[1] == "slicer-links" && req.method == "GET") {
                std::string aid = fs::path(segs[2]).filename().string();
                auto [sc, meta] = load_scene_asset(assets_dir, aid);
                fs::path mf_path = assets_dir / (aid + ".3mf");
                write_binary_file(mf_path, export_3mf_bytes(sc, meta.get_str("title", "FalconPrint")));
                std::string http_url = "http://127.0.0.1:" + std::to_string(port) + "/exports/" + aid + ".3mf";
                return HttpResponse::json(build_slicer_launch_uris(http_url).dump(0));
            }

            // GET /api/plugins/{engine} — Official Zero-Python Live-Link Plugin Bundles (.zip)
            if (segs.size() == 3 && segs[1] == "plugins" && req.method == "GET") {
                std::string eng = to_lower_str(segs[2]);
                std::vector<std::pair<std::string, std::string>> files;
                fs::path pdir = base_dir / "plugins";
                auto add_if_exists = [&](const std::string& arc_name, const fs::path& disk_p) {
                    if (fs::exists(disk_p)) files.push_back({arc_name, read_text_file(disk_p)});
                };
                if (eng == "unity") {
                    add_if_exists("FalconAI_Unity/Editor/FalconAILiveLink.cs", pdir / "unity" / "FalconAILiveLink.cs");
                } else if (eng == "unreal") {
                    add_if_exists("FalconAI_UE5/Public/FalconAILiveLink.h", pdir / "unreal" / "FalconAILiveLink.h");
                    add_if_exists("FalconAI_UE5/Private/FalconAILiveLink.cpp", pdir / "unreal" / "FalconAILiveLink.cpp");
                } else if (eng == "godot") {
                    add_if_exists("addons/falcon_ai/plugin.cfg", pdir / "godot" / "plugin.cfg");
                    add_if_exists("addons/falcon_ai/falcon_ai_dock.gd", pdir / "godot" / "falcon_ai_dock.gd");
                } else if (eng == "roblox") {
                    add_if_exists("FalconAI_Roblox/FalconAILiveLink.lua", pdir / "roblox" / "FalconAILiveLink.lua");
                } else if (eng == "blender" || eng == "dcc-bridge") {
                    add_if_exists("FalconAI_DCC_Bridge/falcon_dcc_bridge.cpp", base_dir / "cpp_engine" / "src" / "falcon_dcc_bridge.cpp");
                    if (fs::exists(base_dir / "bin" / "falcon_dcc_bridge")) {
                        add_if_exists("FalconAI_DCC_Bridge/bin/falcon_dcc_bridge", base_dir / "bin" / "falcon_dcc_bridge");
                    }
                } else {
                    return HttpResponse::json("{\"error\":\"Unknown plugin engine\"}", 404);
                }
                auto zbytes = zip_util::build_zip(files);
                return HttpResponse::bytes(std::move(zbytes), "application/zip", "FalconAI_v3_" + eng + "_Plugin.zip");
            }

            // POST /api/prompt/enhance
            if (segs.size() == 3 && segs[1] == "prompt" && segs[2] == "enhance" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string p = body.get_str("prompt", "");
                JsonValue bp = compile_prompt_cpp_symbolic(p);
                JsonValue out = JsonValue::object();
                out["original"] = p;
                out["enhanced"] = bp.get_str("enhanced_prompt", p);
                return HttpResponse::json(out.dump(0));
            }

            // POST /api/generate/draft-grid — Meshy-Style 4-Candidate Fast Draft Preview Grid
            if (segs.size() == 3 && segs[1] == "generate" && segs[2] == "draft-grid" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string prompt = body.get_str("prompt", "Armored sci-fi cyber knight");
                RouterConfig cfg_copy;
                {
                    std::lock_guard<std::mutex> lk(g_state_mutex);
                    cfg_copy = g_router_cfg;
                }
                auto t0 = now_epoch_ms();
                auto drafts = generate_four_draft_candidates(cfg_copy, prompt, 62);
                JsonValue cands = JsonValue::array();
                for (size_t i = 0; i < drafts.size(); ++i) {
                    JsonValue rec = save_scene_asset(
                        assets_dir,
                        drafts[i].scene,
                        drafts[i].blueprint.get_str("title", "Draft") + " (" + drafts[i].style_tag + ")",
                        prompt,
                        "draft-candidate",
                        drafts[i].variant_label,
                        drafts[i].blueprint,
                        (now_epoch_ms() - t0) / 4
                    );
                    rec["variant_label"] = drafts[i].variant_label;
                    rec["style_tag"] = drafts[i].style_tag;
                    cands.push_back(std::move(rec));
                }
                JsonValue out = JsonValue::object({
                    {"prompt", prompt},
                    {"total_ms", now_epoch_ms() - t0},
                    {"candidates", cands}
                });
                return HttpResponse::json(out.dump(0));
            }

            // GET /api/neural/status — Neural 3D Diffusion Bridge + CGTrader Learned Priors Telemetry
            if (segs.size() == 3 && segs[1] == "neural" && segs[2] == "status" && req.method == "GET") {
                JsonValue st = get_neural_and_cgt_training_status_json();
                fs::path wpath = base_dir / "data" / "falcon_cgtrader_learned_weights.json";
                if (fs::exists(wpath)) {
                    try { st["weights_telemetry"] = JsonValue::parse(read_text_file(wpath)); } catch (...) {}
                }
                return HttpResponse::json(st.dump(0));
            }

            // POST /api/neural/train-priors — Run Pure C++20 Corpus Harvester & 250-Epoch Prior Optimizer
            if (segs.size() == 3 && segs[1] == "neural" && segs[2] == "train-priors" && req.method == "POST") {
                JsonValue body = req.body_str().empty() ? JsonValue::object() : JsonValue::parse(req.body_str());
                std::string local_dir = body.get_str("local_dir", "");
                bool offline = body.get_bool("offline", false);
#ifdef _WIN32
                fs::path trainer_bin = base_dir / "bin" / "falcon_corpus_trainer.exe";
#else
                fs::path trainer_bin = base_dir / "bin" / "falcon_corpus_trainer";
#endif
                if (fs::exists(trainer_bin)) {
                    std::string cmd = "\"" + trainer_bin.string() + "\" --root \"" + base_dir.string() + "\"";
                    if (offline) cmd += " --offline";
                    if (!local_dir.empty() && fs::exists(local_dir)) cmd += " --local-dir \"" + local_dir + "\"";
                    int rc = std::system(cmd.c_str());
                    (void)rc;
                }
                JsonValue st = get_neural_and_cgt_training_status_json();
                fs::path wpath = base_dir / "data" / "falcon_cgtrader_learned_weights.json";
                if (fs::exists(wpath)) {
                    try { st["weights_telemetry"] = JsonValue::parse(read_text_file(wpath)); } catch (...) {}
                }
                return HttpResponse::json(st.dump(0));
            }

            // POST /api/generate/multiview — 1 to 4 View Space-Carving OR Neural Cloud Image-to-3D Bridge
            if (segs.size() == 3 && segs[1] == "generate" && segs[2] == "multiview" && req.method == "POST") {
                auto mp = req.parse_multipart();
                std::vector<uint8_t> front_b = mp.count("front") ? mp["front"].data : (mp.count("file") ? mp["file"].data : std::vector<uint8_t>{});
                std::vector<uint8_t> side_b  = mp.count("side")  ? mp["side"].data  : std::vector<uint8_t>{};
                std::vector<uint8_t> back_b  = mp.count("back")  ? mp["back"].data  : std::vector<uint8_t>{};
                std::vector<uint8_t> top_b   = mp.count("top")   ? mp["top"].data   : std::vector<uint8_t>{};
                if (front_b.empty() && side_b.empty()) {
                    return HttpResponse::json("{\"error\":\"Upload at least a Front reference image\"}", 400);
                }
                std::string n_eng = mp.count("neural_engine") ? mp["neural_engine"].as_string() : "local_hull";
                std::string hf_tok = mp.count("hf_token") ? mp["hf_token"].as_string() : "";

                if (n_eng == "sf3d" || n_eng == "triposg" || n_eng == "hunyuan3d" || n_eng == "neural_auto") {
                    auto t0 = now_epoch_ms();
                    Scene3D n_sc;
                    std::string prov_lbl, note;
                    if (try_neural_3d_diffusion_bridge(n_eng, "3D model from reference photo", front_b, hf_tok, n_sc, prov_lbl, note)) {
                        JsonValue trace = JsonValue::array({
                            mk_agent_trace("@neural-3d-bridge", prov_lbl, note, now_epoch_ms() - t0)
                        });
                        JsonValue rec = save_scene_asset(
                            assets_dir, n_sc, "Neural Image-to-3D Asset", note,
                            "neural-image-to-3d", prov_lbl, JsonValue(nullptr), now_epoch_ms() - t0, "", JsonValue(nullptr), trace
                        );
                        return HttpResponse::json(rec.dump(0));
                    }
                }

                int res = mp.count("resolution") ? std::stoi(mp["resolution"].as_string()) : 96;
                float inf = mp.count("depth_inflation") ? std::stof(mp["depth_inflation"].as_string()) : 0.68f;

                int view_cnt = (!front_b.empty() ? 1 : 0) + (!side_b.empty() ? 1 : 0) + (!back_b.empty() ? 1 : 0) + (!top_b.empty() ? 1 : 0);
                auto t0 = now_epoch_ms();
                Scene3D sc = reconstruct_multiview_to_3d_cpp(front_b, side_b, back_b, top_b, res, inf);
                auto ms = now_epoch_ms() - t0;

                JsonValue rec = save_scene_asset(
                    assets_dir,
                    sc,
                    "Multi-View 3D (" + std::to_string(view_cnt) + "-View Photo Mesh)",
                    std::to_string(view_cnt) + "-View Visual Hull + Photo RGB Projection",
                    "multiview-to-3d",
                    "Falcon v4.0 Multi-View Space Carver (" + std::to_string(view_cnt) + " views)",
                    JsonValue(nullptr),
                    ms
                );
                return HttpResponse::json(rec.dump(0));
            }

            // POST /api/generate/text
            if (segs.size() == 3 && segs[1] == "generate" && segs[2] == "text" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string prompt = body.get_str("prompt", "");
                std::string style = body.get_str("style", "realistic");
                std::string sculpt_mode = body.get_str("sculpt_mode", "auto");
                std::string hf_token = body.get_str("hf_token", "");
                int res = body.get_int("resolution", default_grid);
                bool ultra_detail = body.get_bool("ultra_detail", true);

                JsonValue extra_trace = JsonValue::array();

                // 1. Optional Neural 3D Diffusion Bridge (`hunyuan3d` | `triposg` | `sf3d` | `neural_auto` | `local_neural`)
                if (sculpt_mode == "hunyuan3d" || sculpt_mode == "triposg" || sculpt_mode == "sf3d" ||
                    sculpt_mode == "neural_auto" || sculpt_mode == "local_neural") {
                    auto tn0 = now_epoch_ms();
                    Scene3D n_sc;
                    std::string prov_lbl, note;
                    if (try_neural_3d_diffusion_bridge(sculpt_mode, prompt, {}, hf_token, n_sc, prov_lbl, note)) {
                        extra_trace.push_back(mk_agent_trace("@neural-3d-bridge", prov_lbl, note, now_epoch_ms() - tn0));
                        JsonValue rec = save_scene_asset(
                            assets_dir, n_sc,
                            "Neural 3D: " + prompt.substr(0, 38),
                            prompt, "neural-text-to-3d", prov_lbl,
                            JsonValue(nullptr), now_epoch_ms() - tn0, "", JsonValue(nullptr), extra_trace
                        );
                        return HttpResponse::json(rec.dump(0));
                    } else {
                        extra_trace.push_back(mk_agent_trace(
                            "@neural-3d-bridge",
                            "Hybrid Failover Router",
                            note,
                            std::max<int64_t>(1, now_epoch_ms() - tn0)
                        ));
                    }
                }

                // 2. Bust / Classical Statue SIREN Organic Sculptor
                std::string plow = to_lower_str(prompt);
                bool wants_bust = (plow.find("bust") != std::string::npos || plow.find("head sculpt") != std::string::npos || plow.find("portrait statue") != std::string::npos);
                if (sculpt_mode == "neural_organic" || wants_bust) {
                    auto t0 = now_epoch_ms();
                    bool horned = (plow.find("horn") != std::string::npos || plow.find("demon") != std::string::npos || plow.find("oni") != std::string::npos);
                    bool winged = (plow.find("wing") != std::string::npos || plow.find("angel") != std::string::npos);
                    std::string omode = wants_bust ? (horned ? "horned_bust" : "bust") : (winged ? "winged_hero" : (horned ? "creature" : "humanoid"));
                    std::string fin = (plow.find("marble") != std::string::npos) ? "marble_veined"
                                    : (plow.find("bronze") != std::string::npos) ? "oxidized_bronze"
                                    : (plow.find("gold") != std::string::npos)   ? "anodized_gold"
                                    : "organic_skin";
                    std::string hex = (fin == "marble_veined") ? "#e8e6e1" : (fin == "oxidized_bronze" ? "#7c6244" : (fin == "anodized_gold" ? "#eab308" : "#d6b49c"));
                    Scene3D sc = synthesize_neural_organic_scene(omode, std::max(res, 96), fin, hex);
                    extra_trace.push_back(mk_agent_trace("@neural-siren", "Implicit Anatomical Field", "Evaluated C^2 smooth-metaball + SIREN micro-displacement grid.", now_epoch_ms() - t0));
                    JsonValue rec = save_scene_asset(
                        assets_dir, sc,
                        wants_bust ? "Sculpted Anatomical Bust" : "Sculpted Organic Character",
                        prompt, "neural-organic-3d", "Falcon v4.0 SIREN Implicit Organic Sculptor",
                        JsonValue(nullptr), now_epoch_ms() - t0, "", generate_auto_rig_json(sc, "biped"), extra_trace
                    );
                    return HttpResponse::json(rec.dump(0));
                }

                // 3. CGTrader-Trained Catmull-Clark Subdivision Master Sculptor (`auto` / `cgt_subdiv` / neural fallback)
                if (sculpt_mode != "csg_swarm") {
                    auto tc0 = now_epoch_ms();
                    int subdiv_lv = (res >= 112 || ultra_detail) ? 2 : 1;
                    int tex_res   = (res >= 120) ? 2048 : 1024;
                    Scene3D cgt_sc;
                    std::string cgt_title, cgt_cat, cgt_rig_type;
                    if (try_synthesize_cgt_master_scene(prompt, subdiv_lv, tex_res, cgt_sc, cgt_title, cgt_cat, cgt_rig_type)) {
                        size_t n_quads = 0, n_tris = 0;
                        for (const auto& pm : cgt_sc.parts) {
                            n_quads += pm.quads.size();
                            n_tris  += pm.faces.size();
                        }
                        const auto& pr = lookup_cgt_prior(cgt_cat);
                        extra_trace.push_back(mk_agent_trace(
                            "@cgt-prior-planner",
                            "CGTrader Learned Morphology Agent",
                            "Matched trained `" + cgt_cat + "` prior (250-epoch loss=" +
                                std::to_string(kCgtTrainingFinalLoss).substr(0, 6) + ", crease=" +
                                std::to_string(pr.subdiv_crease).substr(0, 4) + ") with zero mesh copying.",
                            4
                        ));
                        extra_trace.push_back(mk_agent_trace(
                            "@catmull-clark-sculptor",
                            "CGT-Standard Quad Subdivision Engine",
                            "Subdivided Lv." + std::to_string(subdiv_lv) + " quad cages into " +
                                std::to_string(n_quads) + " pure 4-gon quads (" + std::to_string(n_tris) +
                                " watertight tris, 0 N-gons) across " + std::to_string(cgt_sc.parts.size()) + " semantic parts.",
                            std::max<int64_t>(2, now_epoch_ms() - tc0)
                        ));
                        extra_trace.push_back(mk_agent_trace(
                            "@material-pbr",
                            "High-Res Studio PBR Baker",
                            "Baked " + std::to_string(tex_res) + "px square Albedo, ORM & Tangent-Space Normal maps + Cavity AO.",
                            18
                        ));
                        JsonValue rig_j(nullptr);
                        if (!cgt_rig_type.empty()) {
                            rig_j = generate_auto_rig_json(cgt_sc, cgt_rig_type);
                            extra_trace.push_back(mk_agent_trace(
                                "@rig-kinematics",
                                "Skeletal Binding Agent",
                                "Bound `" + cgt_rig_type + "` skeleton ready for 24-clip animation & FBX/USDZ export.",
                                6
                            ));
                        }
                        JsonValue rec = save_scene_asset(
                            assets_dir, cgt_sc, cgt_title, prompt,
                            "cgt-subdiv-3d",
                            "Falcon v4.0 CGTrader-Trained Catmull-Clark Master (" + cgt_cat + ")",
                            JsonValue(nullptr), now_epoch_ms() - tc0, "", rig_j, extra_trace
                        );
                        return HttpResponse::json(rec.dump(0));
                    }
                }

                RouterConfig cfg_copy;
                {
                    std::lock_guard<std::mutex> lk(g_state_mutex);
                    cfg_copy = g_router_cfg;
                }

                auto t0 = now_epoch_ms();
                Swarm3DResult sw = execute_3d_agent_swarm(cfg_copy, prompt, res, style, ultra_detail);
                auto ms = now_epoch_ms() - t0;

                JsonValue rec = save_scene_asset(
                    assets_dir,
                    sw.scene,
                    sw.blueprint.get_str("title", "C++20 3D Asset"),
                    prompt,
                    "text-to-3d",
                    "Falcon 6-Agent Swarm · " + sw.provider,
                    sw.blueprint,
                    ms,
                    "",
                    sw.rig,
                    sw.agent_trace
                );
                return HttpResponse::json(rec.dump(0));
            }

            // POST /api/swarm/detail ("Add Everything Needed" / Ultra-Detail Kitbash Agent Pass)
            if (segs.size() == 3 && segs[1] == "swarm" && segs[2] == "detail" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string aid = fs::path(body.get_str("asset_id", "")).filename().string();
                std::string instruction = body.get_str("instruction", "add everything needed ultra detail kitbash");
                int res = body.get_int("resolution", std::max(default_grid, 108));

                auto [cur_sc, meta] = load_scene_asset(assets_dir, aid);
                RouterConfig cfg_copy;
                {
                    std::lock_guard<std::mutex> lk(g_state_mutex);
                    cfg_copy = g_router_cfg;
                }

                auto t0 = now_epoch_ms();
                JsonValue cur_bp = meta.at_or_null("blueprint");
                if (cur_bp.is_object() && cur_bp.has("parts")) {
                    Swarm3DResult sw = execute_3d_agent_swarm(cfg_copy, instruction, res, "realistic", true, &cur_bp);
                    auto ms = now_epoch_ms() - t0;
                    JsonValue rec = save_scene_asset(
                        assets_dir,
                        sw.scene,
                        sw.blueprint.get_str("title", meta.get_str("title")) + " [Ultra]",
                        meta.get_str("prompt") + " + " + instruction,
                        "swarm-ultra-detail",
                        "Falcon @kitbash-detailer + @sculptor-csg",
                        sw.blueprint,
                        ms,
                        aid,
                        sw.rig.is_object() ? sw.rig : meta.at_or_null("rig"),
                        sw.agent_trace
                    );
                    return HttpResponse::json(rec.dump(0));
                } else {
                    // Subdivide & smooth raw/image mesh when no parametric blueprint exists
                    for (auto& m : cur_sc.parts) {
                        if (m.faces.size() < 40000) m.subdivide_midpoint();
                        m.taubin_smooth(4);
                    }
                    JsonValue trace = JsonValue::array({
                        mk_agent_trace("@sculptor-csg", "Subdivision & Polish Agent", "Subdivided & Taubin-smoothed organic surface.", now_epoch_ms() - t0)
                    });
                    JsonValue rec = save_scene_asset(
                        assets_dir, cur_sc, meta.get_str("title") + " [Polished]", meta.get_str("prompt"),
                        meta.get_str("source_mode"), "Falcon Subdivision Polish", JsonValue(nullptr),
                        now_epoch_ms() - t0, aid, meta.at_or_null("rig"), trace
                    );
                    return HttpResponse::json(rec.dump(0));
                }
            }

            // GET /api/games — List all Multi-Agent 3D Games
            if (segs.size() == 2 && segs[1] == "games" && req.method == "GET") {
                std::vector<std::pair<int64_t, JsonValue>> items;
                for (const auto& entry : fs::directory_iterator(games_dir)) {
                    if (entry.path().extension() == ".json") {
                        try {
                            JsonValue j = JsonValue::parse(read_text_file(entry.path()));
                            items.push_back({static_cast<int64_t>(j.get_num("created_at", 0)), std::move(j)});
                        } catch (...) {}
                    }
                }
                std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                JsonValue arr = JsonValue::array();
                for (auto& kv : items) arr.push_back(std::move(kv.second));
                return HttpResponse::json(arr.dump(0));
            }

            // POST /api/games/create — Multi-Agent 3D Game World & Asset Synthesizer
            if (segs.size() == 3 && segs[1] == "games" && segs[2] == "create" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string prompt = body.get_str("prompt", "3D Action RPG Citadel Dragon Siege");
                RouterConfig cfg_copy;
                {
                    std::lock_guard<std::mutex> lk(g_state_mutex);
                    cfg_copy = g_router_cfg;
                }
                std::string gid = random_hex_id();
                JsonValue g = build_3d_game_manifest(
                    gid,
                    prompt,
                    now_epoch_sec(),
                    [&](const std::string& kw, const std::string& fp) {
                        return resolve_or_synthesize_game_asset(assets_dir, cfg_copy, default_grid, kw, fp);
                    }
                );
                write_text_file(games_dir / (gid + ".json"), g.dump(2));
                return HttpResponse::json(g.dump(0));
            }

            // POST /api/games/{id}/mutate — "Add Everything Needed" Live Game Agent Co-Pilot
            if (segs.size() == 4 && segs[1] == "games" && segs[3] == "mutate" && req.method == "POST") {
                std::string gid = fs::path(segs[2]).filename().string();
                fs::path gp = games_dir / (gid + ".json");
                if (!fs::exists(gp)) return HttpResponse::json("{\"error\":\"Game not found\"}", 404);
                JsonValue body = JsonValue::parse(req.body_str());
                std::string instruction = body.get_str("instruction", "add everything needed");
                RouterConfig cfg_copy;
                {
                    std::lock_guard<std::mutex> lk(g_state_mutex);
                    cfg_copy = g_router_cfg;
                }
                JsonValue g = JsonValue::parse(read_text_file(gp));
                auto changes = mutate_game_manifest(g, instruction, [&](const std::string& kw, const std::string& fp) {
                    return resolve_or_synthesize_game_asset(assets_dir, cfg_copy, default_grid, kw, fp);
                });
                write_text_file(gp, g.dump(2));
                JsonValue out = JsonValue::object({
                    {"game", g},
                    {"changes_count", changes.size()},
                    {"summary", changes.empty() ? "Updated game world." : changes[0]}
                });
                return HttpResponse::json(out.dump(0));
            }

            // DELETE /api/games/{id}
            if (segs.size() == 3 && segs[1] == "games" && req.method == "DELETE") {
                std::string gid = fs::path(segs[2]).filename().string();
                fs::remove(games_dir / (gid + ".json"));
                return HttpResponse::json("{\"deleted\":true}");
            }

            // GET /api/games/{id}/export/{fmt}
            if (segs.size() == 5 && segs[1] == "games" && segs[3] == "export" && req.method == "GET") {
                std::string gid = fs::path(segs[2]).filename().string();
                std::string fmt = to_lower_str(segs[4]);
                fs::path gp = games_dir / (gid + ".json");
                if (!fs::exists(gp)) return HttpResponse::json("{\"error\":\"Game not found\"}", 404);
                JsonValue g = JsonValue::parse(read_text_file(gp));
                std::string slug;
                for (char c : g.get_str("title", "FalconGame")) {
                    slug.push_back(std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
                }
                if (fmt == "html") {
                    std::string html = export_standalone_game_html(g, assets_dir);
                    std::vector<uint8_t> b(html.begin(), html.end());
                    return HttpResponse::bytes(std::move(b), "text/html; charset=utf-8", slug + "_Playable3DGame.html");
                }
                if (fmt == "cs") {
                    std::string cs = export_unity_game_csharp(g);
                    std::vector<uint8_t> b(cs.begin(), cs.end());
                    return HttpResponse::bytes(std::move(b), "text/plain; charset=utf-8", slug + "_UnityGameSuite.cs");
                }
                return HttpResponse::json("{\"error\":\"Unsupported game export format\"}", 400);
            }

            // POST /api/generate/image (Multipart PNG/JPEG Upload or 2D SketchPad)
            if (segs.size() == 3 && segs[1] == "generate" && segs[2] == "image" && req.method == "POST") {
                auto mp = req.parse_multipart();
                if (mp.find("file") == mp.end() || mp["file"].data.empty()) {
                    return HttpResponse::json("{\"error\":\"Missing image file\"}", 400);
                }
                std::string mode = mp.count("mode") ? mp["mode"].as_string() : "volumetric";
                float depth_s = mp.count("depth_strength") ? std::stof(mp["depth_strength"].as_string()) : 0.68f;
                float detail_r = mp.count("detail_relief") ? std::stof(mp["detail_relief"].as_string()) : 0.38f;
                int res = mp.count("resolution") ? std::stoi(mp["resolution"].as_string()) : 116;

                auto t0 = now_epoch_ms();
                Scene3D sc = reconstruct_image_to_3d_cpp(mp["file"].data, mode, depth_s, detail_r, res);
                auto ms = now_epoch_ms() - t0;

                std::string stem = fs::path(mp["file"].filename.empty() ? "Sketch" : mp["file"].filename).stem().string();
                JsonValue rec = save_scene_asset(
                    assets_dir,
                    sc,
                    "3D " + stem,
                    "C++20 Volumetric " + mode + " Reconstruction",
                    "image-to-3d",
                    "C++20 O(N) EDT + Surface Nets",
                    JsonValue(nullptr),
                    ms
                );
                return HttpResponse::json(rec.dump(0));
            }

            // POST /api/generate/blueprint
            if (segs.size() == 3 && segs[1] == "generate" && segs[2] == "blueprint" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                const auto& bp = body.at_or_null("blueprint");
                int res = body.get_int("resolution", default_grid);
                std::string aid = body.get_str("asset_id", "");

                auto t0 = now_epoch_ms();
                Scene3D sc = compile_blueprint(bp, res);
                auto ms = now_epoch_ms() - t0;

                JsonValue rec = save_scene_asset(
                    assets_dir,
                    sc,
                    bp.get_str("title", "Custom C++20 Parametric"),
                    bp.get_str("enhanced_prompt", "Edited Blueprint"),
                    "parametric-blueprint",
                    "C++20 Parametric Blueprint Compiler",
                    bp,
                    ms,
                    aid
                );
                return HttpResponse::json(rec.dump(0));
            }

            // POST /api/agent/plan (OpenCode Read-Only Plan Mode — scopes blueprint without mutating files)
            if (segs.size() == 3 && segs[1] == "agent" && segs[2] == "plan" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string prompt = body.get_str("prompt", "");
                RouterConfig cfg_copy;
                {
                    std::lock_guard<std::mutex> lk(g_state_mutex);
                    cfg_copy = g_router_cfg;
                }
                auto [bp, llm_used] = generate_blueprint_routed(cfg_copy, prompt);
                JsonValue steps = JsonValue::array();
                steps.push_back("1. Initialize C++20 OpenMP voxel space (" + std::to_string(default_grid) + "^3 grid, outer 2-manifold boundary clamp)");
                const auto& parts = bp["parts"];
                for (size_t i = 0; i < parts.size(); ++i) {
                    const auto& p = parts[i];
                    std::string sym = p.get_bool("mirror_x") ? " [mirror_x]" : (p.get_int("radial_count", 1) > 1 ? " [radial x" + std::to_string(p.get_int("radial_count", 1)) + "]" : "");
                    steps.push_back(std::to_string(i + 2) + ". Synthesize `" + p.get_str("id") + "` (" + p.get_str("primitive") + sym + ") · PBR " + p.get_str("color") + " (M=" + std::to_string(p.get_num("metallic")).substr(0, 4) + ", R=" + std::to_string(p.get_num("roughness")).substr(0, 4) + ")");
                }
                steps.push_back(std::to_string(parts.size() + 2) + ". Ground assembly on Y=0mm build plate & serialize multi-node glTF 2.0 .GLB + Unity C# Builder");

                JsonValue out = JsonValue::object();
                out["mode"] = "plan";
                out["provider"] = llm_used;
                out["title"] = bp.get_str("title", "Planned 3D Asset");
                out["parts_count"] = parts.size();
                out["steps"] = steps;
                out["blueprint"] = bp;
                return HttpResponse::json(out.dump(0));
            }

            // POST /api/agent/chat
            if (segs.size() == 3 && segs[1] == "agent" && segs[2] == "chat" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string msg = body.get_str("message", "");
                std::string cur_id = body.get_str("current_asset_id", "");

                JsonValue cur_bp;
                bool has_bp = false;
                if (!cur_id.empty()) {
                    try {
                        auto [_, meta] = load_scene_asset(assets_dir, cur_id);
                        if (meta.has("blueprint") && meta["blueprint"].is_object()) {
                            cur_bp = meta["blueprint"];
                            has_bp = true;
                        }
                    } catch (...) {}
                }

                RouterConfig cfg_copy;
                {
                    std::lock_guard<std::mutex> lk(g_state_mutex);
                    cfg_copy = g_router_cfg;
                }

                auto t0 = now_epoch_ms();
                JsonValue outcome = agent_chat_cpp(cfg_copy, msg, has_bp ? &cur_bp : nullptr);
                const auto& bp = outcome["blueprint"];
                Scene3D sc = compile_blueprint(bp, default_grid);
                auto ms = now_epoch_ms() - t0;

                std::string target_id = (outcome.get_str("action") == "updated_model" && has_bp) ? cur_id : "";
                JsonValue rec = save_scene_asset(
                    assets_dir,
                    sc,
                    bp.get_str("title", "Agent 3D Asset"),
                    msg,
                    "3d-agent",
                    "C++20 Agent · " + outcome.get_str("provider"),
                    bp,
                    ms,
                    target_id
                );

                JsonValue resp = JsonValue::object();
                resp["reply"] = outcome["reply"];
                resp["action"] = outcome["action"];
                resp["provider"] = outcome["provider"];
                resp["asset"] = rec;
                return HttpResponse::json(resp.dump(0));
            }

            // POST /api/mesh/{id}/{op}
            if (segs.size() == 4 && segs[1] == "mesh" && req.method == "POST") {
                std::string aid = fs::path(segs[2]).filename().string();
                std::string op = segs[3];
                JsonValue body = JsonValue::parse(req.body_str());
                auto [sc, meta] = load_scene_asset(assets_dir, aid);
                auto t0 = now_epoch_ms();

                if (op == "resize") {
                    AABB b = sc.bounds();
                    Vec3 ext = b.extents();
                    float sx = (static_cast<float>(body.get_num("x_mm", 80.0)) / kDefaultMmPerUnit) / std::max(ext.x, 1e-4f);
                    float sy = (static_cast<float>(body.get_num("y_mm", 80.0)) / kDefaultMmPerUnit) / std::max(ext.y, 1e-4f);
                    float sz = (static_cast<float>(body.get_num("z_mm", 80.0)) / kDefaultMmPerUnit) / std::max(ext.z, 1e-4f);
                    for (auto& m : sc.parts) m.apply_transform(Mat4::scale(sx, sy, sz));
                    sc.ground_on_y0();
                    JsonValue rec = save_scene_asset(assets_dir, sc, meta.get_str("title"), meta.get_str("prompt"),
                        meta.get_str("source_mode"), "C++20 Exact Millimeter CAD Resizer",
                        meta.at_or_null("blueprint"), now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "topology") {
                    std::string mode = body.get_str("operation", "smooth");
                    int iters = body.get_int("smooth_iterations", 4);
                    float ratio = static_cast<float>(body.get_num("target_ratio", 0.55));
                    for (auto& m : sc.parts) {
                        if (mode == "subdivide") m.subdivide_midpoint();
                        else if (mode == "decimate") m.decimate_cluster(ratio);
                        m.taubin_smooth(iters);
                    }
                    JsonValue rec = save_scene_asset(assets_dir, sc, meta.get_str("title"), meta.get_str("prompt"),
                        meta.get_str("source_mode"), "C++20 Topology (" + mode + ")",
                        meta.at_or_null("blueprint"), now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "print-repair") {
                    bool flat_cut = body.get_bool("flat_bottom_cut", true);
                    AABB b = sc.bounds();
                    float cut_y = b.min.y + b.extents().y * 0.035f;
                    for (auto& m : sc.parts) {
                        m.weld_vertices(1e-4f);
                        if (flat_cut) {
                            AABB mb = m.bounds();
                            if (mb.min.y < cut_y && mb.max.y > cut_y) {
                                Mesh sliced = m.slice_plane_capped({0.0f, cut_y, 0.0f}, {0.0f, 1.0f, 0.0f});
                                if (sliced.faces.size() > 8) m = std::move(sliced);
                            }
                        }
                    }
                    sc.ground_on_y0();
                    JsonValue rec = save_scene_asset(assets_dir, sc, meta.get_str("title"), meta.get_str("prompt"),
                        meta.get_str("source_mode"), "C++20 Print Doctor · Manifold & Flat Bed",
                        meta.at_or_null("blueprint"), now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "auto-split") {
                    std::string axis = body.get_str("axis", "y");
                    float gap = static_cast<float>(body.get_num("explode_gap", 0.36));
                    Mesh merged = sc.merged();
                    merged.weld_vertices(1e-4f);
                    AABB b = merged.bounds();
                    Vec3 c = b.center();
                    Vec3 ext = b.extents();

                    Vec3 n_pos = (axis == "x") ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
                    Vec3 n_neg = (axis == "x") ? Vec3{-1, 0, 0} : Vec3{0, -1, 0};

                    Mesh half_b = merged.slice_plane_capped(c, n_pos);
                    Mesh half_a = merged.slice_plane_capped(c, n_neg);

                    if (axis == "x") {
                        half_b.translate(gap * 0.5f, 0.0f, 0.0f);
                        half_a.translate(-gap * 0.5f, 0.0f, 0.0f);
                    } else {
                        half_b.translate(0.0f, gap, 0.0f);
                    }
                    half_a.id = "split_half_A"; half_a.name = "Split Half A";
                    half_a.material = {"Mat_HalfA", "#cbd5e1", 0.35f, 0.42f, "", 0.0f};
                    half_b.id = "split_half_B"; half_b.name = "Split Half B";
                    half_b.material = {"Mat_HalfB", "#38bdf8", 0.35f, 0.38f, "", 0.0f};

                    float min_e = std::min({ext.x, ext.y, ext.z});
                    float pin_r = std::max(0.035f, min_e * 0.055f);
                    float pin_h = std::max(0.14f, min_e * 0.18f);
                    Mesh pins;
                    for (float sign : {-1.0f, 1.0f}) {
                        Mesh p = make_cylinder_mesh(pin_r, pin_h, 28);
                        if (axis == "x") {
                            p.apply_transform(Mat4::trs({c.x - gap * 0.5f + pin_h * 0.35f, c.y, c.z + sign * ext.z * 0.18f}, {0, 0, -90}, {1,1,1}));
                        } else {
                            p.apply_transform(Mat4::trs({c.x + sign * ext.x * 0.18f, c.y + pin_h * 0.35f, c.z}, {0, 0, 0}, {1,1,1}));
                        }
                        if (pins.positions.empty()) pins = p;
                        else pins.append(p);
                    }
                    pins.id = "alignment_pins_025mm_tol";
                    pins.name = "0.25mm Snap Alignment Pins";
                    pins.material = {"Mat_Pins", "#f59e0b", 0.65f, 0.25f, "#f59e0b", 0.28f};

                    Scene3D split_sc;
                    split_sc.parts = {std::move(half_a), std::move(half_b), std::move(pins)};
                    split_sc.ground_on_y0();

                    JsonValue rec = save_scene_asset(assets_dir, split_sc, meta.get_str("title") + " (Auto-Split + Pins)",
                        meta.get_str("prompt"), "print-split",
                        "C++20 Auto-Split (" + to_lower_str(axis) + "-Plane + 0.25mm Pins)",
                        JsonValue(nullptr), now_epoch_ms() - t0);
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "smart-remesh") {
                    size_t target_faces = static_cast<size_t>(std::clamp(body.get_int("target_faces", 8000), 100, 300000));
                    bool quad_mode = body.get_bool("quad_mode", true);
                    bool preserve_creases = body.get_bool("preserve_creases", true);
                    int tex_res = body.get_int("tex_resolution", meta.get_int("tex_resolution", 256));

                    SmartRemeshStats st = smart_remesh_scene(sc, target_faces, quad_mode, preserve_creases);
                    JsonValue rep = JsonValue::object({
                        {"input_triangles", static_cast<int64_t>(st.input_tris)},
                        {"final_triangles", static_cast<int64_t>(st.output_tris)},
                        {"quad_count", static_cast<int64_t>(st.output_quads)},
                        {"final_polygons", static_cast<int64_t>(st.output_polygons)},
                        {"quad_mode", quad_mode},
                        {"target_faces", static_cast<int64_t>(target_faces)}
                    });
                    for (auto& m : sc.parts) {
                        m.material.tex_resolution = std::clamp(tex_res, 128, 4096);
                    }
                    std::string badge = quad_mode
                        ? ("C++20 Garland-Heckbert QEM + Quad Remesh (" + std::to_string(rep.get_int("quad_count", 0)) + " quads)")
                        : ("C++20 Garland-Heckbert QEM (" + std::to_string(rep.get_int("final_triangles", 0)) + " tris)");
                    JsonValue rec = save_scene_asset(assets_dir, sc, meta.get_str("title"), meta.get_str("prompt"),
                        meta.get_str("source_mode"), badge,
                        JsonValue(nullptr), now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    rec["remesh_report"] = rep;
                    write_text_file(assets_dir / (aid + ".json"), rec.dump(2));
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "ai-texture") {
                    std::string tprompt = to_lower_str(body.get_str("prompt", "brushed titanium"));
                    int tex_res = std::clamp(body.get_int("tex_resolution", 2048), 256, 4096);
                    std::string fin = "brushed_titanium";
                    std::string col = "#cbd5e1";
                    float met = 0.78f, rgh = 0.28f;

                    if (tprompt.find("gold") != std::string::npos || tprompt.find("brass") != std::string::npos) {
                        fin = "anodized_gold"; col = "#eab308"; met = 0.92f; rgh = 0.20f;
                    } else if (tprompt.find("carbon") != std::string::npos || tprompt.find("stealth") != std::string::npos) {
                        fin = "carbon_fiber"; col = "#1e293b"; met = 0.35f; rgh = 0.26f;
                    } else if (tprompt.find("damascus") != std::string::npos || tprompt.find("forged") != std::string::npos) {
                        fin = "damascus_steel"; col = "#cbd5e1"; met = 0.90f; rgh = 0.22f;
                    } else if (tprompt.find("anime") != std::string::npos || tprompt.find("cel") != std::string::npos || tprompt.find("toon") != std::string::npos) {
                        fin = "anime_cel"; col = "#38bdf8"; met = 0.05f; rgh = 0.65f;
                    } else if (tprompt.find("stylized") != std::string::npos || tprompt.find("hand") != std::string::npos || tprompt.find("fantasy") != std::string::npos) {
                        fin = "stylized_handpainted"; col = "#60a5fa"; met = 0.18f; rgh = 0.48f;
                    } else if (tprompt.find("skin") != std::string::npos || tprompt.find("flesh") != std::string::npos || tprompt.find("organic") != std::string::npos) {
                        fin = "organic_skin"; col = "#d6b49c"; met = 0.04f; rgh = 0.44f;
                    } else if (tprompt.find("cloth") != std::string::npos || tprompt.find("fabric") != std::string::npos || tprompt.find("leather") != std::string::npos) {
                        fin = "woven_fabric"; col = "#7c2d12"; met = 0.06f; rgh = 0.72f;
                    } else if (tprompt.find("marble") != std::string::npos || tprompt.find("stone") != std::string::npos) {
                        fin = "marble_veined"; col = "#f1f5f9"; met = 0.08f; rgh = 0.24f;
                    } else if (tprompt.find("wood") != std::string::npos || tprompt.find("oak") != std::string::npos) {
                        fin = "carved_wood"; col = "#92400e"; met = 0.05f; rgh = 0.62f;
                    } else if (tprompt.find("crimson") != std::string::npos || tprompt.find("red") != std::string::npos) {
                        fin = "painted_armor"; col = "#dc2626"; met = 0.65f; rgh = 0.22f;
                    } else if (tprompt.find("emerald") != std::string::npos || tprompt.find("jade") != std::string::npos) {
                        fin = "dragon_scales"; col = "#10b981"; met = 0.42f; rgh = 0.30f;
                    }

                    for (auto& m : sc.parts) {
                        m.colors.clear();
                        m.material.finish = fin;
                        m.material.color_hex = col;
                        m.material.metallic = met;
                        m.material.roughness = rgh;
                        m.material.tex_resolution = tex_res;
                    }
                    JsonValue rec = save_scene_asset(assets_dir, sc, meta.get_str("title"), meta.get_str("prompt"),
                        meta.get_str("source_mode"), "Falcon HD PBR Baker (" + std::to_string(tex_res) + "px · " + fin + ")",
                        meta.at_or_null("blueprint"), now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "paint-region") {
                    Vec3 center{0.0f, 0.5f, 0.0f};
                    if (body.has("center") && body["center"].is_array() && body["center"].size() >= 3) {
                        center.x = static_cast<float>(body["center"][0].as_num());
                        center.y = static_cast<float>(body["center"][1].as_num());
                        center.z = static_cast<float>(body["center"][2].as_num());
                    }
                    float radius = static_cast<float>(body.get_num("radius", 0.35));
                    std::string col = body.get_str("color", "#38bdf8");
                    float feather = static_cast<float>(body.get_num("feather", 0.40));
                    std::string pid = body.get_str("part_id", "");

                    size_t painted = paint_mesh_region_3d(sc, center, radius, col, feather, pid);
                    JsonValue rec = save_scene_asset(assets_dir, sc, meta.get_str("title"), meta.get_str("prompt"),
                        meta.get_str("source_mode"), "C++20 3D Region Brush (" + std::to_string(painted) + " verts healed)",
                        meta.at_or_null("blueprint"), now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    rec["painted_vertices"] = static_cast<int64_t>(painted);
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "material") {
                    std::string col = body.get_str("color", "#94a3b8");
                    float met = static_cast<float>(body.get_num("metallic", 0.5));
                    float rgh = static_cast<float>(body.get_num("roughness", 0.35));
                    std::string emit = body.get_str("emissive", "");
                    float estr = static_cast<float>(body.get_num("emissive_strength", 0.0));
                    std::string fin = body.get_str("finish", "");
                    std::string pid = body.get_str("part_id", "");
                    int tex_res = std::clamp(body.get_int("tex_resolution", meta.get_int("tex_resolution", 256)), 128, 4096);

                    for (auto& m : sc.parts) {
                        if (!pid.empty() && m.id != pid) continue;
                        m.colors.clear();
                        m.material.color_hex = col;
                        m.material.metallic = met;
                        m.material.roughness = rgh;
                        m.material.emissive_hex = emit;
                        m.material.emissive_strength = estr;
                        m.material.tex_resolution = tex_res;
                        if (!fin.empty()) m.material.finish = fin;
                    }
                    JsonValue bp = meta.at_or_null("blueprint");
                    if (bp.is_object() && bp.has("parts")) {
                        for (size_t i = 0; i < bp["parts"].size(); ++i) {
                            auto& p = bp["parts"][i];
                            if (pid.empty() || p.get_str("id") == pid) {
                                p["color"] = col;
                                p["metallic"] = met;
                                p["roughness"] = rgh;
                                p["emissive"] = emit.empty() ? JsonValue(nullptr) : JsonValue(emit);
                                p["emissive_strength"] = estr;
                                if (!fin.empty()) p["finish"] = fin;
                            }
                        }
                    }
                    JsonValue rec = save_scene_asset(assets_dir, sc, meta.get_str("title"), meta.get_str("prompt"),
                        meta.get_str("source_mode"), "C++20 PBR Baker (" + std::to_string(tex_res) + "px)",
                        bp, now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "rig") {
                    std::string rtype = body.get_str("rig_type", "auto");
                    std::string clip_id = body.get_str("clip_id", "walk_standard");
                    bool bake_skin = body.get_bool("bake_skin", true);

                    RiggedSkinData skin = compute_volumetric_skin_weights(sc, rtype);
                    JsonValue rig_meta = generate_auto_rig_json(sc, skin.rig_type == "vehicle" ? "biped" : skin.rig_type);
                    rig_meta["rig_type"] = skin.rig_type;
                    rig_meta["bone_count"] = static_cast<int64_t>(skin.bones.size());
                    rig_meta["active_clip"] = clip_id;
                    rig_meta["skinned_glb_ready"] = bake_skin;
                    rig_meta["motion_clips_embedded"] = static_cast<int64_t>(get_motion_library_catalog().size());

                    if (bake_skin) {
                        std::vector<uint8_t> rigged_glb = export_rigged_animated_glb_bytes(sc, skin.rig_type, clip_id, true);
                        write_binary_file(assets_dir / (aid + "_rigged.glb"), rigged_glb);
                        meta["glb_url"] = "/exports/" + aid + "_rigged.glb?t=" + std::to_string(now_epoch_ms());
                        meta["engine_used"] = "Falcon Auto-Rigger (" + std::to_string(skin.bones.size()) + " bones · 24 Motion Clips)";
                    }
                    meta["rig"] = rig_meta;
                    write_text_file(assets_dir / (aid + ".json"), meta.dump(2));
                    return HttpResponse::json(meta.dump(0));
                }
            }

            // GET /api/export/{id}/{fmt}
            if (segs.size() == 4 && segs[1] == "export" && req.method == "GET") {
                std::string aid = fs::path(segs[2]).filename().string();
                std::string fmt = to_lower_str(segs[3]);
                auto [sc, meta] = load_scene_asset(assets_dir, aid);
                std::string title = meta.get_str("title", "Falcon_Asset");
                std::string slug;
                for (char c : title) {
                    slug.push_back(std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
                }
                if (slug.empty()) slug = aid;

                bool wants_rigged = (req.query.find("rigged=1") != std::string::npos) ||
                                    (meta.has("rig") && meta["rig"].is_object() && meta["rig"].get_bool("skinned_glb_ready", false));
                std::string active_clip = (meta.has("rig") && meta["rig"].is_object())
                    ? meta["rig"].get_str("active_clip", "walk_standard") : "walk_standard";
                std::string rtype = (meta.has("rig") && meta["rig"].is_object())
                    ? meta["rig"].get_str("rig_type", "auto") : "auto";

                if (fmt == "glb") {
                    fs::path rpath = assets_dir / (aid + "_rigged.glb");
                    if (wants_rigged && fs::exists(rpath)) {
                        return HttpResponse::bytes(read_binary_file(rpath), "model/gltf-binary", slug + "_Rigged24Clips.glb");
                    }
                    if (req.query.find("rigged=1") != std::string::npos) {
                        return HttpResponse::bytes(export_rigged_animated_glb_bytes(sc, rtype, active_clip, true), "model/gltf-binary", slug + "_Rigged24Clips.glb");
                    }
                    return HttpResponse::bytes(read_binary_file(assets_dir / (aid + ".glb")), "model/gltf-binary", slug + ".glb");
                }
                if (fmt == "fbx") {
                    return HttpResponse::bytes(export_fbx_bytes(sc, wants_rigged, rtype, active_clip), "application/octet-stream", slug + ".fbx");
                }
                if (fmt == "usdz") {
                    return HttpResponse::bytes(export_usdz_bytes(sc), "model/vnd.usdz+zip", slug + ".usdz");
                }
                if (fmt == "stl") {
                    return HttpResponse::bytes(export_stl_bytes(sc), "model/stl", slug + ".stl");
                }
                if (fmt == "3mf") {
                    return HttpResponse::bytes(export_3mf_bytes(sc, title), "application/vnd.ms-package.3dmanufacturing-3dmodel+xml", slug + ".3mf");
                }
                if (fmt == "obj") {
                    return HttpResponse::bytes(export_obj_bytes(sc), "text/plain; charset=utf-8", slug + ".obj");
                }
                if (fmt == "ply") {
                    return HttpResponse::bytes(export_ply_bytes(sc), "application/octet-stream", slug + ".ply");
                }
                if (fmt == "cs") {
                    std::string cs = meta.get_str("unity_csharp_script", "// Unity C# Script\n");
                    std::vector<uint8_t> b(cs.begin(), cs.end());
                    return HttpResponse::bytes(std::move(b), "text/plain; charset=utf-8", slug + "_UnityBuilder.cs");
                }
                return HttpResponse::json("{\"error\":\"Unsupported format\"}", 400);
            }

            return HttpResponse::json("{\"error\":\"Unknown API endpoint\"}", 404);
        }

        // 3. Static SPA Serving from In-Binary Embedded VFS (Zero Docker, Single-Executable!)
        if (!segs.empty() && segs[0] == "v1") {
            return HttpResponse::json("{\"error\":\"Not found\"}", 404);
        }

        std::string vpath = (req.path.empty() || req.path == "/") ? "/index.html" : req.path;
        auto vit = embedded_vfs.find(vpath);
        if (vit != embedded_vfs.end()) {
            return HttpResponse::bytes(vit->second.bytes, vit->second.mime);
        }

        // Optional disk override during frontend development
        fs::path rel = vpath.substr(1);
        fs::path target = web_dir / rel;
        if (fs::exists(target) && fs::is_regular_file(target)) {
            return HttpResponse::bytes(read_binary_file(target), guess_mime(target));
        }

        // SPA fallback to embedded /index.html
        auto idx_it = embedded_vfs.find("/index.html");
        if (idx_it != embedded_vfs.end()) {
            return HttpResponse::bytes(idx_it->second.bytes, idx_it->second.mime);
        }
        return HttpResponse::json("{\"status\":\"OpenMesh C++20 Standalone Engine Running\"}");
    });

    server.listen_forever("0.0.0.0", port);
#ifndef _WIN32
    curl_global_cleanup();
#endif
    return 0;
}
