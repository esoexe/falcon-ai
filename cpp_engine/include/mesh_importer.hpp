#pragma once
/**
 * Falcon AI v3.0 — Universal External 3D Mesh Importer (.GLB, .OBJ, Binary/ASCII .STL, .PLY).
 * Enables Meshy-style "Upload Your Own 3D Model" to Retexture (2K/4K PBR), Smart Quad-Remesh,
 * Auto-Rig & Animate, Repair Manifold, or CAD-Scale in Millimeters.
 */

#include "exporters.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace openmesh {

namespace imp_detail {

inline std::string to_lower_ascii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline Scene3D parse_obj_bytes(const std::vector<uint8_t>& bytes, const std::string& stem) {
    std::string text(bytes.begin(), bytes.end());
    std::istringstream iss(text);
    std::string line;

    std::vector<Vec3> global_v;
    Scene3D scene;
    Mesh cur;
    cur.id = "imported_part_1";
    cur.name = stem.empty() ? "Imported Mesh" : stem;
    cur.material = {"ImportedMat", "#94a3b8", 0.30f, 0.42f, "", 0.0f, "forged_metal", 512};

    auto flush_part = [&]() {
        if (!cur.faces.empty()) {
            // Compact only referenced vertices into part-local positions
            std::unordered_map<uint32_t, uint32_t> vmap;
            Mesh compact;
            compact.id = cur.id;
            compact.name = cur.name;
            compact.material = cur.material;
            for (const auto& f : cur.faces) {
                std::array<uint32_t, 3> nf{};
                for (int k = 0; k < 3; ++k) {
                    auto it = vmap.find(f[k]);
                    if (it == vmap.end()) {
                        uint32_t nid = static_cast<uint32_t>(compact.positions.size());
                        vmap[f[k]] = nid;
                        compact.positions.push_back(global_v[f[k]]);
                        nf[k] = nid;
                    } else {
                        nf[k] = it->second;
                    }
                }
                compact.faces.push_back(nf);
            }
            compact.recompute_normals();
            scene.parts.push_back(std::move(compact));
            cur.faces.clear();
        }
    };

    int part_no = 1;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ls(line);
        std::string tok;
        ls >> tok;
        if (tok == "v") {
            float x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            global_v.push_back({x, y, z});
        } else if (tok == "o" || tok == "g") {
            flush_part();
            std::string gname;
            std::getline(ls, gname);
            while (!gname.empty() && std::isspace(static_cast<unsigned char>(gname.front()))) gname.erase(gname.begin());
            ++part_no;
            cur.id = "part_" + std::to_string(part_no);
            cur.name = gname.empty() ? cur.id : gname;
        } else if (tok == "f") {
            std::vector<uint32_t> poly;
            std::string vtok;
            while (ls >> vtok) {
                size_t slash = vtok.find('/');
                std::string idx_str = (slash == std::string::npos) ? vtok : vtok.substr(0, slash);
                if (idx_str.empty()) continue;
                int raw = std::stoi(idx_str);
                int idx = (raw > 0) ? (raw - 1) : (static_cast<int>(global_v.size()) + raw);
                if (idx >= 0 && static_cast<size_t>(idx) < global_v.size()) {
                    poly.push_back(static_cast<uint32_t>(idx));
                }
            }
            for (size_t i = 1; i + 1 < poly.size(); ++i) {
                cur.faces.push_back({poly[0], poly[i], poly[i + 1]});
            }
        }
    }
    flush_part();
    return scene;
}

inline Scene3D parse_stl_bytes(const std::vector<uint8_t>& bytes, const std::string& stem) {
    Mesh m;
    m.id = "stl_body";
    m.name = stem.empty() ? "Imported STL" : stem;
    m.material = {"STLMat", "#cbd5e1", 0.25f, 0.45f, "", 0.0f, "forged_metal", 512};

    bool looks_binary = false;
    if (bytes.size() >= 84) {
        uint32_t tri_count = 0;
        std::memcpy(&tri_count, bytes.data() + 80, 4);
        size_t expected = 84ULL + static_cast<size_t>(tri_count) * 50ULL;
        if (tri_count > 0 && (expected == bytes.size() || (expected < bytes.size() && bytes.size() - expected < 64))) {
            looks_binary = true;
            m.positions.reserve(static_cast<size_t>(tri_count) * 3);
            m.faces.reserve(tri_count);
            for (uint32_t t = 0; t < tri_count; ++t) {
                const uint8_t* ptr = bytes.data() + 84 + static_cast<size_t>(t) * 50;
                uint32_t base = static_cast<uint32_t>(m.positions.size());
                for (int k = 0; k < 3; ++k) {
                    float vx = 0, vy = 0, vz = 0;
                    std::memcpy(&vx, ptr + 12 + k * 12 + 0, 4);
                    std::memcpy(&vy, ptr + 12 + k * 12 + 4, 4);
                    std::memcpy(&vz, ptr + 12 + k * 12 + 8, 4);
                    // Convert Z-up STL to Y-up Studio space
                    m.positions.push_back({vx, vz, -vy});
                }
                m.faces.push_back({base, base + 1, base + 2});
            }
        }
    }

    if (!looks_binary) {
        std::string text(bytes.begin(), bytes.end());
        std::istringstream iss(text);
        std::string tok;
        std::vector<Vec3> tri;
        while (iss >> tok) {
            if (tok == "vertex") {
                float x = 0, y = 0, z = 0;
                iss >> x >> y >> z;
                tri.push_back({x, z, -y});
                if (tri.size() == 3) {
                    uint32_t base = static_cast<uint32_t>(m.positions.size());
                    m.positions.push_back(tri[0]);
                    m.positions.push_back(tri[1]);
                    m.positions.push_back(tri[2]);
                    m.faces.push_back({base, base + 1, base + 2});
                    tri.clear();
                }
            }
        }
    }

    m.weld_vertices(1e-5f);
    m.recompute_normals();
    Scene3D sc;
    if (!m.faces.empty()) sc.parts.push_back(std::move(m));
    return sc;
}

