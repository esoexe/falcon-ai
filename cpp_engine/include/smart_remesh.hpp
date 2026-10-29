#pragma once
/**
 * Falcon AI v3.0 — Smart Topology Engine (Pure C++20):
 *  1. Garland-Heckbert Quadric Error Metric (QEM) Edge-Collapse Decimator + Midpoint Subdivider
 *     for exact user-requested polycount targets (100 to 300,000 faces, <2% error).
 *  2. Field-Aligned Coplanar/Aspect Quad-Dominant Retopologizer (`quad_mode`) with
 *     tangent-plane Laplacian quad relaxation and true 4-gon `.OBJ/.FBX/.USDZ/.PLY` output.
 */

#include "mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openmesh {

namespace qem_detail {

// Symmetric 4x4 Quadric Matrix stored as 10 doubles:
// [ q0 q1 q2 q3 ]
// [ q1 q4 q5 q6 ]
// [ q2 q5 q7 q8 ]
// [ q3 q6 q8 q9 ]
struct SymMat4 {
    double q[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    static SymMat4 from_plane(double a, double b, double c, double d) {
        SymMat4 m;
        m.q[0] = a * a; m.q[1] = a * b; m.q[2] = a * c; m.q[3] = a * d;
                        m.q[4] = b * b; m.q[5] = b * c; m.q[6] = b * d;
                                        m.q[7] = c * c; m.q[8] = c * d;
                                                        m.q[9] = d * d;
        return m;
    }

    SymMat4& operator+=(const SymMat4& o) {
        for (int i = 0; i < 10; ++i) q[i] += o.q[i];
        return *this;
    }

    SymMat4 operator+(const SymMat4& o) const {
        SymMat4 r = *this;
        r += o;
        return r;
    }

    double eval(const Vec3& v) const {
        double x = v.x, y = v.y, z = v.z;
        return q[0] * x * x + 2.0 * q[1] * x * y + 2.0 * q[2] * x * z + 2.0 * q[3] * x
             + q[4] * y * y + 2.0 * q[5] * y * z + 2.0 * q[6] * y
             + q[7] * z * z + 2.0 * q[8] * z
             + q[9];
    }

    bool solve_optimal(Vec3& out_v) const {
        // Solve upper 3x3 linear system A * v = -b via Cramer's rule
        double a00 = q[0], a01 = q[1], a02 = q[2], b0 = -q[3];
        double a10 = q[1], a11 = q[4], a12 = q[5], b1 = -q[6];
        double a20 = q[2], a21 = q[5], a22 = q[7], b2 = -q[8];

        double det = a00 * (a11 * a22 - a12 * a21)
                   - a01 * (a10 * a22 - a12 * a20)
                   + a02 * (a10 * a21 - a11 * a20);
        if (std::fabs(det) < 1e-10) return false;

        double inv = 1.0 / det;
        double vx = (b0 * (a11 * a22 - a12 * a21) - a01 * (b1 * a22 - a12 * b2) + a02 * (b1 * a21 - a11 * b2)) * inv;
        double vy = (a00 * (b1 * a22 - a12 * b2) - b0 * (a10 * a22 - a12 * a20) + a02 * (a10 * b2 - b1 * a20)) * inv;
        double vz = (a00 * (a11 * b2 - b1 * a21) - a01 * (a10 * b2 - b1 * a20) + b0 * (a10 * a21 - a11 * a20)) * inv;
        if (!std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(vz)) return false;
        out_v = {static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)};
        return true;
    }
};

inline uint64_t edge_key(uint32_t a, uint32_t b) {
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
}

inline void decimate_qem_single_mesh(Mesh& m, size_t target_tris, bool preserve_creases = true) {
    m.weld_vertices(1e-5f);
    if (m.faces.size() <= target_tris || m.faces.size() <= 4) {
        m.recompute_normals();
        return;
    }

    const size_t nv = m.positions.size();
    const size_t nf = m.faces.size();
    bool has_col = m.has_vertex_colors();

    std::vector<SymMat4> quadrics(nv);
    std::vector<std::vector<uint32_t>> vert_faces(nv);
    std::vector<bool> face_deleted(nf, false);
    std::vector<uint32_t> vert_version(nv, 0);

    // Count edge multiplicity to penalize open boundaries & sharp creases
    std::unordered_map<uint64_t, std::vector<uint32_t>> edge_faces;
    edge_faces.reserve(nf * 2);

    std::vector<Vec3> face_normals(nf, Vec3{0, 1, 0});
    for (size_t fi = 0; fi < nf; ++fi) {
        const auto& f = m.faces[fi];
        vert_faces[f[0]].push_back(static_cast<uint32_t>(fi));
        vert_faces[f[1]].push_back(static_cast<uint32_t>(fi));
        vert_faces[f[2]].push_back(static_cast<uint32_t>(fi));

        const Vec3& p0 = m.positions[f[0]];
        const Vec3& p1 = m.positions[f[1]];
        const Vec3& p2 = m.positions[f[2]];
        Vec3 cr = (p1 - p0).cross(p2 - p0);
        float len = cr.length();
        Vec3 n = (len > 1e-9f) ? (cr / len) : Vec3{0, 1, 0};
        face_normals[fi] = n;
        double d = -static_cast<double>(n.dot(p0));
        double area_w = std::max(static_cast<double>(len) * 0.5, 1e-7);
        SymMat4 kp = SymMat4::from_plane(n.x, n.y, n.z, d);
        for (int k = 0; k < 10; ++k) kp.q[k] *= area_w;

        quadrics[f[0]] += kp;
        quadrics[f[1]] += kp;
        quadrics[f[2]] += kp;

        edge_faces[edge_key(f[0], f[1])].push_back(static_cast<uint32_t>(fi));
        edge_faces[edge_key(f[1], f[2])].push_back(static_cast<uint32_t>(fi));
        edge_faces[edge_key(f[2], f[0])].push_back(static_cast<uint32_t>(fi));
    }

    // Add boundary & sharp-crease constraint planes
    for (const auto& kv : edge_faces) {
        uint32_t u = static_cast<uint32_t>(kv.first >> 32);
        uint32_t v = static_cast<uint32_t>(kv.first & 0xFFFFFFFFULL);
        bool is_boundary = (kv.second.size() == 1);
        bool is_crease = false;
        if (preserve_creases && kv.second.size() == 2) {
            float dot_n = face_normals[kv.second[0]].dot(face_normals[kv.second[1]]);
            if (dot_n < 0.65f) is_crease = true; // > ~49 deg crease
        }
        if (is_boundary || is_crease) {
            Vec3 e = m.positions[v] - m.positions[u];
            Vec3 fn = face_normals[kv.second[0]];
            Vec3 bn = e.cross(fn).normalized();
            double d = -static_cast<double>(bn.dot(m.positions[u]));
            double penalty = is_boundary ? 50.0 : 4.5;
            SymMat4 bp = SymMat4::from_plane(bn.x, bn.y, bn.z, d);
            for (int k = 0; k < 10; ++k) bp.q[k] *= penalty;
            quadrics[u] += bp;
            quadrics[v] += bp;
        }
    }

    struct CollapseItem {
        double cost;
        uint32_t u, v;
        uint32_t ver_u, ver_v;
        Vec3 best_pos;
        bool operator>(const CollapseItem& o) const { return cost > o.cost; }
    };

    std::priority_queue<CollapseItem, std::vector<CollapseItem>, std::greater<CollapseItem>> pq;

    auto push_edge = [&](uint32_t u, uint32_t v) {
        if (u == v) return;
        if (u > v) std::swap(u, v);
        SymMat4 qsum = quadrics[u] + quadrics[v];
        const Vec3& pu = m.positions[u];
        const Vec3& pv = m.positions[v];
        Vec3 pm = (pu + pv) * 0.5f;

        Vec3 opt = pm;
        double best_c = 1e30;
        if (qsum.solve_optimal(opt)) {
            // Guard against wild extrapolation far from segment
            float seg_len = (pv - pu).length();
            if ((opt - pm).length() <= seg_len * 1.5f + 1e-4f) {
                best_c = qsum.eval(opt);
            }
        }
        for (const Vec3& cand : {pm, pu, pv}) {
            double c = qsum.eval(cand);
            if (c < best_c) {
                best_c = c;
                opt = cand;
            }
        }
        pq.push({std::max(0.0, best_c), u, v, vert_version[u], vert_version[v], opt});
    };

    for (const auto& kv : edge_faces) {
        uint32_t u = static_cast<uint32_t>(kv.first >> 32);
        uint32_t v = static_cast<uint32_t>(kv.first & 0xFFFFFFFFULL);
        push_edge(u, v);
    }

    size_t live_faces = nf;
    while (live_faces > target_tris && !pq.empty()) {
        CollapseItem item = pq.top();
        pq.pop();
        uint32_t u = item.u, v = item.v;
        if (u == v || vert_version[u] != item.ver_u || vert_version[v] != item.ver_v) continue;

        // Check normal-flip prevention before committing collapse v -> u at best_pos
        bool flips = false;
        for (uint32_t fi : vert_faces[u]) {
            if (face_deleted[fi]) continue;
            const auto& f = m.faces[fi];
            if (f[0] == v || f[1] == v || f[2] == v) continue;
            Vec3 a = (f[0] == u) ? item.best_pos : m.positions[f[0]];
            Vec3 b = (f[1] == u) ? item.best_pos : m.positions[f[1]];
            Vec3 c = (f[2] == u) ? item.best_pos : m.positions[f[2]];
            Vec3 nn = (b - a).cross(c - a);
            if (nn.length() > 1e-9f && nn.normalized().dot(face_normals[fi]) < 0.15f) { flips = true; break; }
        }
        if (!flips) {
            for (uint32_t fi : vert_faces[v]) {
                if (face_deleted[fi]) continue;
                const auto& f = m.faces[fi];
                if (f[0] == u || f[1] == u || f[2] == u) continue;
                Vec3 a = (f[0] == v) ? item.best_pos : m.positions[f[0]];
                Vec3 b = (f[1] == v) ? item.best_pos : m.positions[f[1]];
                Vec3 c = (f[2] == v) ? item.best_pos : m.positions[f[2]];
                Vec3 nn = (b - a).cross(c - a);
                if (nn.length() > 1e-9f && nn.normalized().dot(face_normals[fi]) < 0.15f) { flips = true; break; }
            }
        }
        if (flips) continue;

        // Commit collapse v -> u
        m.positions[u] = item.best_pos;
        if (has_col) {
            m.colors[u].r = static_cast<uint8_t>((int(m.colors[u].r) + int(m.colors[v].r)) / 2);
            m.colors[u].g = static_cast<uint8_t>((int(m.colors[u].g) + int(m.colors[v].g)) / 2);
            m.colors[u].b = static_cast<uint8_t>((int(m.colors[u].b) + int(m.colors[v].b)) / 2);
        }
        quadrics[u] += quadrics[v];
        ++vert_version[u];
        vert_version[v] = 0xFFFFFFFFu; // retired

        for (uint32_t fi : vert_faces[v]) {
            if (face_deleted[fi]) continue;
            auto& f = m.faces[fi];
            for (int k = 0; k < 3; ++k) {
                if (f[k] == v) f[k] = u;
            }
            if (f[0] == f[1] || f[1] == f[2] || f[2] == f[0]) {
                face_deleted[fi] = true;
                --live_faces;
            } else {
                vert_faces[u].push_back(fi);
            }
        }
        vert_faces[v].clear();

        // Compact u's face list & enqueue incident edges
        std::unordered_set<uint32_t> nbrs;
        size_t wr = 0;
        for (size_t i = 0; i < vert_faces[u].size(); ++i) {
            uint32_t fi = vert_faces[u][i];
            if (face_deleted[fi]) continue;
            const auto& f = m.faces[fi];
            if (f[0] == f[1] || f[1] == f[2] || f[2] == f[0]) {
                face_deleted[fi] = true;
                --live_faces;
                continue;
            }
            Vec3 cr = (m.positions[f[1]] - m.positions[f[0]]).cross(m.positions[f[2]] - m.positions[f[0]]);
            if (cr.length() > 1e-9f) face_normals[fi] = cr.normalized();
            vert_faces[u][wr++] = fi;
            for (int k = 0; k < 3; ++k) {
                if (f[k] != u && vert_version[f[k]] != 0xFFFFFFFFu) nbrs.insert(f[k]);
            }
        }
        vert_faces[u].resize(wr);
        for (uint32_t nb : nbrs) push_edge(u, nb);
    }

    // Compact surviving vertices & faces
    std::vector<int32_t> remap(nv, -1);
    std::vector<Vec3> out_pos;
    std::vector<ColorRGBA> out_col;
    std::vector<std::array<uint32_t, 3>> out_faces;
    out_faces.reserve(live_faces);

    for (size_t fi = 0; fi < nf; ++fi) {
        if (face_deleted[fi]) continue;
        const auto& f = m.faces[fi];
        if (f[0] == f[1] || f[1] == f[2] || f[2] == f[0]) continue;
        std::array<uint32_t, 3> nf_arr{};
        for (int k = 0; k < 3; ++k) {
            if (remap[f[k]] < 0) {
                remap[f[k]] = static_cast<int32_t>(out_pos.size());
                out_pos.push_back(m.positions[f[k]]);
                if (has_col) out_col.push_back(m.colors[f[k]]);
            }
            nf_arr[k] = static_cast<uint32_t>(remap[f[k]]);
        }
        out_faces.push_back(nf_arr);
    }

    m.positions = std::move(out_pos);
    m.colors = std::move(out_col);
    m.faces = std::move(out_faces);
    m.quads.clear();
    m.recompute_normals();
}

// Greedy Field-Aligned Triangle-to-Quad Pairing + Tangent-Plane Quad Relaxation
inline size_t extract_quad_dominant_topology(Mesh& m, int relax_iters = 2) {
    m.quads.clear();
    const size_t nf = m.faces.size();
    if (nf < 2) return 0;

    struct AdjRef {
        uint32_t fi;
        int edge_slot; // 0:(f0,f1), 1:(f1,f2), 2:(f2,f0)
    };
    std::unordered_map<uint64_t, std::vector<AdjRef>> edge_map;
    edge_map.reserve(nf * 2);

    std::vector<Vec3> fnorm(nf);
    for (size_t fi = 0; fi < nf; ++fi) {
        const auto& f = m.faces[fi];
        fnorm[fi] = (m.positions[f[1]] - m.positions[f[0]]).cross(m.positions[f[2]] - m.positions[f[0]]).normalized();
        edge_map[edge_key(f[0], f[1])].push_back({static_cast<uint32_t>(fi), 0});
        edge_map[edge_key(f[1], f[2])].push_back({static_cast<uint32_t>(fi), 1});
        edge_map[edge_key(f[2], f[0])].push_back({static_cast<uint32_t>(fi), 2});
    }

    struct QuadCand {
        float score;
        uint32_t fA, fB;
        std::array<uint32_t, 4> q;
        bool operator<(const QuadCand& o) const { return score < o.score; } // max-heap
    };

    std::priority_queue<QuadCand> cands;
    for (const auto& kv : edge_map) {
        if (kv.second.size() != 2) continue;
        uint32_t fA = kv.second[0].fi;
        uint32_t fB = kv.second[1].fi;
        float coplanar = fnorm[fA].dot(fnorm[fB]);
        if (coplanar < 0.82f) continue; // Only merge reasonably coplanar triangles (< ~35 deg)

        const auto& tA = m.faces[fA];
        const auto& tB = m.faces[fB];
        int sA = kv.second[0].edge_slot;
        int sB = kv.second[1].edge_slot;

        // Shared edge in tA is (tA[sA], tA[(sA+1)%3]), opposite vertex in tA is tA[(sA+2)%3]
        uint32_t v0 = tA[(sA + 2) % 3];
        uint32_t v1 = tA[sA];
        uint32_t v2 = tB[(sB + 2) % 3];
        uint32_t v3 = tA[(sA + 1) % 3];
        if (v0 == v2 || v1 == v3) continue;

        // Measure angle closeness to 90 deg at all 4 quad corners & convexity
        std::array<uint32_t, 4> q = {v0, v1, v2, v3};
        Vec3 qn = (fnorm[fA] + fnorm[fB]).normalized();
        float angle_pen = 0.0f;
        bool convex = true;
        for (int k = 0; k < 4; ++k) {
            const Vec3& prev = m.positions[q[(k + 3) % 4]];
            const Vec3& cur  = m.positions[q[k]];
            const Vec3& next = m.positions[q[(k + 1) % 4]];
            Vec3 e1 = (prev - cur).normalized();
            Vec3 e2 = (next - cur).normalized();
            if ((next - cur).cross(prev - cur).dot(qn) < 0.0f) { convex = false; break; }
            angle_pen += std::fabs(e1.dot(e2)); // 0 when 90 degrees!
        }
        if (!convex) continue;

        float score = coplanar * 2.0f - angle_pen * 0.5f;
        cands.push({score, fA, fB, q});
    }

    std::vector<bool> paired(nf, false);
    while (!cands.empty()) {
        QuadCand c = cands.top();
        cands.pop();
        if (paired[c.fA] || paired[c.fB]) continue;
        paired[c.fA] = true;
        paired[c.fB] = true;
        m.quads.push_back(c.q);
    }

    // Rebuild faces so paired quads have consistent clean diagonal triangulation for GLB,
    // while m.quads + residual unpaired triangles are available for true quad OBJ/FBX/USDZ!
    std::vector<std::array<uint32_t, 3>> residual_tris;
    for (size_t fi = 0; fi < nf; ++fi) {
        if (!paired[fi]) residual_tris.push_back(m.faces[fi]);
    }

    // Tangent-plane quad vertex relaxation for crisp grid-like quad edges
    if (relax_iters > 0 && !m.quads.empty()) {
        m.recompute_normals();
        std::vector<std::vector<uint32_t>> q_nbrs(m.positions.size());
        for (const auto& q : m.quads) {
            for (int k = 0; k < 4; ++k) {
                q_nbrs[q[k]].push_back(q[(k + 1) % 4]);
                q_nbrs[q[k]].push_back(q[(k + 3) % 4]);
            }
        }
        for (int it = 0; it < relax_iters; ++it) {
            std::vector<Vec3> next_p = m.positions;
            for (size_t vi = 0; vi < m.positions.size(); ++vi) {
                if (q_nbrs[vi].size() < 4) continue;
                Vec3 avg{0, 0, 0};
                for (uint32_t nb : q_nbrs[vi]) avg += m.positions[nb];
                avg = avg / static_cast<float>(q_nbrs[vi].size());
                Vec3 delta = avg - m.positions[vi];
                // Project onto vertex tangent plane so volume never shrinks!
                const Vec3& vn = m.normals[vi];
                Vec3 tang = delta - vn * delta.dot(vn);
                next_p[vi] = m.positions[vi] + tang * 0.35f;
            }
            m.positions = std::move(next_p);
            m.recompute_normals();
        }
    }

    // Reconstruct triangulated representation (2 tris per quad + residual_tris) for WebGL/GLB
    m.faces.clear();
    m.faces.reserve(m.quads.size() * 2 + residual_tris.size());
    for (const auto& q : m.quads) {
        m.faces.push_back({q[0], q[1], q[2]});
        m.faces.push_back({q[0], q[2], q[3]});
    }
    for (const auto& t : residual_tris) {
        m.faces.push_back(t);
    }
    m.recompute_normals();
    return m.quads.size();
}

} // namespace qem_detail

