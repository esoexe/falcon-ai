#pragma once
/**
 * Falcon AI — Native C++20 Universal Local LLM Router & High-Realism Sculpted 3D Architect.
 * Routes prompts via libcurl to Ollama, LM Studio, llama.cpp, vLLM, or custom OpenAI servers,
 * with a zero-dependency C++20 Anatomical SDF + Boolean CSG + PBR Texture Blueprint Architect.
 */

#include "hardware.hpp"
#include "json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <tuple>
#include <vector>

namespace openmesh {

struct RouterConfig {
    std::string provider = "auto";
    std::string ollama_url = "http://127.0.0.1:11434";
    std::string ollama_model = "";
    std::string openai_base_url = "http://127.0.0.1:1234/v1";
    std::string openai_api_key = "";
    std::string openai_model = "";

    JsonValue to_json() const {
        JsonValue j = JsonValue::object();
        j["provider"] = provider;
        j["ollama_url"] = ollama_url;
        j["ollama_model"] = ollama_model;
        j["openai_base_url"] = openai_base_url;
        j["openai_api_key"] = openai_api_key;
        j["openai_model"] = openai_model;
        return j;
    }

    void update_from_json(const JsonValue& j) {
        if (j.has("provider")) provider = j.get_str("provider", provider);
        if (j.has("ollama_url")) ollama_url = j.get_str("ollama_url", ollama_url);
        if (j.has("ollama_model")) ollama_model = j.get_str("ollama_model", ollama_model);
        if (j.has("openai_base_url")) openai_base_url = j.get_str("openai_base_url", openai_base_url);
        if (j.has("openai_api_key")) openai_api_key = j.get_str("openai_api_key", openai_api_key);
        if (j.has("openai_model")) openai_model = j.get_str("openai_model", openai_model);
    }
};

inline std::string to_lower_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline bool has_word_or_phrase(const std::string& hay, const char* w) {
    if (!w || !*w) return false;
    const size_t wlen = std::char_traits<char>::length(w);
    size_t pos = 0;
    while ((pos = hay.find(w, pos)) != std::string::npos) {
        bool left_ok = (pos == 0) || !std::isalnum(static_cast<unsigned char>(hay[pos - 1]));
        size_t end = pos + wlen;
        bool right_ok = false;
        if (end == hay.size() || !std::isalnum(static_cast<unsigned char>(hay[end]))) {
            right_ok = true;
        } else if (hay[end] == 's' && (end + 1 == hay.size() || !std::isalnum(static_cast<unsigned char>(hay[end + 1])))) {
            right_ok = true; // plural -s (e.g. "cars", "swords")
        } else if (end + 1 < hay.size() && hay[end] == 'e' && hay[end + 1] == 's' &&
                   (end + 2 == hay.size() || !std::isalnum(static_cast<unsigned char>(hay[end + 2])))) {
            right_ok = true; // plural -es (e.g. "axes", "boxes")
        }
        if (left_ok && right_ok) return true;
        pos += 1;
    }
    return false;
}

inline bool contains_any(const std::string& hay, const std::vector<const char*>& words) {
    for (const char* w : words) {
        if (has_word_or_phrase(hay, w)) return true;
    }
    return false;
}

struct MatSpec {
    std::string hex;
    double met;
    double rgh;
};

inline void extract_palette_cpp(
    const std::string& prompt,
    MatSpec& prim,
    MatSpec& acc,
    std::string& glow
) {
    std::string low = to_lower_str(prompt);
    struct KwMat { const char* kw; const char* hex; double m; double r; };
    static constexpr KwMat kMats[] = {
        {"gold", "#d4af37", 0.94, 0.20}, {"golden", "#eab308", 0.90, 0.22},
        {"brass", "#ca8a04", 0.88, 0.28}, {"bronze", "#92400e", 0.84, 0.34},
        {"copper", "#c2410c", 0.88, 0.26}, {"silver", "#cbd5e1", 0.94, 0.16},
        {"chrome", "#e2e8f0", 0.98, 0.08}, {"titanium", "#94a3b8", 0.88, 0.24},
        {"damascus", "#94a3b8", 0.92, 0.22}, {"steel", "#64748b", 0.86, 0.28},
        {"iron", "#475569", 0.80, 0.42}, {"obsidian", "#18181b", 0.26, 0.16},
        {"carbon", "#1f2937", 0.24, 0.32}, {"black", "#1e293b", 0.36, 0.38},
        {"white", "#f1f5f9", 0.16, 0.24}, {"stone", "#78716c", 0.05, 0.88},
        {"wood", "#78350f", 0.05, 0.78}, {"bone", "#e7e5e4", 0.06, 0.58},
        {"jade", "#059669", 0.14, 0.20}, {"emerald", "#047857", 0.22, 0.16},
        {"ruby", "#be123c", 0.28, 0.14}, {"crimson", "#991b1b", 0.38, 0.34},
        {"red", "#dc2626", 0.44, 0.30}, {"cobalt", "#1d4ed8", 0.74, 0.26},
        {"blue", "#2563eb", 0.68, 0.28}, {"yellow", "#eab308", 0.62, 0.24},
        {"orange", "#ea580c", 0.58, 0.26}, {"green", "#16a34a", 0.52, 0.28},
        {"purple", "#6d28d9", 0.40, 0.28}
    };

    std::vector<MatSpec> hits;
    for (const auto& km : kMats) {
        if (has_word_or_phrase(low, km.kw)) {
            hits.push_back({km.hex, km.m, km.r});
        }
    }
    if (hits.size() >= 1) prim = hits[0];
    if (hits.size() >= 2) acc = hits[1];

    struct KwGlow { const char* kw; const char* hex; };
    static constexpr KwGlow kGlows[] = {
        {"cyan", "#22d3ee"}, {"plasma", "#38bdf8"}, {"neon", "#06b6d4"},
        {"purple", "#c084fc"}, {"arcane", "#a855f7"}, {"void", "#9333ea"},
        {"fire", "#f97316"}, {"lava", "#ef4444"}, {"molten", "#fb923c"},
        {"crimson", "#f43f5e"}, {"emerald", "#34d399"}, {"green", "#22c55e"},
        {"gold", "#fbbf24"}, {"amber", "#f59e0b"}, {"ice", "#7dd3fc"}
    };
    for (const auto& kg : kGlows) {
        if (has_word_or_phrase(low, kg.kw)) {
            glow = kg.hex;
            break;
        }
    }
}

inline JsonValue mk_part(
    const std::string& id,
    const std::string& name,
    const std::string& prim,
    JsonValue params,
    std::array<double, 3> pos,
    std::array<double, 3> rot,
    std::array<double, 3> scl,
    const std::string& col,
    double met,
    double rgh,
    bool mirror_x = false,
    int radial = 1,
    const std::string& detail = "smooth",
    const std::string& emit_hex = "",
    double emit_str = 0.0,
    const std::string& finish = ""
) {
    JsonValue p = JsonValue::object();
    p["id"] = id;
    p["name"] = name;
    p["primitive"] = prim;
    p["params"] = std::move(params);
    p["detail_style"] = detail;
    if (!finish.empty()) p["finish"] = finish;
    p["position"] = JsonValue::array({pos[0], pos[1], pos[2]});
    p["rotation"] = JsonValue::array({rot[0], rot[1], rot[2]});
    p["scale"] = JsonValue::array({scl[0], scl[1], scl[2]});
    p["mirror_x"] = mirror_x;
    p["radial_count"] = radial;
    p["color"] = col;
    p["metallic"] = met;
    p["roughness"] = rgh;
    p["emissive"] = emit_hex.empty() ? JsonValue(nullptr) : JsonValue(emit_hex);
    p["emissive_strength"] = emit_str;
    return p;
}

inline JsonValue mk_lobe(
    std::array<double, 3> c,
    std::array<double, 3> r,
    double blend = 0.16,
    const char* shape = "ellipsoid",
    const char* op = "smooth_union"
) {
    JsonValue l = JsonValue::object();
    l["center"] = JsonValue::array({c[0], c[1], c[2]});
    l["radii"] = JsonValue::array({r[0], r[1], r[2]});
    l["blend"] = blend;
    l["shape"] = shape;
    l["op"] = op;
    return l;
}

inline JsonValue mk_cap(
    std::array<double, 3> p0,
    std::array<double, 3> p1,
    double r0,
    double r1,
    double blend = 0.14,
    const char* op = "smooth_union"
) {
    JsonValue l = JsonValue::object();
    l["shape"] = "tapered_capsule";
    l["p0"] = JsonValue::array({p0[0], p0[1], p0[2]});
    l["p1"] = JsonValue::array({p1[0], p1[1], p1[2]});
    l["r0"] = r0;
    l["r1"] = r1;
    l["blend"] = blend;
    l["op"] = op;
    return l;
}

inline JsonValue compile_prompt_cpp_symbolic(const std::string& prompt, bool* out_matched = nullptr) {
    if (out_matched) *out_matched = true;
    std::string low = to_lower_str(prompt);
    JsonValue bp = JsonValue::object();
    JsonValue parts = JsonValue::array();

    if (contains_any(low, {"sword", "blade", "dagger", "katana", "claymore", "saber", "axe", "greatsword"})) {
        MatSpec prim{"#cbd5e1", 0.94, 0.20}, acc{"#d4af37", 0.90, 0.24};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);
        bool katana = contains_any(low, {"katana", "samurai", "wakizashi", "nodachi"});
        double blen = katana ? 2.55 : 2.35;
        double bw   = katana ? 0.145 : 0.27;
        double bcur = katana ? -0.18 : 0.0;

        // 1. Sculpted Counterweight Pommel
        parts.push_back(mk_part("ornate_pommel", "Sculpted Crown Pommel", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.45}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.0, 0}, {0.13, 0.15, 0.09}, 0.05, "round_box"),
                mk_cap({-0.11, 0.04, 0}, {-0.18, -0.08, 0}, 0.05, 0.018, 0.04),
                mk_cap({ 0.11, 0.04, 0}, { 0.18, -0.08, 0}, 0.05, 0.018, 0.04),
                mk_lobe({0, 0.0, 0.09}, {0.06, 0.06, 0.04}, 0.02, "ellipsoid", "subtract"),
                mk_lobe({0, 0.0, -0.09}, {0.06, 0.06, 0.04}, 0.02, "ellipsoid", "subtract")
            })}}),
            {0, 0.18, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));

        // 2. Ergonomic Oval Leather-Wrapped Hilt
        parts.push_back(mk_part("wrapped_grip", "Cord-Bound Leather Hilt", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.65}, {"lobes", JsonValue::array({
                mk_cap({0, -0.36, 0}, {0, 0.0, 0}, 0.078, 0.092, 0.08),
                mk_cap({0,  0.0,  0}, {0, 0.36, 0}, 0.092, 0.082, 0.08)
            })}}),
            {0, 0.64, 0}, {0,0,0}, {1.05, 1.0, 0.68}, "#3b1d0e", 0.06, 0.80, false, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        // 3. Grip Retention Rings
        parts.push_back(mk_part("hilt_ferrules", "Engraved Hilt Ferrules", "torus",
            JsonValue::object({{"major_r", 0.088}, {"minor_r", 0.022}}),
            {0, 0.99, 0}, {90, 0, 0}, {1.12, 0.78, 1.0}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));

        // 4. Sculpted Crossguard / Tsuba with Boolean Socket
        if (katana) {
            parts.push_back(mk_part("katana_tsuba", "Carved Dragon Tsuba Guard", "sculpted_sdf",
                JsonValue::object({{"bounds", 0.45}, {"lobes", JsonValue::array({
                    mk_lobe({0, 0, 0}, {0.24, 0.038, 0.20}, 0.03, "ellipsoid"),
                    mk_lobe({0.11, 0, 0.07}, {0.045, 0.06, 0.045}, 0.015, "ellipsoid", "subtract"),
                    mk_lobe({-0.11, 0, -0.07}, {0.045, 0.06, 0.045}, 0.015, "ellipsoid", "subtract")
                })}}),
                {0, 1.05, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));
        } else {
            parts.push_back(mk_part("sculpted_crossguard", "Sculpted Winged Crossguard", "sculpted_sdf",
                JsonValue::object({{"bounds", 0.90}, {"lobes", JsonValue::array({
                    mk_lobe({0, 0, 0}, {0.22, 0.12, 0.11}, 0.05, "round_box"),
                    mk_cap({-0.14, 0.0, 0}, {-0.62, 0.14, 0}, 0.095, 0.032, 0.06),
                    mk_cap({-0.55, 0.12, 0}, {-0.72, 0.24, 0}, 0.042, 0.016, 0.04),
                    mk_cap({ 0.14, 0.0, 0}, { 0.62, 0.14, 0}, 0.095, 0.032, 0.06),
                    mk_cap({ 0.55, 0.12, 0}, { 0.72, 0.24, 0}, 0.042, 0.016, 0.04),
                    mk_lobe({0, 0.02, 0.10}, {0.075, 0.09, 0.05}, 0.02, "ellipsoid", "subtract"),
                    mk_lobe({0, 0.02, -0.10}, {0.075, 0.09, 0.05}, 0.02, "ellipsoid", "subtract")
                })}}),
                {0, 1.06, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));
        }

        // 5. Habaki Ricasso Collar
        parts.push_back(mk_part("habaki_collar", "Precision Habaki Blade Collar", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.35}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {bw * 0.78, 0.11, 0.062}, 0.02, "round_box")
            })}}),
            {0, 1.16, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));

        // 6. Damascus Folded-Steel Hollow-Fuller Blade
        parts.push_back(mk_part("damascus_blade", "Damascus Hollow-Fuller Blade", "blade",
            JsonValue::object({{"length", blen}, {"width", bw}, {"thickness", 0.078}, {"curve", bcur}}),
            {0, 1.18, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "damascus_steel"));

        // 7. Embedded Conduit Gem
        parts.push_back(mk_part("conduit_gem", "Faceted Conduit Core Gem", "octa_crystal",
            JsonValue::object({{"height", 0.19}, {"radius", 0.068}, {"sides", 6}}),
            {0, 1.08, 0}, {0,0,0}, {1.0, 1.0, 1.85}, glow, 0.18, 0.10, false, 1, "smooth", glow, 0.92, "forged_metal"));

        bp["title"] = katana ? "Damascus Steel Forged Katana" : "Damascus Forged Runesword";
        bp["category"] = "weapon";

    } else if (contains_any(low, {"rifle", "gun", "blaster", "pistol", "cannon", "sniper", "railgun"})) {
        MatSpec prim{"#334155", 0.84, 0.30}, acc{"#94a3b8", 0.92, 0.22};
        std::string glow = "#06b6d4";
        extract_palette_cpp(prompt, prim, acc, glow);

        // 1. Boolean-Ventilated M-LOK Upper Receiver & Shroud
        parts.push_back(mk_part("ventilated_receiver", "Ventilated M-LOK Receiver & Shroud", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.08, 0.12}, {0.16, 0.20, 0.98}, 0.04, "round_box"),
                mk_lobe({0, -0.08, -0.28}, {0.14, 0.16, 0.46}, 0.05, "round_box"),
                // Carve 3 pairs of M-LOK cooling windows through barrel shroud
                mk_lobe({0, 0.10, 0.42}, {0.24, 0.065, 0.11}, 0.018, "round_box", "subtract"),
                mk_lobe({0, 0.10, 0.72}, {0.24, 0.065, 0.11}, 0.018, "round_box", "subtract"),
                mk_lobe({0, 0.10, 1.00}, {0.24, 0.065, 0.09}, 0.018, "round_box", "subtract"),
                // Carve ejection port recess
                mk_lobe({0.15, 0.10, -0.08}, {0.06, 0.055, 0.16}, 0.015, "round_box", "subtract"),
                // Carve trigger well
                mk_lobe({0, -0.18, -0.26}, {0.20, 0.09, 0.13}, 0.02, "round_box", "subtract")
            })}}),
            {0, 0.92, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        // 2. Sculpted Ergonomic Thumbhole Stock & Contoured Grip
        parts.push_back(mk_part("tactical_stock_grip", "Sculpted Thumbhole Stock & Grip", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.25}, {"lobes", JsonValue::array({
                mk_cap({0, -0.02, -0.36}, {0, -0.44, -0.52}, 0.092, 0.078, 0.06),
                mk_lobe({0, 0.04, -0.84}, {0.11, 0.19, 0.38}, 0.06, "round_box"),
                mk_cap({0, -0.34, -0.52}, {0, -0.16, -1.12}, 0.065, 0.085, 0.07),
                mk_lobe({0, -0.04, -1.18}, {0.12, 0.24, 0.07}, 0.03, "round_box"),
                // Boolean thumbhole cutout
                mk_lobe({0, -0.10, -0.78}, {0.22, 0.095, 0.16}, 0.025, "ellipsoid", "subtract")
            })}}),
            {0, 0.92, 0}, {0,0,0}, {1,1,1}, "#1e293b", 0.08, 0.82, false, 1, "smooth", "", 0.0, "wrapped_leather"));

        // 3. Heavy Fluted Accelerator Barrel + Ported Muzzle Brake
        parts.push_back(mk_part("accelerator_barrel", "Fluted Magnetic Rail Barrel", "lathe_vase",
            JsonValue::object({{"height", 1.55}, {"mode", "column"}, {"flutes", 10}, {"twist", 0.0}}),
            {0, 1.01, 1.05}, {90, 0, 0}, {0.19, 1.0, 0.19}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("muzzle_brake", "Ported Compensator Muzzle Brake", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.45}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.11, 0.12, 0.24}, 0.03, "round_box"),
                mk_lobe({0, 0, -0.06}, {0.18, 0.05, 0.05}, 0.01, "round_box", "subtract"),
                mk_lobe({0, 0,  0.08}, {0.18, 0.05, 0.05}, 0.01, "round_box", "subtract")
            })}}),
            {0, 1.01, 1.84}, {0,0,0}, {1,1,1}, "#1e293b", 0.88, 0.28, false, 1, "smooth", "", 0.0, "forged_metal"));

        // 4. Internal Glowing Plasma Accelerator Conduit
        parts.push_back(mk_part("plasma_conduit", "Plasma Rail Accelerator Core", "capsule",
            JsonValue::object({{"radius", 0.068}, {"height", 1.02}}),
            {0, 1.02, 0.62}, {90, 0, 0}, {1,1,1}, glow, 0.15, 0.10, false, 1, "smooth", glow, 0.95));

        // 5. Boolean-Hollowed Holographic Reflex Sight
        parts.push_back(mk_part("holo_optic", "Hollowed Tactical Reflex Optic", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.50}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.11, 0.12, 0.24}, 0.025, "round_box"),
                mk_lobe({0, 0.02, 0}, {0.075, 0.075, 0.28}, 0.015, "round_box", "subtract")
            })}}),
            {0, 1.24, 0.04}, {0,0,0}, {1,1,1}, "#0f172a", 0.45, 0.34, false, 1, "smooth", "", 0.0, "carbon_fiber"));

        // 6. Angled Energy Magazine & Foregrip
        parts.push_back(mk_part("energy_magazine", "Ribbed Power-Cell Magazine", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.55}, {"lobes", JsonValue::array({
                mk_cap({0, 0.22, -0.04}, {0, -0.24, 0.08}, 0.095, 0.085, 0.04)
            })}}),
            {0, 0.56, 0.24}, {0,0,0}, {0.78, 1.0, 1.25}, acc.hex, acc.met, acc.rgh, false, 1, "scifi_panels", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("angled_foregrip", "Tactical Angled Foregrip", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.40}, {"lobes", JsonValue::array({
                mk_cap({0, 0.08, -0.12}, {0, -0.10, 0.12}, 0.068, 0.052, 0.04)
            })}}),
            {0, 0.72, 0.78}, {0,0,0}, {0.85, 1.0, 1.0}, "#1e293b", 0.10, 0.80, false, 1, "smooth", "", 0.0, "wrapped_leather"));

        bp["title"] = "M-LOK Tactical Plasma Railgun";
        bp["category"] = "weapon";

    } else if (contains_any(low, {"dragon", "monster", "creature", "beast", "wyvern", "demon", "alien", "kaiju"})) {
        MatSpec prim{"#7f1d1d", 0.16, 0.52}, acc{"#e7e5e4", 0.08, 0.56};
        std::string glow = "#f97316";
        extract_palette_cpp(prompt, prim, acc, glow);

        // 1. Craggy Volcanic Cliff Pedestal
        parts.push_back(mk_part("basalt_cliff_base", "Craggy Volcanic Basalt Outcrop", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.35}, {"lobes", JsonValue::array({
                mk_lobe({0, -0.12, 0}, {0.98, 0.20, 0.92}, 0.12, "ellipsoid"),
                mk_lobe({-0.25, 0.06, 0.12}, {0.55, 0.22, 0.48}, 0.14, "round_box"),
                mk_lobe({ 0.28, 0.02, -0.16}, {0.48, 0.18, 0.44}, 0.14, "round_box")
            })}}),
            {0, 0.24, 0}, {0,0,0}, {1,1,1}, "#292524", 0.06, 0.90, false, 1, "stone", "", 0.0, "weathered_stone"));

        // 2. Continuous Anatomical S-Curved Dragon Body, Neck & Coiled Serpentine Tail
        parts.push_back(mk_part("dragon_anatomy_body", "Muscular Scaled Torso, S-Neck & Coiled Tail", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.65}, {"lobes", JsonValue::array({
                // Pelvis & muscular chest ribcage
                mk_cap({0, -0.42, -0.24}, {0, 0.08, 0.10}, 0.34, 0.46, 0.18),
                mk_lobe({0.18, 0.06, 0.16}, {0.28, 0.30, 0.28}, 0.16),
                mk_lobe({-0.18, 0.06, 0.16}, {0.28, 0.30, 0.28}, 0.16),
                // Sweeping S-curved muscular neck
                mk_cap({0, 0.12, 0.12}, {0, 0.56, 0.04}, 0.30, 0.22, 0.14),
                mk_cap({0, 0.56, 0.04}, {0, 0.95, 0.34}, 0.22, 0.16, 0.12),
                // Coiled sweeping serpentine tail wrapping around the rock!
                mk_cap({0, -0.44, -0.26}, {0.24, -0.68, -0.82}, 0.30, 0.20, 0.14),
                mk_cap({0.24, -0.68, -0.82}, {0.68, -0.78, -0.42}, 0.20, 0.13, 0.12),
                mk_cap({0.68, -0.78, -0.42}, {0.58, -0.76, 0.28}, 0.13, 0.06, 0.10),
                mk_cap({0.58, -0.76, 0.28}, {0.28, -0.72, 0.62}, 0.06, 0.025, 0.06)
            })}}),
            {0, 1.15, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scales", "", 0.0, "dragon_scales"));

        // 3. Sculpted Roaring Dragon Skull with Boolean-Carved Open Jaw & Eye Sockets
        parts.push_back(mk_part("dragon_roaring_skull", "Armored Skull & Roaring Jaws", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.95}, {"lobes", JsonValue::array({
                // Cranium & brow ridges
                mk_lobe({0, 0.08, -0.06}, {0.22, 0.20, 0.26}, 0.10),
                mk_cap({-0.14, 0.18, -0.02}, {-0.10, 0.14, 0.24}, 0.075, 0.05, 0.06),
                mk_cap({ 0.14, 0.18, -0.02}, { 0.10, 0.14, 0.24}, 0.075, 0.05, 0.06),
                // Elongated upper snout & nasal bridge
                mk_cap({0, 0.06, 0.04}, {0, 0.04, 0.50}, 0.17, 0.105, 0.08),
                // Hinged roaring lower mandible jaw
                mk_cap({0, -0.08, -0.02}, {0, -0.22, 0.42}, 0.13, 0.075, 0.07),
                // Boolean carve roaring mouth cavity & nostrils & orbital sockets!
                mk_cap({0, -0.06, 0.18}, {0, -0.08, 0.52}, 0.095, 0.085, 0.035, "subtract"),
                mk_lobe({ 0.15, 0.10, 0.10}, {0.065, 0.055, 0.08}, 0.02, "ellipsoid", "subtract"),
                mk_lobe({-0.15, 0.10, 0.10}, {0.065, 0.055, 0.08}, 0.02, "ellipsoid", "subtract")
            })}}),
            {0, 2.12, 0.40}, {8, 0, 0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scales", "", 0.0, "dragon_scales"));

        // 4. Swept S-Curved Striated Crown Horns & Cheek Spikes (Carved Bone!)
        parts.push_back(mk_part("dragon_swept_horns", "Swept S-Curved Striated Horns", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.90}, {"lobes", JsonValue::array({
                mk_cap({0.0, 0.0, 0.0}, {0.14, 0.18, -0.28}, 0.078, 0.052, 0.04),
                mk_cap({0.14, 0.18, -0.28}, {0.26, 0.42, -0.58}, 0.052, 0.016, 0.04),
                mk_cap({0.02, -0.10, -0.02}, {0.16, -0.08, -0.24}, 0.045, 0.014, 0.03)
            })}}),
            {0.13, 2.24, 0.34}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "carved_bone"));

        // 5. Predator Fangs & Dorsal Spine Crest
        parts.push_back(mk_part("dragon_fangs_spines", "Ivory Fangs & Dorsal Osteoderm Spines", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                // Upper & lower canine fangs
                mk_cap({0.075, 0.90, 0.80}, {0.070, 0.81, 0.81}, 0.024, 0.006, 0.01),
                mk_cap({-0.075, 0.90, 0.80}, {-0.070, 0.81, 0.81}, 0.024, 0.006, 0.01),
                // Dorsal spine ridge along neck & back
                mk_cap({0, 0.82, 0.14}, {0, 0.94, -0.02}, 0.038, 0.010, 0.02),
                mk_cap({0, 0.56, -0.10}, {0, 0.70, -0.28}, 0.044, 0.012, 0.02),
                mk_cap({0, 0.26, -0.14}, {0, 0.42, -0.34}, 0.048, 0.012, 0.02),
                mk_cap({0, -0.06, -0.24}, {0, 0.08, -0.44}, 0.046, 0.012, 0.02),
                mk_cap({0, -0.36, -0.42}, {0, -0.24, -0.62}, 0.040, 0.010, 0.02)
            })}}),
            {0, 1.18, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "carved_bone"));

        // 6. Muscular Forelimbs & 3-Clawed Talons
        parts.push_back(mk_part("dragon_forelimbs", "Muscular Forelimbs & Raptorial Claws", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.95}, {"lobes", JsonValue::array({
                mk_cap({0.0, 0.28, 0.0}, {0.20, 0.02, 0.14}, 0.14, 0.10, 0.08),
                mk_cap({0.20, 0.02, 0.14}, {0.16, -0.32, 0.28}, 0.095, 0.068, 0.06),
                mk_cap({0.16, -0.32, 0.28}, {0.08, -0.42, 0.42}, 0.042, 0.012, 0.03),
                mk_cap({0.16, -0.32, 0.28}, {0.18, -0.43, 0.44}, 0.042, 0.012, 0.03),
                mk_cap({0.16, -0.32, 0.28}, {0.27, -0.41, 0.40}, 0.038, 0.012, 0.03)
            })}}),
            {0.26, 1.24, 0.12}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, true, 1, "scales", "", 0.0, "dragon_scales"));

        // 7. Muscular Hind Haunches, Crus & Talons Planted on Rock
        parts.push_back(mk_part("dragon_hindlimbs", "Muscular Haunches & Planted Talons", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.05}, {"lobes", JsonValue::array({
                mk_cap({0.0, 0.34, 0.0}, {0.22, 0.04, 0.18}, 0.21, 0.13, 0.10),
                mk_cap({0.22, 0.04, 0.18}, {0.14, -0.26, -0.06}, 0.125, 0.085, 0.07),
                mk_cap({0.14, -0.26, -0.06}, {0.18, -0.46, 0.16}, 0.082, 0.065, 0.06),
                mk_cap({0.18, -0.46, 0.16}, {0.10, -0.50, 0.34}, 0.044, 0.014, 0.03),
                mk_cap({0.18, -0.46, 0.16}, {0.20, -0.50, 0.36}, 0.044, 0.014, 0.03),
                mk_cap({0.18, -0.46, 0.16}, {0.30, -0.49, 0.32}, 0.040, 0.014, 0.03)
            })}}),
            {0.26, 0.80, -0.14}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, true, 1, "scales", "", 0.0, "dragon_scales"));

        // 8. Anatomical 4-Fingered Membranous Dragon Wings!
        parts.push_back(mk_part("dragon_bat_wings", "4-Fingered Membranous Dragon Wings", "bat_wing",
            JsonValue::object({{"span", 1.95}, {"chord", 1.15}}),
            {0.26, 1.44, 0.04}, {14, -10, 18}, {1,1,1}, prim.hex, prim.met, 0.62, true, 1, "scales", "", 0.0, "dragon_scales"));

        // 9. Molten Slit Predator Eyes
        parts.push_back(mk_part("dragon_eyes", "Molten Predator Eyes", "sphere",
            JsonValue::object({{"radius", 0.042}}),
            {0.135, 2.21, 0.50}, {0,0,0}, {0.9, 0.72, 1.25}, glow, 0.1, 0.08, true, 1, "smooth", glow, 0.98));

        bp["title"] = "Elder Scaled Wyvern Collectible";
        bp["category"] = "creature";

    } else if (contains_any(low, {"spaceship", "starfighter", "jet", "aircraft", "plane", "shuttle", "drone", "fighter"})) {
        MatSpec prim{"#cbd5e1", 0.76, 0.26}, acc{"#b91c1c", 0.58, 0.32};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);

        // 1. Stealth Chined Fuselage with Twin Boolean Air Intakes
        parts.push_back(mk_part("stealth_fuselage", "Chined Stealth Fuselage & Twin Intakes", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.65}, {"lobes", JsonValue::array({
                mk_cap({0, 0.0, 1.38}, {0, 0.02, 0.15}, 0.055, 0.32, 0.14),
                mk_lobe({0, 0.02, -0.35}, {0.42, 0.20, 0.78}, 0.14, "round_box"),
                mk_cap({ 0.26, 0.0, 0.38}, { 0.28, 0.02, -1.15}, 0.18, 0.21, 0.10),
                mk_cap({-0.26, 0.0, 0.38}, {-0.28, 0.02, -1.15}, 0.18, 0.21, 0.10),
                // Boolean carve supersonic diverterless intake mouths!
                mk_lobe({ 0.30, -0.02, 0.46}, {0.11, 0.09, 0.18}, 0.02, "round_box", "subtract"),
                mk_lobe({-0.30, -0.02, 0.46}, {0.11, 0.09, 0.18}, 0.02, "round_box", "subtract")
            })}}),
            {0, 0.68, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        // 2. Teardrop Fighter Bubble Canopy
        parts.push_back(mk_part("bubble_canopy", "Frameless Tinted Fighter Canopy", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.90}, {"lobes", JsonValue::array({
                mk_cap({0, -0.02, 0.44}, {0, 0.06, 0.02}, 0.09, 0.18, 0.08),
                mk_cap({0, 0.06, 0.02}, {0, -0.02, -0.42}, 0.18, 0.09, 0.08)
            })}}),
            {0, 0.84, 0.28}, {0,0,0}, {0.95, 0.85, 1.0}, "#0c4a6e", 0.92, 0.06, false, 1, "smooth", glow, 0.18, "forged_metal"));

        // 3. Swept Strike Wings + Forward Canards + Canted Twin V-Tails
        parts.push_back(mk_part("swept_wings", "Swept Stealth Strike Wings", "wing",
            JsonValue::object({{"span", 1.72}, {"chord", 0.98}, {"thickness", 0.075}, {"sweep", 0.68}}),
            {0.36, 0.66, -0.14}, {0, 0, -3}, {1,1,1}, prim.hex, prim.met, prim.rgh, true, 1, "smooth", "", 0.0, "painted_armor"));

        parts.push_back(mk_part("forward_canards", "Forward Delta Canards", "wing",
            JsonValue::object({{"span", 0.52}, {"chord", 0.36}, {"thickness", 0.045}, {"sweep", 0.38}}),
            {0.26, 0.69, 0.58}, {0, 0, 4}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "carbon_fiber"));

        parts.push_back(mk_part("canted_tails", "Canted Twin Vertical Stabilizers", "wing",
            JsonValue::object({{"span", 0.64}, {"chord", 0.48}, {"thickness", 0.055}, {"sweep", 0.42}}),
            {0.38, 0.82, -0.82}, {0, 0, 62}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "painted_armor"));

        // 4. Twin 12-Petal Variable-Geometry Afterburners + Ion Plumes
        parts.push_back(mk_part("afterburner_nozzles", "12-Petal Vectoring Afterburners", "serrated_nozzle",
            JsonValue::object({{"radius", 0.21}, {"length", 0.44}, {"petals", 12}}),
            {0.28, 0.70, -1.28}, {90, 0, 0}, {1,1,1}, "#1e293b", 0.92, 0.24, true, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("ion_cores", "Afterburner Plasma Cores", "sphere",
            JsonValue::object({{"radius", 0.13}}),
            {0.28, 0.70, -1.24}, {0,0,0}, {1.0, 1.0, 1.3}, glow, 0.1, 0.1, true, 1, "smooth", glow, 0.96));

        bp["title"] = "Stealth Vector-Thrust Starfighter";
        bp["category"] = "vehicle";

    } else if (contains_any(low, {"helicopter", "apache", "gunship", "quadcopter", "drone"})) {
        MatSpec prim{"#334155", 0.72, 0.28}, acc{"#18181b", 0.35, 0.32};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("heli_fuselage", "Sculpted Aerodynamic Fuselage & Tail Boom", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.65}, {"lobes", JsonValue::array({
                mk_cap({0, 0.04, 0.65}, {0, 0.0, -0.25}, 0.38, 0.34, 0.12),
                mk_cap({0, 0.06, -0.20}, {0, 0.14, -1.42}, 0.22, 0.075, 0.08),
                mk_cap({0, 0.14, -1.32}, {0, 0.48, -1.48}, 0.085, 0.045, 0.05)
            })}}),
            {0, 0.86, 0.12}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        parts.push_back(mk_part("heli_cockpit_glass", "Smoked Bubble Cockpit Canopy", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.85}, {"lobes", JsonValue::array({
                mk_cap({0, 0.04, 0.05}, {0, -0.06, 0.42}, 0.32, 0.20, 0.10)
            })}}),
            {0, 0.92, 0.56}, {0,0,0}, {1.02, 0.82, 1.0}, "#090d16", 0.95, 0.05, false, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("main_rotor_hub", "Turboshaft Mast & 4-Blade Main Rotor", "wing",
            JsonValue::object({{"span", 1.55}, {"chord", 0.14}, {"thickness", 0.024}, {"sweep", 0.02}}),
            {0.08, 1.38, 0.08}, {0,0,0}, {1,1,1}, acc.hex, 0.42, 0.30, false, 4, "smooth", "", 0.0, "carbon_fiber"));

        parts.push_back(mk_part("landing_skids", "Tubular Landing Skids & Struts", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.15}, {"lobes", JsonValue::array({
                mk_cap({0, -0.28, -0.58}, {0, -0.28, 0.68}, 0.032, 0.032, 0.02),
                mk_cap({0, -0.28,  0.34}, {-0.14, 0.08,  0.34}, 0.028, 0.028, 0.02),
                mk_cap({0, -0.28, -0.32}, {-0.14, 0.08, -0.32}, 0.028, 0.028, 0.02)
            })}}),
            {0.42, 0.52, 0.16}, {0,0,0}, {1,1,1}, "#475569", 0.86, 0.28, true, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("tail_rotor", "Fenestron Tail Rotor Assembly", "wheel_assembly",
            JsonValue::object({{"radius", 0.18}, {"width", 0.06}, {"spokes", 6}}),
            {0.08, 1.18, -1.32}, {0,0,0}, {1,1,1}, acc.hex, 0.55, 0.35, false, 1, "smooth", "", 0.0, "carbon_fiber"));

        bp["title"] = "Sculpted Twin-Turbine Gunship Helicopter";
        bp["category"] = "vehicle";

    } else if (contains_any(low, {"boat", "ship", "yacht", "submarine", "sailboat", "cruiser"})) {
        bool is_sub = contains_any(low, {"submarine", "sub"});
        MatSpec prim{is_sub ? "#1e293b" : "#f8fafc", is_sub ? 0.72 : 0.22, is_sub ? 0.36 : 0.18}, acc{"#0f172a", 0.40, 0.30};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("vessel_hull", is_sub ? "Hydrodynamic Pressure Hull" : "Sculpted Deep-V Marine Hull", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.70}, {"lobes", JsonValue::array({
                mk_cap({0, 0.0, 1.15}, {0, 0.02, -1.18}, is_sub ? 0.32 : 0.44, is_sub ? 0.22 : 0.38, 0.14),
                mk_cap({0, 0.06, 1.05}, {0, 0.12, 1.48}, 0.28, 0.06, 0.10)
            })}}),
            {0, 0.48, 0}, {0,0,0}, {1.0, is_sub ? 1.0 : 0.62, 1.0}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "painted_armor"));

        parts.push_back(mk_part("vessel_bridge", is_sub ? "Conning Sail Tower & Dive Planes" : "Panoramic Flybridge Cabin", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.05}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {is_sub ? 0.12 : 0.34, is_sub ? 0.28 : 0.18, 0.42}, 0.06, "round_box")
            })}}),
            {0, 0.84, 0.12}, {0,0,0}, {1,1,1}, acc.hex, 0.86, 0.12, false, 1, "smooth", "", 0.0, "forged_metal"));

        bp["title"] = is_sub ? "Sculpted Stealth Attack Submarine" : "Sculpted Luxury Motor Yacht";
        bp["category"] = "vehicle";

    } else if (contains_any(low, {"suv", "truck", "pickup", "jeep", "offroad", "off-road", "4x4", "rover", "van", "humvee"})) {
        MatSpec prim{"#334155", 0.72, 0.24}, acc{"#18181b", 0.28, 0.42};
        std::string glow = "#fef08a";
        extract_palette_cpp(prompt, prim, acc, glow);
        bool is_pickup = contains_any(low, {"truck", "pickup"});

        // 1. High-Clearance 4x4 Sculpted Body & Flared Wheel Arches
        parts.push_back(mk_part("suv_body_shell", is_pickup ? "Sculpted 4x4 Crew-Cab & Cargo Bed" : "Sculpted 4x4 Armored SUV Body", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.70}, {"lobes", JsonValue::array({
                mk_lobe({0, -0.02, 0.04}, {0.68, 0.28, 1.36}, 0.08, "round_box"),
                mk_lobe({0,  0.30, is_pickup ? 0.22 : -0.14}, {0.60, 0.26, is_pickup ? 0.62 : 0.94}, 0.08, "round_box"),
                // Carve open pickup bed if truck!
                is_pickup ? mk_lobe({0, 0.22, -0.84}, {0.54, 0.20, 0.50}, 0.03, "round_box", "subtract")
                          : mk_lobe({0, 0.48, -0.14}, {0.52, 0.04, 0.76}, 0.03, "round_box"),
                // Carve 4 tall off-road wheel wells
                mk_lobe({ 0.70, -0.22,  0.84}, {0.28, 0.38, 0.40}, 0.03, "ellipsoid", "subtract"),
                mk_lobe({-0.70, -0.22,  0.84}, {0.28, 0.38, 0.40}, 0.03, "ellipsoid", "subtract"),
                mk_lobe({ 0.70, -0.22, -0.84}, {0.28, 0.38, 0.40}, 0.03, "ellipsoid", "subtract"),
                mk_lobe({-0.70, -0.22, -0.84}, {0.28, 0.38, 0.40}, 0.03, "ellipsoid", "subtract")
            })}}),
            {0, 0.62, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "painted_armor"));

        // 2. Smoked Cabin Glass Greenhouse
        parts.push_back(mk_part("suv_cabin_glass", "Tinted Panoramic Cabin Glass", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.25}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.615, 0.17, is_pickup ? 0.56 : 0.86}, 0.05, "round_box")
            })}}),
            {0, 0.92, is_pickup ? 0.22 : -0.12}, {0,0,0}, {1,1,1}, "#090d16", 0.94, 0.06, false, 1, "smooth", "", 0.0, "forged_metal"));

        // 3. Heavy Tubular Steel Front Bullbar & Skidplate
        parts.push_back(mk_part("front_bullbar", "Tubular Steel Bullbar & Winch Bumper", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.05}, {"lobes", JsonValue::array({
                mk_lobe({0, -0.06, 0}, {0.66, 0.11, 0.12}, 0.04, "round_box"),
                mk_cap({-0.46, -0.04, 0.04}, {-0.38, 0.20, 0.02}, 0.042, 0.038, 0.03),
                mk_cap({ 0.46, -0.04, 0.04}, { 0.38, 0.20, 0.02}, 0.042, 0.038, 0.03),
                mk_cap({-0.38,  0.20, 0.02}, { 0.38, 0.20, 0.02}, 0.038, 0.038, 0.03)
            })}}),
            {0, 0.52, 1.42}, {0,0,0}, {1,1,1}, acc.hex, 0.78, 0.36, false, 1, "smooth", "", 0.0, "forged_metal"));

        // 4. 4 Oversized Mud-Terrain 4x4 Wheels & Brakes
        parts.push_back(mk_part("front_offroad_wheels", "Front Mud-Terrain 4x4 Wheels", "wheel_assembly",
            JsonValue::object({{"radius", 0.35}, {"width", 0.28}, {"spokes", 8}}),
            {0.68, 0.35, 0.84}, {0,0,0}, {1,1,1}, "#18181b", 0.25, 0.78, true, 1, "smooth", "", 0.0, "rubber_tire"));

        parts.push_back(mk_part("rear_offroad_wheels", "Rear Mud-Terrain 4x4 Wheels", "wheel_assembly",
            JsonValue::object({{"radius", 0.35}, {"width", 0.28}, {"spokes", 8}}),
            {0.68, 0.35, -0.84}, {0,0,0}, {1,1,1}, "#18181b", 0.25, 0.78, true, 1, "smooth", "", 0.0, "rubber_tire"));

        // 5. Roof Expedition LED Lightbar + Headlights + Taillights
        parts.push_back(mk_part("roof_led_bar", "Expedition Roof LED Lightbar", "capsule",
            JsonValue::object({{"radius", 0.032}, {"height", 0.96}}),
            {0, 1.18, 0.62}, {0, 0, 90}, {1,1,1}, glow, 0.12, 0.08, false, 1, "smooth", glow, 0.95));

        parts.push_back(mk_part("suv_headlights", "Dual LED Projector Headlights", "capsule",
            JsonValue::object({{"radius", 0.048}, {"height", 0.14}}),
            {0.48, 0.64, 1.38}, {0, 0, 90}, {1,1,1}, "#f8fafc", 0.1, 0.08, true, 1, "smooth", "#e0f2fe", 0.92));

        parts.push_back(mk_part("suv_taillights", "Vertical Rear LED Taillights", "capsule",
            JsonValue::object({{"radius", 0.038}, {"height", 0.22}}),
            {0.58, 0.68, -1.36}, {0,0,0}, {1,1,1}, "#ef4444", 0.1, 0.10, true, 1, "smooth", "#ef4444", 0.94));

        bp["title"] = is_pickup ? "Sculpted 4x4 Off-Road Pickup Truck" : "Sculpted 4x4 Armored Off-Road SUV";
        bp["category"] = "vehicle";

    } else if (contains_any(low, {"motorcycle", "motorbike", "bike", "superbike", "chopper", "ducati", "kawasaki"})) {
        MatSpec prim{"#dc2626", 0.84, 0.16}, acc{"#18181b", 0.35, 0.32};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("front_bike_wheel", "Front Alloy Superbike Wheel & Rotor", "wheel_assembly",
            JsonValue::object({{"radius", 0.31}, {"width", 0.14}, {"spokes", 10}}),
            {0, 0.31, 0.74}, {0,0,0}, {1,1,1}, "#18181b", 0.28, 0.72, false, 1, "smooth", "", 0.0, "rubber_tire"));

        parts.push_back(mk_part("rear_bike_wheel", "Rear Wide Superbike Tire & Sprocket", "wheel_assembly",
            JsonValue::object({{"radius", 0.32}, {"width", 0.20}, {"spokes", 10}}),
            {0, 0.32, -0.70}, {0,0,0}, {1,1,1}, "#18181b", 0.28, 0.72, false, 1, "smooth", "", 0.0, "rubber_tire"));

        parts.push_back(mk_part("bike_tank_fairing", "Sculpted Fuel Tank & Aero Nose Fairing", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.25}, {"lobes", JsonValue::array({
                mk_cap({0, 0.10, 0.18}, {0, 0.02, -0.18}, 0.20, 0.15, 0.08),
                mk_cap({0, 0.04, 0.42}, {0, -0.06, 0.68}, 0.18, 0.10, 0.08),
                mk_cap({0, -0.04, -0.22}, {0, 0.12, -0.78}, 0.13, 0.06, 0.07),
                mk_lobe({0, -0.18, 0.06}, {0.16, 0.19, 0.28}, 0.05, "round_box")
            })}}),
            {0, 0.68, 0.02}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "painted_armor"));

        parts.push_back(mk_part("inverted_forks", "Gold Anodized Inverted Front Forks", "hydraulic_piston",
            JsonValue::object({{"length", 0.68}, {"radius", 0.032}}),
            {0.10, 0.56, 0.60}, {-24, 0, 0}, {1,1,1}, "#eab308", 0.92, 0.18, true, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("rider_saddle", "Contoured Alcantara Rider Saddle", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.55}, {"lobes", JsonValue::array({
                mk_cap({0, 0, 0.14}, {0, 0.04, -0.22}, 0.13, 0.11, 0.05)
            })}}),
            {0, 0.72, -0.24}, {0,0,0}, {1.0, 0.55, 1.0}, "#18181b", 0.08, 0.82, false, 1, "smooth", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("bike_exhaust", "Upswept Titanium Akrapovic Exhaust", "serrated_nozzle",
            JsonValue::object({{"radius", 0.062}, {"length", 0.44}, {"petals", 8}}),
            {0.16, 0.48, -0.52}, {68, 8, 0}, {1,1,1}, "#cbd5e1", 0.94, 0.18, false, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("bike_headlight", "Aggressive Dual-LED Nose Headlight", "capsule",
            JsonValue::object({{"radius", 0.035}, {"height", 0.18}}),
            {0, 0.66, 0.72}, {0, 0, 90}, {1,1,1}, glow, 0.1, 0.08, false, 1, "smooth", glow, 0.95));

        bp["title"] = "Sculpted Streetfighter Superbike";
        bp["category"] = "vehicle";

    } else if (contains_any(low, {"car", "automobile", "sedan", "coupe", "roadster", "convertible", "gt", "hypercar", "supercar", "racecar", "lamborghini", "ferrari", "porsche", "mustang", "bmw", "mercedes", "audi", "tesla", "bugatti", "mclaren", "vehicle"})) {
        MatSpec prim{"#be123c", 0.84, 0.14}, acc{"#18181b", 0.26, 0.28};
        std::string glow = "#e0f2fe";
        extract_palette_cpp(prompt, prim, acc, glow);
        bool is_hyper = contains_any(low, {"hypercar", "supercar", "lamborghini", "ferrari", "bugatti", "mclaren", "racecar", "spoiler", "wing"});

        // 1. Sculpted Aerodynamic Widebody Monocoque with Boolean Wheel Wells, Grille & Side Intakes!
        parts.push_back(mk_part("car_monocoque", "Sculpted Aerodynamic Widebody Chassis", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.65}, {"lobes", JsonValue::array({
                // Low wide chassis core + sculpted muscular front/rear fenders
                mk_lobe({0, -0.05, 0.02}, {0.65, 0.18, 1.34}, 0.10, "round_box"),
                mk_lobe({ 0.55, -0.02,  0.76}, {0.22, 0.19, 0.42}, 0.12),
                mk_lobe({-0.55, -0.02,  0.76}, {0.22, 0.19, 0.42}, 0.12),
                mk_lobe({ 0.57,  0.02, -0.78}, {0.24, 0.22, 0.48}, 0.14),
                mk_lobe({-0.57,  0.02, -0.78}, {0.24, 0.22, 0.48}, 0.14),
                // Tapered aerodynamic hood & fastback rear decklid with ducktail lip
                mk_cap({0, -0.08, 0.72}, {0, -0.12, 1.36}, 0.49, 0.35, 0.12),
                mk_cap({0,  0.15, -0.22}, {0, 0.06, -1.26}, 0.40, 0.28, 0.12),
                // Boolean carve 4 crisp wheel arches!
                mk_lobe({ 0.68, -0.13,  0.82}, {0.24, 0.32, 0.34}, 0.03, "ellipsoid", "subtract"),
                mk_lobe({-0.68, -0.13,  0.82}, {0.24, 0.32, 0.34}, 0.03, "ellipsoid", "subtract"),
                mk_lobe({ 0.69, -0.11, -0.82}, {0.25, 0.34, 0.36}, 0.03, "ellipsoid", "subtract"),
                mk_lobe({-0.69, -0.11, -0.82}, {0.25, 0.34, 0.36}, 0.03, "ellipsoid", "subtract"),
                // Boolean carve front air dam grille + side intakes!
                mk_lobe({0.0, -0.11, 1.38}, {0.38, 0.08, 0.10}, 0.02, "round_box", "subtract"),
                mk_lobe({ 0.65, -0.02, -0.18}, {0.13, 0.09, 0.26}, 0.025, "round_box", "subtract"),
                mk_lobe({-0.65, -0.02, -0.18}, {0.13, 0.09, 0.26}, 0.025, "round_box", "subtract")
            })}}),
            {0, 0.43, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "painted_armor"));

        // 2. Sculpted Teardrop Smoked Glass Cabin Greenhouse
        parts.push_back(mk_part("teardrop_canopy", "Smoked Panoramic Glass Cabin", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.12}, {"lobes", JsonValue::array({
                mk_cap({0, -0.03, 0.44}, {0, 0.09, -0.02}, 0.36, 0.45, 0.12),
                mk_cap({0, 0.09, -0.02}, {0, -0.01, -0.60}, 0.45, 0.28, 0.12)
            })}}),
            {0, 0.57, -0.03}, {0,0,0}, {1.06, 0.54, 1.0}, "#090d16", 0.95, 0.05, false, 1, "smooth", "", 0.0, "forged_metal"));

        // 3. Carbon-Fiber Front Splitter, Side Skirts, Grille Mesh & Rear Venturi Diffuser
        parts.push_back(mk_part("carbon_aero_floor", "Carbon Front Splitter, Skirts & Rear Diffuser", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.65}, {"lobes", JsonValue::array({
                mk_lobe({0, -0.14, 0.04}, {0.68, 0.035, 1.40}, 0.02, "round_box"),
                mk_lobe({0, -0.06, 1.34}, {0.36, 0.065, 0.04}, 0.015, "round_box"),
                mk_lobe({0.22, -0.10, -1.36}, {0.025, 0.08, 0.14}, 0.01, "round_box"),
                mk_lobe({-0.22, -0.10, -1.36}, {0.025, 0.08, 0.14}, 0.01, "round_box")
            })}}),
            {0, 0.30, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "carbon_fiber"));

        // 4. 4 Real 10-Spoke Concave Alloy Wheels + Ventilated Brake Rotors + Wide Performance Tires!
        parts.push_back(mk_part("front_wheels", "Front 10-Spoke Alloy Wheels & Brakes", "wheel_assembly",
            JsonValue::object({{"radius", 0.285}, {"width", 0.23}, {"spokes", 10}}),
            {0.64, 0.285, 0.82}, {0,0,0}, {1,1,1}, "#18181b", 0.28, 0.72, true, 1, "smooth", "", 0.0, "rubber_tire"));

        parts.push_back(mk_part("rear_wheels", "Rear Deep-Dish Alloy Wheels & Brakes", "wheel_assembly",
            JsonValue::object({{"radius", 0.305}, {"width", 0.26}, {"spokes", 10}}),
            {0.65, 0.305, -0.82}, {0,0,0}, {1,1,1}, "#18181b", 0.28, 0.72, true, 1, "smooth", "", 0.0, "rubber_tire"));

        // 5. Rear Spoiler: Integrated Ducktail Lip on GT Sports Car OR Swan-Neck Carbon Wing on Hypercar!
        if (is_hyper) {
            parts.push_back(mk_part("gt_airfoil_wing", "Carbon-Fiber Swan-Neck GT Wing", "wing",
                JsonValue::object({{"span", 0.68}, {"chord", 0.24}, {"thickness", 0.04}, {"sweep", 0.06}}),
                {0.02, 0.76, -1.24}, {-8, 0, 0}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "carbon_fiber"));

            parts.push_back(mk_part("wing_pylons", "Swan-Neck Aero Pylons", "wing",
                JsonValue::object({{"span", 0.24}, {"chord", 0.16}, {"thickness", 0.03}, {"sweep", 0.12}}),
                {0.24, 0.62, -1.18}, {0, 0, 88}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "carbon_fiber"));
        } else {
            parts.push_back(mk_part("ducktail_spoiler", "Sculpted Carbon Ducktail Lip Spoiler", "wing",
                JsonValue::object({{"span", 0.56}, {"chord", 0.14}, {"thickness", 0.032}, {"sweep", 0.04}}),
                {0.02, 0.55, -1.30}, {-14, 0, 0}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "carbon_fiber"));
        }

        // 6. Angled Laser-LED Matrix Headlights & Full-Width Rear LED Taillight Blade
        parts.push_back(mk_part("matrix_headlights", "Angled Matrix Laser-LED Headlights", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.35}, {"lobes", JsonValue::array({
                mk_cap({-0.14, -0.02, 0.05}, {0.14, 0.03, -0.05}, 0.026, 0.022, 0.01)
            })}}),
            {0.46, 0.39, 1.28}, {0,0,0}, {1,1,1}, glow, 0.1, 0.08, true, 1, "smooth", glow, 0.96));

        parts.push_back(mk_part("rear_light_blade", "Full-Width Rear LED Taillight", "capsule",
            JsonValue::object({{"radius", 0.022}, {"height", 1.12}}),
            {0, 0.46, -1.36}, {0, 0, 90}, {1,1,1}, "#ef4444", 0.1, 0.1, false, 1, "smooth", "#ef4444", 0.95));

        bp["title"] = is_hyper ? "Aero Widebody GT Hypercar" : "Sculpted Realistic GT Sports Car";
        bp["category"] = "vehicle";

    } else if (contains_any(low, {"tank", "panzer", "apc", "artillery"})) {
        MatSpec prim{"#3f6212", 0.42, 0.58}, acc{"#1e293b", 0.78, 0.38};
        std::string glow = "#f97316";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("tank_hull", "Sloped Composite Armor Hull", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.65}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.72, 0.24, 1.32}, 0.06, "round_box"),
                mk_cap({0, 0.04, 0.85}, {0, -0.06, 1.38}, 0.62, 0.42, 0.08)
            })}}),
            {0, 0.46, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        parts.push_back(mk_part("tank_treads", "Armored Continuous Track Pods", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.55}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.20, 0.28, 1.36}, 0.12, "round_box")
            })}}),
            {0.76, 0.30, 0}, {0,0,0}, {1,1,1}, "#18181b", 0.30, 0.76, true, 1, "smooth", "", 0.0, "rubber_tire"));

        parts.push_back(mk_part("tank_turret", "Low-Profile Reactive Armor Turret", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.05}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, -0.06}, {0.52, 0.20, 0.64}, 0.07, "round_box"),
                mk_cap({0, 0.02, 0.20}, {0, 0.0, 0.62}, 0.36, 0.22, 0.06)
            })}}),
            {0, 0.84, -0.06}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        parts.push_back(mk_part("smoothbore_cannon", "120mm Smoothbore Main Cannon & Muzzle Brake", "cylinder",
            JsonValue::object({{"radius", 0.068}, {"height", 1.85}, {"sections", 24}}),
            {0, 0.86, 1.18}, {88, 0, 0}, {1,1,1}, acc.hex, 0.88, 0.28, false, 1, "smooth", "", 0.0, "forged_metal"));

        bp["title"] = "Armored Main Battle Tank";
        bp["category"] = "vehicle";

    } else if (contains_any(low, {"wolf", "dog", "hound", "fox", "cat", "lion", "tiger", "panther", "horse", "stag", "deer", "bear"})) {
        bool is_horse = contains_any(low, {"horse", "stallion", "stag", "deer"});
        MatSpec prim{is_horse ? "#78350f" : "#64748b", 0.08, 0.74}, acc{"#e2e8f0", 0.06, 0.68};
        std::string glow = "#fbbf24";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("quad_torso", "Anatomical Ribcage, Shoulders & Haunches", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_cap({0, 0.06, 0.46}, {0, 0.02, -0.44}, 0.32, 0.25, 0.14),
                mk_cap({0, 0.10, 0.42}, {0, 0.48, 0.76}, 0.24, 0.16, 0.12)
            })}}),
            {0, is_horse ? 1.18 : 0.92, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("quad_head", "Sculpted Cranium, Muzzle & Pointed Ears", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.65}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.04, -0.04}, {0.17, 0.16, 0.20}, 0.06),
                mk_cap({0, 0.0, 0.06}, {0, -0.04, 0.34}, 0.13, 0.075, 0.06),
                mk_cap({ 0.10, 0.14, -0.06}, { 0.14, 0.32, -0.04}, 0.055, 0.015, 0.03),
                mk_cap({-0.10, 0.14, -0.06}, {-0.14, 0.32, -0.04}, 0.055, 0.015, 0.03)
            })}}),
            {0, is_horse ? 1.72 : 1.42, 0.84}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("quad_forelegs", "Muscular Forelimbs & Paws", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.95}, {"lobes", JsonValue::array({
                mk_cap({0, 0.38, 0.02}, {0, 0.02, 0.04}, 0.11, 0.075, 0.05),
                mk_cap({0, 0.02, 0.04}, {0, -0.38, 0.06}, 0.072, 0.055, 0.04),
                mk_lobe({0, -0.40, 0.10}, {0.075, 0.05, 0.10}, 0.03, "round_box")
            })}}),
            {0.22, is_horse ? 0.62 : 0.48, 0.44}, {0,0,0}, {1, is_horse ? 1.32 : 1.0, 1}, prim.hex, prim.met, prim.rgh, true, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("quad_hindlegs", "Anatomical Hindquarters, Hocks & Paws", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.95}, {"lobes", JsonValue::array({
                mk_cap({0, 0.40, 0.02}, {0, 0.04, -0.08}, 0.13, 0.08, 0.05),
                mk_cap({0, 0.04, -0.08}, {0, -0.38, 0.02}, 0.075, 0.055, 0.04),
                mk_lobe({0, -0.40, 0.06}, {0.075, 0.05, 0.10}, 0.03, "round_box")
            })}}),
            {0.23, is_horse ? 0.62 : 0.48, -0.42}, {0,0,0}, {1, is_horse ? 1.32 : 1.0, 1}, prim.hex, prim.met, prim.rgh, true, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("quad_tail", "Flowing Sculpted Tail", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.75}, {"lobes", JsonValue::array({
                mk_cap({0, 0.18, 0.24}, {0, -0.18, -0.34}, 0.095, 0.045, 0.06)
            })}}),
            {0, is_horse ? 1.12 : 0.86, -0.68}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        bp["title"] = is_horse ? "Sculpted Anatomical Stallion" : "Sculpted Anatomical Dire Wolf";
        bp["category"] = "creature";

    } else if (contains_any(low, {"house", "cottage", "cabin", "villa", "home", "hut"})) {
        MatSpec prim{"#e7e5e4", 0.04, 0.86}, acc{"#991b1b", 0.08, 0.74};
        std::string glow = "#fbbf24";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("house_foundation", "Stone Terrace Foundation", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.50}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {1.18, 0.14, 0.96}, 0.05, "round_box")
            })}}),
            {0, 0.14, 0}, {0,0,0}, {1,1,1}, "#57534e", 0.05, 0.90, false, 1, "stone", "", 0.0, "weathered_stone"));

        parts.push_back(mk_part("house_walls", "Plastered Timber-Frame Walls & Recessed Doorway", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.35}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.96, 0.56, 0.78}, 0.05, "round_box"),
                mk_lobe({0, -0.22, 0.78}, {0.18, 0.34, 0.10}, 0.02, "round_box", "subtract")
            })}}),
            {0, 0.82, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "stone", "", 0.0, "weathered_stone"));

        parts.push_back(mk_part("pitched_roof", "Pitched Gable Tile Roof", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_lobe({-0.36, 0, 0}, {0.68, 0.08, 0.90}, 0.04, "round_box")
            })}}),
            {0, 1.56, 0}, {0, 0, 32}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "weathered_stone"));

        parts.push_back(mk_part("stone_chimney", "Masonry Fireplace Chimney", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.65}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.16, 0.48, 0.16}, 0.03, "round_box")
            })}}),
            {0.62, 1.72, -0.28}, {0,0,0}, {1,1,1}, "#78716c", 0.05, 0.88, false, 1, "stone", "", 0.0, "weathered_stone"));

        parts.push_back(mk_part("warm_windows", "Warm Lit Cottage Windows", "capsule",
            JsonValue::object({{"radius", 0.11}, {"height", 0.18}}),
            {0.52, 0.92, 0.76}, {0,0,0}, {1.0, 1.0, 0.35}, glow, 0.05, 0.15, true, 1, "smooth", glow, 0.90));

        bp["title"] = "Sculpted Timber & Stone Cottage";
        bp["category"] = "architecture";

    } else if (contains_any(low, {"chair", "armchair", "throne", "sofa", "couch", "stool"})) {
        MatSpec prim{"#7c2d12", 0.08, 0.72}, acc{"#d4af37", 0.88, 0.24};
        std::string glow = "#fbbf24";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("seat_cushion", "Tufted Leather Seat & Sculpted Backrest", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.15}, {"lobes", JsonValue::array({
                mk_lobe({0, -0.22, 0.06}, {0.48, 0.12, 0.46}, 0.06, "round_box"),
                mk_lobe({0,  0.32, -0.32}, {0.46, 0.52, 0.12}, 0.07, "round_box"),
                mk_lobe({ 0.48, 0.0, 0.02}, {0.09, 0.22, 0.42}, 0.05, "round_box"),
                mk_lobe({-0.48, 0.0, 0.02}, {0.09, 0.22, 0.42}, 0.05, "round_box")
            })}}),
            {0, 0.76, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("chair_legs", "Turned Hardwood & Brass Legs", "cylinder",
            JsonValue::object({{"radius", 0.045}, {"height", 0.52}, {"sections", 20}}),
            {0.40, 0.26, 0.36}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 4, "smooth", "", 0.0, "forged_metal"));

        bp["title"] = "Sculpted Tufted Leather Armchair";
        bp["category"] = "furniture";

    } else if (contains_any(low, {"table", "desk", "workbench"})) {
        MatSpec prim{"#78350f", 0.06, 0.68}, acc{"#1e293b", 0.84, 0.30};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("tabletop_slab", "Beveled Live-Edge Walnut Tabletop", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {1.12, 0.055, 0.66}, 0.03, "round_box")
            })}}),
            {0, 0.78, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("steel_trestle_legs", "Powder-Coated Steel Trestle Legs", "cylinder",
            JsonValue::object({{"radius", 0.045}, {"height", 0.76}, {"sections", 20}}),
            {0.92, 0.38, 0.50}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("rear_trestle_legs", "Rear Steel Trestle Legs", "cylinder",
            JsonValue::object({{"radius", 0.045}, {"height", 0.76}, {"sections", 20}}),
            {0.92, 0.38, -0.50}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "forged_metal"));

        bp["title"] = "Sculpted Walnut & Steel Studio Desk";
        bp["category"] = "furniture";

    } else if (contains_any(low, {"tree", "pine", "oak", "bonsai"})) {
        MatSpec prim{"#15803d", 0.04, 0.84}, acc{"#78350f", 0.05, 0.88};
        std::string glow = "#4ade80";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("gnarled_trunk", "Gnarled Bark Trunk & Root Flare", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.35}, {"lobes", JsonValue::array({
                mk_cap({0, -0.68, 0}, {0.06, 0.42, 0.02}, 0.24, 0.12, 0.12),
                mk_cap({0.06, 0.18, 0.02}, { 0.44, 0.62,  0.14}, 0.11, 0.05, 0.10),
                mk_cap({0.06, 0.24, 0.02}, {-0.40, 0.68, -0.12}, 0.11, 0.05, 0.10)
            })}}),
            {0, 0.78, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "organic_muscle", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("foliage_canopy", "Layered Organic Leaf Canopy", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.18, 0}, {0.72, 0.46, 0.70}, 0.18),
                mk_lobe({ 0.48, -0.06,  0.18}, {0.48, 0.34, 0.46}, 0.16),
                mk_lobe({-0.46,  0.02, -0.16}, {0.48, 0.36, 0.46}, 0.16)
            })}}),
            {0, 1.72, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scales", "", 0.0, "weathered_stone"));

        bp["title"] = "Sculpted Ancient Oak Tree";
        bp["category"] = "nature";

    } else if (contains_any(low, {"guitar", "violin", "cello", "lute", "ukulele"})) {
        MatSpec prim{"#b45309", 0.18, 0.26}, acc{"#18181b", 0.35, 0.30};
        std::string glow = "#fbbf24";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("guitar_body", "Figure-8 Sunburst Resonant Body & Soundhole", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.15}, {"lobes", JsonValue::array({
                mk_lobe({0, -0.26, 0}, {0.46, 0.42, 0.12}, 0.12),
                mk_lobe({0,  0.28, 0}, {0.35, 0.32, 0.11}, 0.12),
                mk_lobe({0,  0.12, 0.11}, {0.13, 0.13, 0.06}, 0.02, "ellipsoid", "subtract")
            })}}),
            {0, 0.78, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("fretboard_neck", "Rosewood Fretboard & Headstock", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.15}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.0, 0.02}, {0.065, 0.68, 0.032}, 0.015, "round_box"),
                mk_lobe({0, 0.78, -0.02}, {0.095, 0.16, 0.028}, 0.02, "round_box")
            })}}),
            {0, 1.68, 0.04}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("bridge_saddle", "Machined Brass Bridge & Pickups", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.45}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.16, 0.045, 0.035}, 0.01, "round_box")
            })}}),
            {0, 0.62, 0.12}, {0,0,0}, {1,1,1}, "#d4af37", 0.92, 0.20, false, 1, "smooth", "", 0.0, "forged_metal"));

        bp["title"] = "Sculpted Luthier Sunburst Guitar";
        bp["category"] = "prop";

    } else if (contains_any(low, {"shield", "buckler", "aegis", "helmet", "crown"})) {
        bool is_helm = contains_any(low, {"helmet", "crown"});
        MatSpec prim{"#94a3b8", 0.90, 0.22}, acc{"#d4af37", 0.94, 0.18};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);

        if (is_helm) {
            parts.push_back(mk_part("spartan_helm", "Forged Corinthian War Helmet", "sculpted_sdf",
                JsonValue::object({{"bounds", 0.95}, {"lobes", JsonValue::array({
                    mk_lobe({0, 0.08, -0.02}, {0.36, 0.42, 0.40}, 0.08),
                    mk_lobe({0, -0.08, 0.36}, {0.08, 0.24, 0.12}, 0.02, "round_box", "subtract"),
                    mk_lobe({0.16, 0.06, 0.34}, {0.11, 0.05, 0.10}, 0.015, "round_box", "subtract"),
                    mk_lobe({-0.16, 0.06, 0.34}, {0.11, 0.05, 0.10}, 0.015, "round_box", "subtract")
                })}}),
                {0, 0.92, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "damascus_steel"));
            parts.push_back(mk_part("helm_crest", "Sculpted Horsehair Plume Crest", "sculpted_sdf",
                JsonValue::object({{"bounds", 0.85}, {"lobes", JsonValue::array({
                    mk_cap({0, 0.12, 0.32}, {0, -0.10, -0.44}, 0.14, 0.10, 0.06)
                })}}),
                {0, 1.42, -0.04}, {0,0,0}, {0.42, 1.0, 1.0}, "#dc2626", 0.12, 0.72, false, 1, "organic_muscle", "", 0.0, "wrapped_leather"));
            bp["title"] = "Forged Steel War Helmet";
        } else {
            parts.push_back(mk_part("heater_shield", "Domed Steel & Gold-Trimmed Kite Shield", "sculpted_sdf",
                JsonValue::object({{"bounds", 1.25}, {"lobes", JsonValue::array({
                    mk_cap({0, 0.42, 0.04}, {0, -0.56, 0.02}, 0.56, 0.18, 0.10)
                })}}),
                {0, 1.05, 0}, {0,0,0}, {1.0, 1.0, 0.24}, prim.hex, prim.met, prim.rgh, false, 1, "smooth", "", 0.0, "damascus_steel"));
            parts.push_back(mk_part("shield_boss", "Gilded Central Umbo Boss", "torus",
                JsonValue::object({{"major_r", 0.22}, {"minor_r", 0.042}}),
                {0, 1.12, 0.14}, {90, 0, 0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "gold_filigree"));
            bp["title"] = "Forged Heraldic Kite Shield";
        }
        bp["category"] = "armor";

    } else if (contains_any(low, {"castle", "tower", "watchtower", "citadel", "fortress", "keep", "building", "cathedral"})) {
        MatSpec prim{"#78716c", 0.05, 0.88}, acc{"#1e3a8a", 0.16, 0.62};
        std::string glow = "#fbbf24";
        extract_palette_cpp(prompt, prim, acc, glow);

        // 1. Craggy Stone Cliff Foundation
        parts.push_back(mk_part("cliff_plinth", "Craggy Stone Fortress Foundation", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {1.12, 0.24, 1.12}, 0.12, "round_box"),
                mk_lobe({0.35, -0.04, 0.35}, {0.65, 0.22, 0.65}, 0.14, "round_box")
            })}}),
            {0, 0.20, 0}, {0,0,0}, {1,1,1}, "#57534e", 0.05, 0.92, false, 1, "stone", "", 0.0, "weathered_stone"));

        // 2. Masonry Keep with Boolean-Carved Gothic Gate Arch & Embrasures
        parts.push_back(mk_part("masonry_keep", "Ashlar Masonry Keep & Recessed Arch", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_lobe({0, -0.25, 0}, {0.72, 0.68, 0.72}, 0.10, "round_box"),
                mk_lobe({0,  0.48, 0}, {0.64, 0.62, 0.64}, 0.10, "round_box"),
                // Carve deep arched gate portal
                mk_cap({0, -0.68, 0.72}, {0, -0.26, 0.72}, 0.24, 0.20, 0.03, "subtract")
            })}}),
            {0, 1.28, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "stone", "", 0.0, "weathered_stone"));

        // 3. 4 Radial Corner Bastion Turrets + Turret Roofs
        parts.push_back(mk_part("corner_turrets", "4 Radial Corner Bastion Turrets", "cylinder",
            JsonValue::object({{"radius", 0.28}, {"height", 2.15}, {"sections", 24}}),
            {0.66, 1.36, 0.66}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 4, "stone", "", 0.0, "weathered_stone"));

        parts.push_back(mk_part("turret_spires", "Conical Slate Turret Spires", "cone",
            JsonValue::object({{"radius", 0.34}, {"height", 0.72}}),
            {0.66, 2.78, 0.66}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 4, "smooth", "", 0.0, "weathered_stone"));

        // 4. Main Crenellated Battlements & Central Keep Spire
        parts.push_back(mk_part("keep_battlements", "Crenellated Stone Parapet", "battlement_ring",
            JsonValue::object({{"radius", 0.78}, {"height", 0.36}, {"merlons", 12}}),
            {0, 2.38, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "stone", "", 0.0, "weathered_stone"));

        parts.push_back(mk_part("central_spire", "Steep Central Keep Spire", "cone",
            JsonValue::object({{"radius", 0.66}, {"height", 1.15}}),
            {0, 3.06, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "weathered_stone"));

        // 5. Iron-Bound Timber Gate & Warm Lit Gothic Windows
        parts.push_back(mk_part("oak_portcullis", "Iron-Bound Timber Gate", "capsule",
            JsonValue::object({{"radius", 0.19}, {"height", 0.32}}),
            {0, 0.66, 0.60}, {0,0,0}, {1.0, 1.0, 0.35}, "#3b1d0e", 0.08, 0.84, false, 1, "smooth", "", 0.0, "wrapped_leather"));

        parts.push_back(mk_part("lancet_windows", "Warm Lit Gothic Lancet Windows", "capsule",
            JsonValue::object({{"radius", 0.065}, {"height", 0.24}}),
            {0, 1.82, 0.63}, {0,0,0}, {1,1,1}, glow, 0.05, 0.18, false, 4, "smooth", glow, 0.90));

        bp["title"] = "Medieval Bastion Citadel Keep";
        bp["category"] = "architecture";

    } else if (contains_any(low, {"gear", "cog", "planetary", "bearing", "mechanical", "turbine"})) {
        MatSpec prim{"#94a3b8", 0.90, 0.22}, acc{"#ca8a04", 0.88, 0.26};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);

        parts.push_back(mk_part("carrier_flange", "Machined Carrier Housing", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.55}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {1.38, 0.07, 1.38}, 0.03, "ellipsoid"),
                mk_lobe({0.95, 0, 0}, {0.14, 0.16, 0.14}, 0.02, "ellipsoid", "subtract"),
                mk_lobe({-0.95, 0, 0}, {0.14, 0.16, 0.14}, 0.02, "ellipsoid", "subtract")
            })}}),
            {0, 0.06, 0}, {0,0,0}, {1,1,1}, "#1e293b", 0.82, 0.36, false, 1, "smooth", "", 0.0, "forged_metal"));
        parts.push_back(mk_part("sun_gear", "Precision Helical Sun Gear (18T)", "gear",
            JsonValue::object({{"teeth", 18}, {"outer_r", 0.72}, {"inner_r", 0.22}, {"height", 0.30}}),
            {0, 0.26, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));
        parts.push_back(mk_part("planet_gears", "Meshing Planet Gears (12T x 3)", "gear",
            JsonValue::object({{"teeth", 12}, {"outer_r", 0.48}, {"inner_r", 0.14}, {"height", 0.28}}),
            {1.14, 0.26, 0}, {0, 15, 0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 3, "smooth", "", 0.0, "forged_metal"));
        parts.push_back(mk_part("hex_shaft", "Keyed Drive Shaft", "hex_prism",
            JsonValue::object({{"radius", 0.21}, {"height", 0.72}}),
            {0, 0.40, 0}, {0,0,0}, {1,1,1}, "#475569", 0.94, 0.18, false, 1, "smooth", "", 0.0, "damascus_steel"));

        bp["title"] = "Machined Planetary Spur Gear Assembly";
        bp["category"] = "mechanical";

    } else if (contains_any(low, {"mecha", "robot", "mech", "gundam", "droid", "knight", "guardian", "humanoid", "warrior", "armor", "cyborg"})) {
        MatSpec prim{"#1d4ed8", 0.78, 0.26}, acc{"#e2e8f0", 0.88, 0.22};
        std::string glow = "#22d3ee";
        extract_palette_cpp(prompt, prim, acc, glow);

        // 1. Multi-Layered Gundanium Chest Armor + Boolean Intake Vents + Hydraulic Abdomen
        parts.push_back(mk_part("mecha_chest_chassis", "Layered Chest Armor & Hydraulic Waist", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.25}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.22, 0.04}, {0.52, 0.34, 0.36}, 0.07, "round_box"),
                mk_lobe({ 0.26, 0.20, 0.24}, {0.22, 0.22, 0.20}, 0.05, "round_box"),
                mk_lobe({-0.26, 0.20, 0.24}, {0.22, 0.22, 0.20}, 0.05, "round_box"),
                mk_cap({0, 0.42, -0.02}, {0, 0.58, 0.02}, 0.16, 0.14, 0.05),
                mk_lobe({0, -0.18, 0.0}, {0.32, 0.26, 0.26}, 0.06, "round_box"),
                // Boolean carve chest turbine vents & central reactor socket!
                mk_lobe({ 0.26, 0.14, 0.42}, {0.13, 0.07, 0.09}, 0.018, "round_box", "subtract"),
                mk_lobe({-0.26, 0.14, 0.42}, {0.13, 0.07, 0.09}, 0.018, "round_box", "subtract"),
                mk_lobe({0.0, 0.24, 0.40}, {0.14, 0.14, 0.10}, 0.02, "ellipsoid", "subtract")
            })}}),
            {0, 1.76, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        // 2. Pelvis Block & Angled Front/Side Waist Armor Skirts
        parts.push_back(mk_part("pelvis_armor_skirts", "Articulated Pelvis & Hip Armor Skirts", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.95}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.06, 0.02}, {0.34, 0.20, 0.28}, 0.05, "round_box"),
                mk_lobe({0, -0.08, 0.24}, {0.13, 0.22, 0.12}, 0.04, "round_box"),
                mk_lobe({ 0.24, -0.12, 0.20}, {0.15, 0.24, 0.07}, 0.03, "round_box"),
                mk_lobe({-0.24, -0.12, 0.20}, {0.15, 0.24, 0.07}, 0.03, "round_box"),
                mk_lobe({ 0.40, -0.10, 0.0}, {0.08, 0.26, 0.24}, 0.03, "round_box"),
                mk_lobe({-0.40, -0.10, 0.0}, {0.08, 0.26, 0.24}, 0.03, "round_box")
            })}}),
            {0, 1.30, 0}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        // 3. Recessed Tokamak Chest Core
        parts.push_back(mk_part("tokamak_core", "Recessed Tokamak Fusion Core", "octa_crystal",
            JsonValue::object({{"height", 0.22}, {"radius", 0.13}, {"sides", 8}}),
            {0, 2.00, 0.34}, {90, 0, 0}, {1.0, 0.65, 1.0}, glow, 0.15, 0.08, false, 1, "smooth", glow, 0.98));

        // 4. Sculpted Combat Helmet with Boolean Visor Trench & Faceplate
        parts.push_back(mk_part("combat_helmet", "Sculpted Combat Helmet & Chin Guard", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.70}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.04, -0.02}, {0.23, 0.24, 0.27}, 0.06, "round_box"),
                mk_cap({0, 0.24, 0.10}, {0, 0.28, -0.22}, 0.06, 0.05, 0.04),
                mk_lobe({0, -0.10, 0.18}, {0.12, 0.10, 0.10}, 0.03, "round_box"),
                // Carve recessed optic visor slit!
                mk_lobe({0, 0.03, 0.24}, {0.22, 0.055, 0.09}, 0.015, "round_box", "subtract")
            })}}),
            {0, 2.50, 0.04}, {0,0,0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "scifi_panels", "", 0.0, "painted_armor"));

        parts.push_back(mk_part("optic_visor", "Recessed Glowing Optic Sensor", "capsule",
            JsonValue::object({{"radius", 0.038}, {"height", 0.26}}),
            {0, 2.53, 0.22}, {0, 0, 90}, {1, 0.85, 1}, glow, 0.1, 0.08, false, 1, "smooth", glow, 0.96));

        parts.push_back(mk_part("command_v_fin", "Swept Dual V-Fin Blade Crest", "wing",
            JsonValue::object({{"span", 0.44}, {"chord", 0.11}, {"thickness", 0.032}, {"sweep", -0.14}}),
            {0.03, 2.66, 0.24}, {0, 0, 28}, {1,1,1}, "#eab308", 0.92, 0.20, true, 1, "smooth", "", 0.0, "forged_metal"));

        // 5. Layered Beveled Shoulder Pauldrons
        parts.push_back(mk_part("shoulder_pauldrons", "Multi-Plate Vernier Shoulder Pauldrons", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.80}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.04, 0}, {0.28, 0.22, 0.30}, 0.05, "round_box"),
                mk_cap({0.10, 0.14, 0}, {0.34, 0.22, -0.04}, 0.11, 0.045, 0.05),
                mk_lobe({0.26, -0.02, 0}, {0.08, 0.06, 0.18}, 0.015, "round_box", "subtract")
            })}}),
            {0.72, 2.08, 0.0}, {0, 0, -10}, {1,1,1}, prim.hex, prim.met, prim.rgh, true, 1, "scifi_panels", "", 0.0, "painted_armor"));

        // 6. Articulated Biceps, Elbow Joints, Forearm Gauntlets & Mechanical Fists
        parts.push_back(mk_part("articulated_arms", "Biceps, Elbow Servos, Gauntlets & Fists", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.05}, {"lobes", JsonValue::array({
                mk_cap({0.0, 0.36, 0.0}, {0.04, 0.06, 0.02}, 0.12, 0.105, 0.04),
                mk_lobe({0.04, 0.02, 0.02}, {0.11, 0.09, 0.11}, 0.03, "ellipsoid"),
                mk_lobe({0.06, -0.22, 0.10}, {0.14, 0.22, 0.15}, 0.04, "round_box"),
                mk_lobe({0.06, -0.48, 0.16}, {0.10, 0.10, 0.11}, 0.03, "round_box")
            })}}),
            {0.74, 1.54, 0.02}, {0,0,0}, {1,1,1}, "#334155", 0.82, 0.32, true, 1, "scifi_panels", "", 0.0, "forged_metal"));

        // 7. Sculpted Thighs, Knee Spikes & Flared Vernier Calf Greaves
        parts.push_back(mk_part("sculpted_mech_legs", "Armored Thighs, Knee Guards & Vernier Greaves", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.20}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.34, 0.02}, {0.19, 0.28, 0.22}, 0.05, "round_box"),
                mk_lobe({0, 0.04, 0.18}, {0.13, 0.18, 0.12}, 0.04, "round_box"),
                mk_lobe({0, -0.28, -0.02}, {0.22, 0.34, 0.26}, 0.06, "round_box"),
                mk_cap({0, -0.10, -0.18}, {0, -0.36, -0.32}, 0.12, 0.08, 0.05),
                mk_lobe({0.20, -0.28, -0.06}, {0.06, 0.14, 0.09}, 0.015, "round_box", "subtract")
            })}}),
            {0.34, 0.74, 0.0}, {0, 0, -2}, {1,1,1}, prim.hex, prim.met, prim.rgh, true, 1, "scifi_panels", "", 0.0, "painted_armor"));

        // 8. Sculpted Split-Toe Mecha Stabilizer Boots (Zero plain boxes!)
        parts.push_back(mk_part("stabilizer_feet", "Articulated Split-Toe Stabilizer Feet", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.65}, {"lobes", JsonValue::array({
                mk_lobe({0, 0.08, 0.04}, {0.17, 0.10, 0.18}, 0.04, "round_box"),
                mk_cap({0, -0.02, 0.06}, {0, -0.06, 0.34}, 0.15, 0.11, 0.04),
                mk_cap({0, -0.02, -0.06}, {0, -0.06, -0.24}, 0.13, 0.09, 0.04),
                mk_lobe({0, -0.06, 0.32}, {0.03, 0.08, 0.10}, 0.01, "round_box", "subtract")
            })}}),
            {0.36, 0.14, 0.06}, {0,0,0}, {1,1,1}, "#1e293b", 0.86, 0.30, true, 1, "smooth", "", 0.0, "forged_metal"));

        // 9. Dorsal Flight Backpack + Twin Serrated Vernier Nozzles + Wing Binders
        parts.push_back(mk_part("dorsal_backpack", "Dorsal Flight Thruster Pack", "sculpted_sdf",
            JsonValue::object({{"bounds", 0.70}, {"lobes", JsonValue::array({
                mk_lobe({0, 0, 0}, {0.32, 0.28, 0.18}, 0.05, "round_box")
            })}}),
            {0, 1.98, -0.38}, {0,0,0}, {1,1,1}, "#1e293b", 0.84, 0.30, false, 1, "scifi_panels", "", 0.0, "carbon_fiber"));

        parts.push_back(mk_part("vernier_nozzles", "Twin Vectoring Backpack Verniers", "serrated_nozzle",
            JsonValue::object({{"radius", 0.13}, {"length", 0.28}, {"petals", 10}}),
            {0.18, 1.72, -0.44}, {22, 0, 0}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("wing_binders", "Swept Aero Wing-Binders", "wing",
            JsonValue::object({{"span", 1.22}, {"chord", 0.44}, {"thickness", 0.065}, {"sweep", 0.44}}),
            {0.24, 2.06, -0.44}, {14, -22, 20}, {1,1,1}, acc.hex, acc.met, acc.rgh, true, 1, "smooth", "", 0.0, "painted_armor"));

        bp["title"] = "Articulated Gundanium Mecha";
        bp["category"] = "mecha";

    } else {
        if (out_matched) *out_matched = false;
        MatSpec prim{"#94a3b8", 0.72, 0.28}, acc{"#d4af37", 0.92, 0.20};
        std::string glow = "#38bdf8";
        extract_palette_cpp(prompt, prim, acc, glow);
        bool is_gyroid = contains_any(low, {"gyroid", "lattice"});
        bool is_stone  = contains_any(low, {"stone", "statue", "ruin", "marble", "bust", "idol"});
        std::string detail = is_gyroid ? "gyroid" : (is_stone ? "stone" : "organic_muscle");
        std::string fin    = is_stone ? "weathered_stone" : "forged_metal";

        parts.push_back(mk_part("stepped_plinth", "Fluted Architectural Pedestal", "lathe_vase",
            JsonValue::object({{"height", 0.34}, {"mode", "column"}, {"flutes", 16}, {"twist", 0.0}}),
            {0, 0.17, 0}, {0,0,0}, {0.95, 1.0, 0.95}, "#27272a", 0.35, 0.65, false, 1, "smooth", "", 0.0, "weathered_stone"));

        parts.push_back(mk_part("sculpted_monolith", "Carved Sculptural Core", "sculpted_sdf",
            JsonValue::object({{"bounds", 1.45}, {"lobes", JsonValue::array({
                mk_cap({0, -0.48, 0}, {0, 0.46, 0}, 0.46, 0.34, 0.18),
                mk_cap({-0.36, -0.10, 0}, {0.36, 0.16, 0}, 0.24, 0.24, 0.18),
                mk_lobe({0, 0.08, 0.36}, {0.22, 0.28, 0.16}, 0.04, "ellipsoid", "subtract"),
                mk_lobe({0, 0.08, -0.36}, {0.22, 0.28, 0.16}, 0.04, "ellipsoid", "subtract")
            })}}),
            {0, 1.18, 0}, {0,0,0}, {1,1,1}, prim.hex, prim.met, prim.rgh, false, 1, detail, "", 0.0, fin));

        parts.push_back(mk_part("gimbal_outer", "Outer Astrolabe Gimbal Ring", "torus",
            JsonValue::object({{"major_r", 0.82}, {"minor_r", 0.048}}),
            {0, 1.22, 0}, {26, 18, 0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "forged_metal"));

        parts.push_back(mk_part("gimbal_inner", "Inner Engraved Gimbal Ring", "torus",
            JsonValue::object({{"major_r", 0.68}, {"minor_r", 0.036}}),
            {0, 1.22, 0}, {-32, -22, 0}, {1,1,1}, acc.hex, acc.met, acc.rgh, false, 1, "smooth", "", 0.0, "damascus_steel"));

        parts.push_back(mk_part("core_crystal", "Suspended Resonance Crystal", "octa_crystal",
            JsonValue::object({{"height", 0.56}, {"radius", 0.16}, {"sides", 8}}),
            {0, 1.26, 0}, {0,0,0}, {1,1,1}, glow, 0.18, 0.10, false, 1, "smooth", glow, 0.92));

        bp["title"] = prompt.size() > 38 ? prompt.substr(0, 38) : (prompt.empty() ? "Sculpted Astrolabe Relic" : prompt);
        bp["category"] = "prop";
    }

    bp["enhanced_prompt"] = prompt + " — sculpted via Falcon AI C++20 Anatomical SDF + Boolean CSG + Embedded 256x256 Albedo/ORM/Normal PBR Maps.";
    bp["parts"] = parts;
    return bp;
}