inline Scene3D parse_ply_bytes(const std::vector<uint8_t>& bytes, const std::string& stem) {
    std::string text(bytes.begin(), bytes.end());
    std::istringstream iss(text);
    std::string line;
    size_t v_count = 0, f_count = 0;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string a, b;
        ls >> a >> b;
        if (a == "element" && b == "vertex") ls >> v_count;
        else if (a == "element" && b == "face") ls >> f_count;
        else if (a == "end_header") break;
    }

    Mesh m;
    m.id = "ply_body";
    m.name = stem.empty() ? "Imported PLY" : stem;
    m.material = {"PLYMat", "#94a3b8", 0.25f, 0.45f, "", 0.0f, "forged_metal", 512};

    for (size_t i = 0; i < v_count && std::getline(iss, line); ++i) {
        std::istringstream ls(line);
        float x = 0, y = 0, z = 0;
        ls >> x >> y >> z;
        m.positions.push_back({x, y, z});
    }
    for (size_t i = 0; i < f_count && std::getline(iss, line); ++i) {
        std::istringstream ls(line);
        int n = 0;
        ls >> n;
        std::vector<uint32_t> poly;
        for (int k = 0; k < n; ++k) {
            uint32_t idx = 0;
            ls >> idx;
            if (idx < m.positions.size()) poly.push_back(idx);
        }
        for (size_t k = 1; k + 1 < poly.size(); ++k) {
            m.faces.push_back({poly[0], poly[k], poly[k + 1]});
        }
    }
    m.recompute_normals();
    Scene3D sc;
    if (!m.faces.empty()) sc.parts.push_back(std::move(m));
    return sc;
}

} // namespace imp_detail

inline Scene3D import_external_mesh_bytes(const std::vector<uint8_t>& bytes, const std::string& filename) {
    if (bytes.size() < 8) throw std::runtime_error("Empty or truncated mesh file");

    std::string ext;
    size_t dot = filename.find_last_of('.');
    std::string stem = (dot == std::string::npos) ? filename : filename.substr(0, dot);
    size_t slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    if (dot != std::string::npos) ext = imp_detail::to_lower_ascii(filename.substr(dot + 1));

    bool magic_glb = bytes.size() >= 12 && bytes[0] == 'g' && bytes[1] == 'l' && bytes[2] == 'T' && bytes[3] == 'F';
    Scene3D sc;
    if (magic_glb || ext == "glb") {
        sc = load_glb_bytes(bytes);
    } else if (ext == "obj") {
        sc = imp_detail::parse_obj_bytes(bytes, stem);
    } else if (ext == "stl") {
        sc = imp_detail::parse_stl_bytes(bytes, stem);
    } else if (ext == "ply") {
        sc = imp_detail::parse_ply_bytes(bytes, stem);
    } else {
        // Fallback auto-detect
        sc = imp_detail::parse_obj_bytes(bytes, stem);
        if (sc.parts.empty()) sc = imp_detail::parse_stl_bytes(bytes, stem);
    }

    if (sc.parts.empty()) {
        throw std::runtime_error("Could not parse 3D geometry (supported: .GLB, .OBJ, .STL, .PLY)");
    }

    // Normalize extreme CAD/millimeter imports (e.g., 150.0 units STL) into Studio bounds (~2.2 units tall)
    AABB box = sc.bounds();
    if (box.valid()) {
        Vec3 ext_v = box.extents();
        float max_e = std::max({ext_v.x, ext_v.y, ext_v.z});
        if (max_e > 12.0f || max_e < 0.08f) {
            float target_u = (max_e > 12.0f) ? (1.0f / kDefaultMmPerUnit) : (1.8f / std::max(max_e, 1e-5f));
            if (max_e * target_u > 8.0f || max_e * target_u < 0.25f) {
                target_u = 2.0f / std::max(max_e, 1e-5f);
            }
            Vec3 c = box.center();
            for (auto& m : sc.parts) {
                m.translate(-c.x, -c.y, -c.z);
                m.apply_transform(Mat4::scale(target_u, target_u, target_u));
            }
        }
        sc.ground_on_y0();
    }
    return sc;
}

} // namespace openmesh