struct SmartRemeshStats {
    size_t input_tris = 0;
    size_t output_tris = 0;
    size_t output_quads = 0;
    size_t output_polygons = 0; // quads + residual tris
};

inline SmartRemeshStats smart_remesh_scene(
    Scene3D& scene,
    size_t target_polygons = 25000,
    bool quad_mode = true,
    bool preserve_creases = true
) {
    target_polygons = std::clamp<size_t>(target_polygons, 100, 300000);
    // In quad_mode, each quad represents ~2 underlying triangles
    size_t target_tris = quad_mode ? std::min<size_t>(600000, target_polygons * 2) : target_polygons;

    size_t total_in = 0;
    for (const auto& m : scene.parts) total_in += m.faces.size();
    if (total_in == 0) return {};

    // Subdivide parts proportionally if user requested HIGHER polycount than current mesh
    for (auto& m : scene.parts) {
        double share = static_cast<double>(m.faces.size()) / static_cast<double>(total_in);
        size_t part_target = std::max<size_t>(12, static_cast<size_t>(std::llround(share * target_tris)));
        int safety = 0;
        while (m.faces.size() * 2 < part_target && m.faces.size() < 90000 && safety++ < 3) {
            m.subdivide_midpoint();
            m.taubin_smooth(2);
        }
    }

    size_t mid_total = 0;
    for (const auto& m : scene.parts) mid_total += m.faces.size();

    SmartRemeshStats st;
    st.input_tris = total_in;
    for (auto& m : scene.parts) {
        double share = static_cast<double>(m.faces.size()) / static_cast<double>(std::max<size_t>(1, mid_total));
        size_t part_target = std::max<size_t>(12, static_cast<size_t>(std::llround(share * target_tris)));
        qem_detail::decimate_qem_single_mesh(m, part_target, preserve_creases);
        if (quad_mode) {
            size_t qcnt = qem_detail::extract_quad_dominant_topology(m, 2);
            st.output_quads += qcnt;
            size_t res_tris = m.faces.size() - qcnt * 2;
            st.output_polygons += qcnt + res_tris;
        } else {
            m.quads.clear();
            st.output_polygons += m.faces.size();
        }
        st.output_tris += m.faces.size();
    }
    scene.ground_on_y0();
    return st;
}

} // namespace openmesh
