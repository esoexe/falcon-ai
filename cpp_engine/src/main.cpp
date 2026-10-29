/**
 * OpenMesh Studio 2.0 — Pure Compiled C++20 Backend & 3D Geometry Server.
 * Zero Python anywhere: serves REST API, Multi-Part SDF/SurfaceNets 3D Generation,
 * PNG/JPEG Volumetric Image-to-3D, CAD Millimeter Resizing, Slicer .3MF/.STL/.GLB/.CS
 * Exports, and Static Web Studio on 0.0.0.0:8000.
 */

#include "agent_swarm.hpp"
#include "embedded_web.hpp"
#include "exporters.hpp"
#include "game_engine_builder.hpp"
#include "hardware.hpp"
#include "http_server.hpp"
#include "image_to_3d.hpp"
#include "json.hpp"
#include "llm_router.hpp"
#include "mesh.hpp"
#include "provider_catalog.hpp"
#include "sdf_compiler.hpp"

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
            parts_meta.push_back(pm);
        }
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
    curl_global_init(CURL_GLOBAL_DEFAULT);

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
            fs::path exe_p = base_dir / "releases" / "Falcon-AI-Setup-2.0.0-x64.exe";
            if (!fs::exists(exe_p)) exe_p = base_dir / "releases" / "Falcon-AI-2.0.0-x64.exe";
            if (!fs::exists(exe_p)) return HttpResponse::json("{\"error\":\"Setup.exe not found\"}", 404);
            return HttpResponse::bytes(read_binary_file(exe_p), "application/vnd.microsoft.portable-executable", exe_p.filename().string());
        }
        if (segs.size() == 2 && segs[0] == "download" && segs[1] == "source.zip") {
            fs::path zip_p = base_dir / "releases" / "Falcon-AI-2.0.0-Source-Code.zip";
            if (!fs::exists(zip_p)) return HttpResponse::json("{\"error\":\"Source archive not found\"}", 404);
            return HttpResponse::bytes(read_binary_file(zip_p), "application/zip", "Falcon-AI-2.0.0-Source-Code.zip");
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
                    fs::remove(assets_dir / (aid + ".json"));
                    return HttpResponse::json("{\"deleted\":true}");
                }
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

            // POST /api/generate/text
            if (segs.size() == 3 && segs[1] == "generate" && segs[2] == "text" && req.method == "POST") {
                JsonValue body = JsonValue::parse(req.body_str());
                std::string prompt = body.get_str("prompt", "");
                std::string style = body.get_str("style", "realistic");
                int res = body.get_int("resolution", default_grid);
                bool ultra_detail = body.get_bool("ultra_detail", true);

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

                if (op == "material") {
                    std::string col = body.get_str("color", "#94a3b8");
                    float met = static_cast<float>(body.get_num("metallic", 0.5));
                    float rgh = static_cast<float>(body.get_num("roughness", 0.35));
                    std::string emit = body.get_str("emissive", "");
                    float estr = static_cast<float>(body.get_num("emissive_strength", 0.0));
                    std::string fin = body.get_str("finish", "");
                    std::string pid = body.get_str("part_id", "");

                    for (auto& m : sc.parts) {
                        if (!pid.empty() && m.id != pid) continue;
                        m.colors.clear();
                        m.material.color_hex = col;
                        m.material.metallic = met;
                        m.material.roughness = rgh;
                        m.material.emissive_hex = emit;
                        m.material.emissive_strength = estr;
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
                        meta.get_str("source_mode"), "C++20 PBR Texture & Material Baker",
                        bp, now_epoch_ms() - t0, aid, meta.at_or_null("rig"));
                    return HttpResponse::json(rec.dump(0));
                }

                if (op == "rig") {
                    std::string rtype = body.get_str("rig_type", "biped");
                    meta["rig"] = generate_auto_rig_json(sc, rtype);
                    write_text_file(assets_dir / (aid + ".json"), meta.dump(2));
                    return HttpResponse::json(meta.dump(0));
                }
            }

            // GET /api/export/{id}/{fmt}
            if (segs.size() == 4 && segs[1] == "export" && req.method == "GET") {
                std::string aid = fs::path(segs[2]).filename().string();
                std::string fmt = to_lower_str(segs[3]);
                auto [sc, meta] = load_scene_asset(assets_dir, aid);
                std::string title = meta.get_str("title", "OpenMesh_Asset");
                std::string slug;
                for (char c : title) {
                    slug.push_back(std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
                }
                if (slug.empty()) slug = aid;

                if (fmt == "glb") {
                    return HttpResponse::bytes(read_binary_file(assets_dir / (aid + ".glb")), "model/gltf-binary", slug + ".glb");
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
    curl_global_cleanup();
    return 0;
}