inline void sanitize_llm_blueprint(JsonValue& bp, const std::string& prompt) {
    if (!bp.is_object()) return;
    if (bp.get_str("title", "").empty()) {
        bp["title"] = prompt.size() > 42 ? prompt.substr(0, 42) : (prompt.empty() ? "Sculpted 3D Asset" : prompt);
    }
    if (bp.get_str("category", "").empty()) bp["category"] = "prop";
    if (bp.get_str("enhanced_prompt", "").empty()) {
        bp["enhanced_prompt"] = prompt + " — AI-architected multi-part C++20 SDF + PBR assembly.";
    }
    if (!bp.has("parts") || !bp["parts"].is_array()) return;

    auto& parts = bp["parts"];
    for (size_t i = 0; i < parts.size(); ++i) {
        auto& p = parts[i];
        if (!p.is_object()) continue;
        if (p.get_str("id", "").empty()) p["id"] = "part_" + std::to_string(i + 1);
        if (p.get_str("name", "").empty()) p["name"] = "Component " + std::to_string(i + 1);

        std::string pr = to_lower_str(p.get_str("primitive", "round_box"));
        if (pr == "box" || pr == "cube") pr = "round_box";
        else if (pr == "wheel" || pr == "tire") pr = "wheel_assembly";
        else if (pr == "ring") pr = "torus";
        else if (pr == "tube" || pr == "rod" || pr == "pipe") pr = "cylinder";
        else if (pr == "ball" || pr == "ellipsoid") pr = "sphere";
        else if (pr == "pill") pr = "capsule";
        else if (pr == "fin") pr = "wing";
        else if (pr == "vase" || pr == "lathe") pr = "lathe_vase";
        else if (pr == "crystal" || pr == "gem") pr = "octa_crystal";
        else if (pr == "nozzle" || pr == "exhaust") pr = "serrated_nozzle";
        else if (pr == "sculpt" || pr == "sdf") pr = "sculpted_sdf";
        p["primitive"] = pr;

        if (!p.has("params") || !p["params"].is_object()) p["params"] = JsonValue::object();

        auto fix_vec3 = [&](const char* key, double def_v, double min_v, double max_v, bool non_zero) {
            if (!p.has(key) || !p[key].is_array() || p[key].size() < 3) {
                p[key] = JsonValue::array({def_v, def_v, def_v});
            } else {
                for (size_t a = 0; a < 3; ++a) {
                    double v = std::clamp(p[key][a].as_num(def_v), min_v, max_v);
                    if (non_zero && std::fabs(v) < 0.04) v = (v < 0.0 ? -0.08 : 0.08);
                    p[key][a] = v;
                }
            }
        };
        fix_vec3("position", 0.0, -3.5, 3.5, false);
        fix_vec3("rotation", 0.0, -360.0, 360.0, false);
        fix_vec3("scale", 1.0, 0.05, 3.5, true);

        std::string col = p.get_str("color", "#94a3b8");
        if (col.size() != 7 || col[0] != '#') p["color"] = "#94a3b8";
        double met = std::clamp(p.get_num("metallic", 0.45), 0.0, 1.0);
        double rgh = std::clamp(p.get_num("roughness", 0.35), 0.04, 1.0);
        p["metallic"] = met;
        p["roughness"] = rgh;
        if (!p.has("mirror_x")) p["mirror_x"] = false;
        p["radial_count"] = std::clamp(p.get_int("radial_count", 1), 1, 16);
        if (p.get_str("detail_style", "").empty()) p["detail_style"] = "smooth";
        if (p.get_str("finish", "").empty()) {
            p["finish"] = (met >= 0.65) ? "forged_metal" : ((rgh >= 0.72) ? "wrapped_leather" : "painted_armor");
        }
    }
}

inline bool try_external_llm_blueprint(const RouterConfig& cfg, const std::string& sys_prompt, const std::string& user_msg, JsonValue& out_bp, std::string& out_provider, bool allow_keyless_cloud = false) {
    if (cfg.provider == "builtin_symbolic") return false;

    auto extract_bp = [&](const std::string& txt, JsonValue& parsed) -> bool {
        auto s = txt.find('{');
        auto e = txt.rfind('}');
        if (s == std::string::npos || e == std::string::npos || e <= s) return false;
        try {
            parsed = JsonValue::parse(std::string_view(txt).substr(s, e - s + 1));
            if (parsed.is_object() && parsed.has("parts") && parsed["parts"].is_array() && parsed["parts"].size() >= 3) {
                sanitize_llm_blueprint(parsed, user_msg);
                return true;
            }
            return false;
        } catch (...) {
            return false;
        }
    };

    if (cfg.provider == "auto" || cfg.provider == "ollama") {
        std::string model = cfg.ollama_model;
        if (model.empty()) {
            JsonValue tags;
            if (curl_util::http_get_json(cfg.ollama_url + "/api/tags", tags, 500) && tags["models"].size() > 0) {
                model = tags["models"][0].get_str("name", "");
            }
        }
        if (!model.empty()) {
            JsonValue req = JsonValue::object();
            req["model"] = model;
            req["stream"] = false;
            req["messages"] = JsonValue::array({
                JsonValue::object({{"role", "system"}, {"content", sys_prompt}}),
                JsonValue::object({{"role", "user"}, {"content", user_msg}})
            });
            JsonValue resp;
            if (curl_util::http_post_json(cfg.ollama_url + "/api/chat", req.dump(0), "", resp, 20000)) {
                std::string content = resp.at_or_null("message").get_str("content", "");
                if (extract_bp(content, out_bp)) {
                    out_provider = "Ollama (" + model + ")";
                    return true;
                }
            }
        }
    }

    if (cfg.provider == "auto" || cfg.provider == "lm_studio" || cfg.provider == "custom_openai" || cfg.provider == "openai_compat") {
        std::string base = cfg.openai_base_url;
        std::string model = cfg.openai_model;
        if (model.empty()) {
            JsonValue mres;
            if (curl_util::http_get_json(base + "/models", mres, 500) && mres["data"].size() > 0) {
                model = mres["data"][0].get_str("id", "local-model");
            }
        }
        if (!model.empty()) {
            JsonValue req = JsonValue::object();
            req["model"] = model;
            req["temperature"] = 0.25;
            req["messages"] = JsonValue::array({
                JsonValue::object({{"role", "system"}, {"content", sys_prompt}}),
                JsonValue::object({{"role", "user"}, {"content", user_msg}})
            });
            JsonValue resp;
            if (curl_util::http_post_json(base + "/chat/completions", req.dump(0), cfg.openai_api_key, resp, 18000)) {
                const auto& ch = resp.at_or_null("choices");
                if (ch.is_array() && ch.size() > 0) {
                    std::string content = ch[0].at_or_null("message").get_str("content", "");
                    if (extract_bp(content, out_bp)) {
                        out_provider = "OpenAI-Compatible (" + model + ")";
                        return true;
                    }
                }
            }
        }
    }

    // Zero-Key Free Cloud AI Fallback for novel 3D objects not in the built-in sculpted library!
    if (allow_keyless_cloud && cfg.provider == "auto") {
        struct KeylessEndpoint { const char* name; const char* url; const char* model; const char* key; bool low_reasoning; };
        static constexpr KeylessEndpoint kFreeCloud[] = {
            {"LLM7.io Free Cloud", "https://api.llm7.io/v1/chat/completions", "default", "", false},
            {"Pollinations Free Cloud", "https://text.pollinations.ai/openai/chat/completions", "openai", "", true}
        };
        for (const auto& ep : kFreeCloud) {
            JsonValue req = JsonValue::object();
            req["model"] = ep.model;
            req["temperature"] = 0.2;
            if (ep.low_reasoning) req["reasoning_effort"] = "low";
            req["messages"] = JsonValue::array({
                JsonValue::object({{"role", "system"}, {"content", sys_prompt}}),
                JsonValue::object({{"role", "user"}, {"content", "Design a detailed 6-part 3D object blueprint for: " + user_msg}})
            });
            JsonValue resp;
            if (curl_util::http_post_json(ep.url, req.dump(0), ep.key, resp, 23000)) {
                const auto& ch = resp.at_or_null("choices");
                if (ch.is_array() && ch.size() > 0) {
                    std::string content = ch[0].at_or_null("message").get_str("content", "");
                    if (extract_bp(content, out_bp)) {
                        out_provider = std::string(ep.name) + " + C++20 SDF/PBR";
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

inline std::pair<JsonValue, std::string> generate_blueprint_routed(const RouterConfig& cfg, const std::string& prompt) {
    static const std::string kSys =
        "Return ONLY valid JSON (no markdown) for a realistic multi-part 3D asset (6-10 parts, centered around x=0, ground at y=0, height ~1.2-2.2m, front=+Z): "
        "{\"title\":str,\"category\":str,\"enhanced_prompt\":str,\"parts\":[{\"id\":str,\"name\":str,"
        "\"primitive\":\"sculpted_sdf|wheel_assembly|serrated_nozzle|blade|gear|lathe_vase|thruster|chalice|column|wing|battlement_ring|octa_crystal|torus|capsule|cylinder|hex_prism|cone|sphere|round_box\","
        "\"params\":{},\"detail_style\":\"smooth|scifi_panels|stone|scales|organic_muscle\","
        "\"finish\":\"dragon_scales|damascus_steel|carbon_fiber|painted_armor|weathered_stone|wrapped_leather|carved_bone|rubber_tire|forged_metal\","
        "\"position\":[x,y,z],\"rotation\":[rx,ry,rz],\"scale\":[sx,sy,sz],"
        "\"mirror_x\":bool,\"radial_count\":1,\"color\":\"#RRGGBB\",\"metallic\":0.5,\"roughness\":0.3,\"emissive\":null,\"emissive_strength\":0.0}]}. "
        "For sculpted_sdf params use {\"bounds\":1.3,\"lobes\":[{\"shape\":\"round_box|ellipsoid\",\"center\":[x,y,z],\"radii\":[rx,ry,rz],\"blend\":0.08,\"op\":\"smooth_union|subtract\"}]} "
        "or {\"shape\":\"tapered_capsule\",\"p0\":[x0,y0,z0],\"p1\":[x1,y1,z1],\"r0\":0.2,\"r1\":0.1,\"blend\":0.08}. "
        "For cylinder/cone/capsule use {\"radius\":0.2,\"height\":0.8}; sphere {\"radius\":0.3}; torus {\"major_r\":0.4,\"minor_r\":0.06}; wheel_assembly {\"radius\":0.3,\"width\":0.22,\"spokes\":10}; round_box {\"extents\":[0.5,0.3,0.5],\"bevel\":0.05}.";

    bool matched_builtin = false;
    JsonValue symbolic_bp = compile_prompt_cpp_symbolic(prompt, &matched_builtin);

    JsonValue bp;
    std::string prov;
    // Try local/configured LLM first; if prompt is a novel object (!matched_builtin), also unlock keyless Free Cloud AI!
    if (try_external_llm_blueprint(cfg, kSys, prompt, bp, prov, !matched_builtin)) {
        return {bp, prov};
    }
    return {symbolic_bp, "Falcon C++20 Sculpted SDF + PBR Architect"};
}

inline JsonValue agent_chat_cpp(const RouterConfig& cfg, const std::string& message, const JsonValue* current_bp) {
    std::string low = to_lower_str(message);
    bool is_new_build_cmd = contains_any(low, {"create", "build", "generate", "synthesize", "model of", "make a", "make an", "make me"});
    bool is_edit = !is_new_build_cmd &&
                   (current_bp != nullptr && current_bp->is_object() && current_bp->has("parts")) &&
                   contains_any(low, {"make it", "change", "color", "recolor", "gold", "silver", "obsidian", "chrome", "matte", "damascus", "carbon",
                                      "glow", "neon", "plasma", "taller", "wider", "bigger", "larger", "add wing", "add base", "pedestal"});

    if (is_edit) {
        JsonValue bp = *current_bp;
        std::vector<std::string> edits;

        MatSpec prim{"", -1.0, -1.0}, acc{"", -1.0, -1.0};
        std::string glow = "";
        extract_palette_cpp(message, prim, acc, glow);

        if (!prim.hex.empty()) {
            for (size_t i = 0; i < bp["parts"].size(); ++i) {
                auto& p = bp["parts"][i];
                if (!p.has("emissive") || p.get_str("emissive").empty()) {
                    p["color"] = prim.hex;
                    p["metallic"] = prim.met;
                    p["roughness"] = prim.rgh;
                    if (low.find("damascus") != std::string::npos) p["finish"] = "damascus_steel";
                    else if (low.find("carbon") != std::string::npos) p["finish"] = "carbon_fiber";
                }
            }
            edits.push_back("applied PBR shader & normal maps `" + prim.hex + "`");
        }

        if (contains_any(low, {"chrome", "mirror", "shiny"})) {
            for (size_t i = 0; i < bp["parts"].size(); ++i) {
                bp["parts"][i]["metallic"] = 0.97;
                bp["parts"][i]["roughness"] = 0.08;
            }
            edits.push_back("upgraded surfaces to mirror chrome");
        } else if (contains_any(low, {"matte", "rough"})) {
            for (size_t i = 0; i < bp["parts"].size(); ++i) {
                bp["parts"][i]["metallic"] = 0.08;
                bp["parts"][i]["roughness"] = 0.85;
            }
            edits.push_back("set matte high-roughness finish");
        }

        if (contains_any(low, {"glow", "neon", "plasma", "emissive", "cyan", "purple", "fire"})) {
            std::string gcol = glow.empty() ? "#22d3ee" : glow;
            bool hit = false;
            for (size_t i = 0; i < bp["parts"].size(); ++i) {
                auto& p = bp["parts"][i];
                std::string pn = to_lower_str(p.get_str("name"));
                if (p.has("emissive") || contains_any(pn, {"core", "gem", "visor", "eye", "conduit", "light", "window"})) {
                    p["color"] = gcol;
                    p["emissive"] = gcol;
                    p["emissive_strength"] = 0.95;
                    hit = true;
                }
            }
            if (!hit && bp["parts"].size() > 0) {
                auto& last = bp["parts"][bp["parts"].size() - 1];
                last["emissive"] = gcol;
                last["emissive_strength"] = 0.92;
            }
            edits.push_back("energized emissive channels (`" + gcol + "`)");
        }

        if (contains_any(low, {"taller", "longer", "stretch"})) {
            for (size_t i = 0; i < bp["parts"].size(); ++i) {
                auto& p = bp["parts"][i];
                p["scale"][1] = p["scale"][1].as_num(1.0) * 1.30;
                p["position"][1] = p["position"][1].as_num(0.0) * 1.25;
            }
            edits.push_back("scaled vertical height +30%");
        } else if (contains_any(low, {"bigger", "larger", "wider"})) {
            for (size_t i = 0; i < bp["parts"].size(); ++i) {
                auto& p = bp["parts"][i];
                for (size_t a = 0; a < 3; ++a) {
                    p["scale"][a] = p["scale"][a].as_num(1.0) * 1.25;
                    p["position"][a] = p["position"][a].as_num(0.0) * 1.25;
                }
            }
            edits.push_back("scaled assembly proportions 1.25x");
        }

        if (low.find("wing") != std::string::npos) {
            bp["parts"].push_back(mk_part(
                "agent_wings_" + std::to_string(bp["parts"].size() + 1),
                "Anatomical Dragon Wings",
                "bat_wing",
                JsonValue::object({{"span", 1.75}, {"chord", 0.95}}),
                {0.30, 1.40, -0.05}, {12, -12, 18}, {1,1,1}, "#991b1b", 0.22, 0.55, true, 1, "scales", "", 0.0, "dragon_scales"
            ));
            edits.push_back("attached bilateral anatomical wings");
        }

        if (contains_any(low, {"base", "pedestal", "stand"})) {
            bp["parts"].push_back(mk_part(
                "print_pedestal_base",
                "Sculpted Stone Display Base",
                "sculpted_sdf",
                JsonValue::object({{"bounds", 1.3}, {"lobes", JsonValue::array({mk_lobe({0,0,0}, {1.05, 0.16, 1.05}, 0.08, "round_box")})}}),
                {0, 0.14, 0}, {0,0,0}, {1,1,1}, "#292524", 0.08, 0.88, false, 1, "stone", "", 0.0, "weathered_stone"
            ));
            edits.push_back("added sculpted stone display base");
        }

        if (!edits.empty()) {
            std::string summary = "Modified active 3D assembly in C++20 (0 credits used): ";
            for (size_t i = 0; i < edits.size(); ++i) {
                if (i > 0) summary += ", ";
                summary += edits[i];
            }
            summary += ".";
            JsonValue res = JsonValue::object();
            res["action"] = "updated_model";
            res["provider"] = "Falcon C++20 Sculpted SDF + PBR Architect";
            res["reply"] = summary;
            res["blueprint"] = bp;
            return res;
        }
    }

    auto [new_bp, prov] = generate_blueprint_routed(cfg, message);
    JsonValue res = JsonValue::object();
    res["action"] = "created_model";
    res["provider"] = prov;
    res["reply"] = "Sculpted **" + new_bp.get_str("title", "3D Asset") + "** (" +
                   std::to_string(new_bp["parts"].size()) + " PBR-textured semantic parts) in native C++20 via `" + prov + "`.";
    res["blueprint"] = new_bp;
    return res;
}

} // namespace openmesh
