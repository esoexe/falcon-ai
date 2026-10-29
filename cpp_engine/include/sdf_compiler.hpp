#pragma once
/**
 * OpenMesh Studio — C++20 Multi-Part SDF + Parametric CSG Compiler & Unity C# Exporter.
 * Uses OpenMP parallel voxel evaluation + Surface Nets + analytical hard-surface primitives.
 */

#include "marching_cubes.hpp"
#include "mesh.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace openmesh {

inline float sdf_smooth_min(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (b - a) / std::max(k, 1e-4f), 0.0f, 1.0f);
    return b * (1.0f - h) + a * h - k * h * (1.0f - h);
}

inline float sdf_smooth_sub(float d1, float d2, float k) {
    float h = std::clamp(0.5f - 0.5f * (d2 + d1) / std::max(k, 1e-4f), 0.0f, 1.0f);
    return d1 + ( -d2 - d1 ) * h + k * h * (1.0f - h);
}

// Exact SDF to a tapered 3D capsule segment (p0 -> p1 with radii r0 -> r1)
inline float sdf_tapered_capsule(Vec3 p, Vec3 a, Vec3 b, float r0, float r1) {
    Vec3 ba = b - a;
    float l2 = ba.length_sq();
    if (l2 < 1e-6f) return (p - a).length() - r0;
    float t = std::clamp((p - a).dot(ba) / l2, 0.0f, 1.0f);
    float r = r0 + (r1 - r0) * t;
    return (p - (a + ba * t)).length() - r;
}

inline Mesh make_box_mesh(float ex, float ey, float ez) {
    Mesh m;
    float hx = ex * 0.5f, hy = ey * 0.5f, hz = ez * 0.5f;
    m.positions = {
        {-hx,-hy,-hz}, { hx,-hy,-hz}, { hx, hy,-hz}, {-hx, hy,-hz},
        {-hx,-hy, hz}, { hx,-hy, hz}, { hx, hy, hz}, {-hx, hy, hz}
    };
    m.faces = {
        {4,5,6},{4,6,7}, // +Z front
        {1,0,3},{1,3,2}, // -Z back
        {5,1,2},{5,2,6}, // +X right
        {0,4,7},{0,7,3}, // -X left
        {3,7,6},{3,6,2}, // +Y top
        {0,1,5},{0,5,4}  // -Y bottom
    };
    m.recompute_normals();
    return m;
}

inline Mesh make_cylinder_mesh(float radius, float height, int sections = 36) {
    Mesh m;
    sections = std::max(3, sections);
    float hy = height * 0.5f;
    for (int ring = 0; ring < 2; ++ring) {
        float y = (ring == 0) ? -hy : hy;
        for (int i = 0; i < sections; ++i) {
            float a = (2.0f * kPi * i) / sections;
            m.positions.push_back({radius * std::sin(a), y, radius * std::cos(a)});
        }
    }
    uint32_t bot_c = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, -hy, 0.0f});
    uint32_t top_c = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, hy, 0.0f});

    for (int i = 0; i < sections; ++i) {
        uint32_t in = (i + 1) % sections;
        uint32_t b0 = i, b1 = in;
        uint32_t t0 = sections + i, t1 = sections + in;
        m.faces.push_back({b0, b1, t1});
        m.faces.push_back({b0, t1, t0});
        m.faces.push_back({bot_c, b1, b0});
        m.faces.push_back({top_c, t0, t1});
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_cone_mesh(float radius, float height, int sections = 36) {
    Mesh m;
    float hy = height * 0.5f;
    for (int i = 0; i < sections; ++i) {
        float a = (2.0f * kPi * i) / sections;
        m.positions.push_back({radius * std::sin(a), -hy, radius * std::cos(a)});
    }
    uint32_t bot_c = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, -hy, 0.0f});
    uint32_t apex = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, hy, 0.0f});

    for (int i = 0; i < sections; ++i) {
        uint32_t in = (i + 1) % sections;
        m.faces.push_back({static_cast<uint32_t>(i), in, apex});
        m.faces.push_back({bot_c, in, static_cast<uint32_t>(i)});
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_sphere_or_capsule(float radius, float cyl_height = 0.0f, int rings = 22, int segs = 32) {
    Mesh m;
    float half_h = cyl_height * 0.5f;
    for (int r = 0; r <= rings; ++r) {
        float v = static_cast<float>(r) / rings;
        float phi = (v - 0.5f) * kPi;
        float y = radius * std::sin(phi) + ((phi >= 0.0f) ? half_h : -half_h);
        float rr = radius * std::cos(phi);
        for (int s = 0; s < segs; ++s) {
            float theta = (2.0f * kPi * s) / segs;
            m.positions.push_back({rr * std::sin(theta), y, rr * std::cos(theta)});
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segs; ++s) {
            uint32_t sn = (s + 1) % segs;
            uint32_t i0 = r * segs + s;
            uint32_t i1 = r * segs + sn;
            uint32_t i2 = (r + 1) * segs + sn;
            uint32_t i3 = (r + 1) * segs + s;
            m.faces.push_back({i0, i1, i2});
            m.faces.push_back({i0, i2, i3});
        }
    }
    m.weld_vertices(1e-5f);
    return m;
}

inline Mesh make_torus_mesh(float major_r, float minor_r, int major_segs = 42, int minor_segs = 20) {
    Mesh m;
    for (int i = 0; i < major_segs; ++i) {
        float u = (2.0f * kPi * i) / major_segs;
        float cu = std::cos(u), su = std::sin(u);
        for (int j = 0; j < minor_segs; ++j) {
            float v = (2.0f * kPi * j) / minor_segs;
            float r = major_r + minor_r * std::cos(v);
            float y = minor_r * std::sin(v);
            m.positions.push_back({r * su, y, r * cu});
        }
    }
    for (int i = 0; i < major_segs; ++i) {
        int in = (i + 1) % major_segs;
        for (int j = 0; j < minor_segs; ++j) {
            int jn = (j + 1) % minor_segs;
            uint32_t a = i * minor_segs + j;
            uint32_t b = in * minor_segs + j;
            uint32_t c = in * minor_segs + jn;
            uint32_t d = i * minor_segs + jn;
            m.faces.push_back({a, b, c});
            m.faces.push_back({a, c, d});
        }
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_blade_mesh(float length = 2.25f, float width = 0.28f, float thickness = 0.085f, float curve = 0.0f, int sections = 40) {
    Mesh m;
    constexpr int N = 12;
    for (int i = 0; i < sections; ++i) {
        float t = static_cast<float>(i) / (sections - 1);
        float y = t * length;
        float x_curve = curve * t * t;
        float w, th;
        if (t < 0.74f) {
            w = width * (1.0f - 0.14f * t);
            th = thickness * (1.0f - 0.16f * t);
        } else {
            float u = (t - 0.74f) / 0.26f;
            w = width * 0.896f * std::cos(u * kPi * 0.495f);
            th = thickness * 0.88f * (1.0f - 0.86f * u);
        }
        w = std::max(w, 0.0025f);
        th = std::max(th, 0.002f);

        bool in_fuller = (t > 0.04f && t < 0.68f);
        float groove_dip = in_fuller ? (0.86f - 0.56f * std::sin(((t - 0.04f) / 0.64f) * kPi)) : 0.88f;
        float z_groove = th * std::clamp(groove_dip, 0.28f, 0.90f);
        float z_ridge = th * 0.96f;
        float x_bevel = w * 0.52f;
        float x_groove = w * 0.20f;

        m.positions.push_back({x_curve - w,        y,  0.0f});
        m.positions.push_back({x_curve - x_bevel,  y,  z_ridge});
        m.positions.push_back({x_curve - x_groove, y,  z_groove});
        m.positions.push_back({x_curve,            y,  z_groove * 0.82f});
        m.positions.push_back({x_curve + x_groove, y,  z_groove});
        m.positions.push_back({x_curve + x_bevel,  y,  z_ridge});
        m.positions.push_back({x_curve + w,        y,  0.0f});
        m.positions.push_back({x_curve + x_bevel,  y, -z_ridge});
        m.positions.push_back({x_curve + x_groove, y, -z_groove});
        m.positions.push_back({x_curve,            y, -z_groove * 0.82f});
        m.positions.push_back({x_curve - x_groove, y, -z_groove});
        m.positions.push_back({x_curve - x_bevel,  y, -z_ridge});
    }
    uint32_t tip_idx = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({curve, length + width * 0.22f, 0.0f});
    uint32_t base_idx = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, 0.0f, 0.0f});

    for (int i = 0; i < sections - 1; ++i) {
        uint32_t b0 = i * N, b1 = (i + 1) * N;
        for (uint32_t j = 0; j < N; ++j) {
            uint32_t jn = (j + 1) % N;
            m.faces.push_back({b0 + j, b0 + jn, b1 + jn});
            m.faces.push_back({b0 + j, b1 + jn, b1 + j});
        }
    }
    uint32_t last = (sections - 1) * N;
    for (uint32_t j = 0; j < N; ++j) {
        uint32_t jn = (j + 1) % N;
        m.faces.push_back({last + j, last + jn, tip_idx});
        m.faces.push_back({base_idx, jn, j});
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_wheel_assembly_mesh(float radius = 0.30f, float width = 0.24f, int spokes = 10) {
    Mesh m;
    int maj = 44, min_s = 20;
    float r_mid = radius * 0.76f;
    float r_tube = radius * 0.24f;
    for (int i = 0; i < maj; ++i) {
        float u = (2.0f * kPi * i) / maj;
        float cu = std::cos(u), su = std::sin(u);
        for (int j = 0; j < min_s; ++j) {
            float v = (2.0f * kPi * j) / min_s;
            float radial = r_mid + r_tube * std::cos(v);
            float axial = (width * 0.5f) * std::tanh(1.6f * std::sin(v));
            m.positions.push_back({axial, radial * su, radial * cu});
        }
    }
    for (int i = 0; i < maj; ++i) {
        int in = (i + 1) % maj;
        for (int j = 0; j < min_s; ++j) {
            int jn = (j + 1) % min_s;
            uint32_t a = i * min_s + j, b = in * min_s + j, c = in * min_s + jn, d = i * min_s + jn;
            m.faces.push_back({a, b, c});
            m.faces.push_back({a, c, d});
        }
    }
    m.recompute_normals();

    Mesh hub = make_cylinder_mesh(radius * 0.22f, width * 0.72f, 24);
    hub.apply_transform(Mat4::rot_z(kPi * 0.5f));
    m.append(hub);

    Mesh rotor = make_cylinder_mesh(radius * 0.54f, width * 0.16f, 32);
    rotor.apply_transform(Mat4::trs({-width * 0.08f, 0, 0}, {0, 0, 90}, {1, 1, 1}));
    m.append(rotor);

    for (int s = 0; s < spokes; ++s) {
        float ang = (2.0f * kPi * s) / spokes;
        Mesh spk = make_box_mesh(width * 0.14f, radius * 0.56f, radius * 0.065f);
        float rm = radius * 0.38f;
        spk.apply_transform(Mat4::trs({width * 0.26f, rm * std::sin(ang), rm * std::cos(ang)}, {ang * -180.0f / kPi, 0, 8}, {1, 1, 1}));
        m.append(spk);
    }
    return m;
}

inline Mesh make_lathe_vessel(float height, const std::string& mode, int flutes, float twist);

inline Mesh make_serrated_nozzle_mesh(float radius = 0.26f, float length = 0.52f, int petals = 12) {
    Mesh m = make_lathe_vessel(length, "thruster", petals, 0.2f);
    m.apply_transform(Mat4::scale(radius / 0.45f, 1.0f, radius / 0.45f));
    for (int i = 0; i < petals; ++i) {
        float ang = (2.0f * kPi * i) / petals;
        Mesh pet = make_box_mesh(radius * 0.30f, length * 0.55f, radius * 0.09f);
        float r_pos = radius * 0.86f;
        pet.apply_transform(Mat4::trs({r_pos * std::sin(ang), -length * 0.18f, r_pos * std::cos(ang)}, {-14.0f, ang * 180.0f / kPi, 0.0f}, {1, 1, 1}));
        m.append(pet);
    }
    return m;
}

inline Mesh make_bat_wing_mesh(float span = 1.85f, float chord = 1.05f, int grid_res = 88) {
    int nx = std::clamp(grid_res, 64, 108);
    int ny = std::max(44, nx / 2);
    int nz = std::max(64, (nx * 3) / 4);

    Vec3 bmin{-0.18f, -0.35f, -chord * 1.18f};
    Vec3 bmax{span * 1.10f, 0.52f, 0.42f};
    Vec3 sp{(bmax.x - bmin.x) / (nx - 1), (bmax.y - bmin.y) / (ny - 1), (bmax.z - bmin.z) / (nz - 1)};
    Grid3D g(nx, ny, nz, bmin, sp);

    Vec3 shoulder{0.0f, 0.0f, 0.05f};
    Vec3 elbow{span * 0.28f, 0.16f, 0.18f};
    Vec3 wrist{span * 0.52f, 0.28f, 0.24f};
    Vec3 thumb_tip{span * 0.58f, 0.38f, 0.36f};
    Vec3 f_tips[4] = {
        {span * 1.00f, 0.20f, -chord * 0.18f},
        {span * 0.86f, 0.08f, -chord * 0.62f},
        {span * 0.58f, -0.04f, -chord * 0.92f},
        {span * 0.26f, -0.10f, -chord * 0.86f}
    };

    auto tri_membrane_sdf = [](Vec3 p, Vec3 a, Vec3 b, Vec3 c, float thick) -> float {
        Vec3 ba = b - a, pa = p - a;
        Vec3 cb = c - b, pb = p - b;
        Vec3 ac = a - c, pc = p - c;
        Vec3 nor = ba.cross(ac);
        float n2 = std::max(nor.length_sq(), 1e-6f);

        bool inside =
            (ba.cross(nor).dot(pa) >= 0.0f) &&
            (cb.cross(nor).dot(pb) >= 0.0f) &&
            (ac.cross(nor).dot(pc) >= 0.0f);

        float dist2;
        if (inside) {
            float dn = nor.dot(pa);
            dist2 = (dn * dn) / n2;
        } else {
            auto seg2 = [](Vec3 d, Vec3 q) {
                float t = std::clamp(q.dot(d) / std::max(d.length_sq(), 0.000001f), 0.0f, 1.0f);
                return (q - d * t).length_sq();
            };
            dist2 = std::min({seg2(ba, pa), seg2(cb, pb), seg2(ac, pc)});
        }
        return std::sqrt(dist2) - thick;
    };

    #pragma omp parallel for schedule(static)
    for (int ix = 0; ix < nx; ++ix) {
        bool bx = (ix == 0 || ix == nx - 1);
        float px = bmin.x + ix * sp.x;
        for (int iy = 0; iy < ny; ++iy) {
            bool by = (iy == 0 || iy == ny - 1);
            float py = bmin.y + iy * sp.y;
            for (int iz = 0; iz < nz; ++iz) {
                if (bx || by || iz == 0 || iz == nz - 1) {
                    g.at(ix, iy, iz) = 1.0f;
                    continue;
                }
                Vec3 p{px, py, bmin.z + iz * sp.z};
                float d = sdf_tapered_capsule(p, shoulder, elbow, 0.095f, 0.072f);
                d = sdf_smooth_min(d, sdf_tapered_capsule(p, elbow, wrist, 0.070f, 0.055f), 0.05f);
                d = sdf_smooth_min(d, sdf_tapered_capsule(p, wrist, thumb_tip, 0.042f, 0.012f), 0.03f);

                for (int f = 0; f < 4; ++f) {
                    d = sdf_smooth_min(d, sdf_tapered_capsule(p, wrist, f_tips[f], 0.044f, 0.014f), 0.04f);
                }

                Vec3 warped_p = p;
                warped_p.y -= 0.045f * std::sin((p.x / std::max(span, 0.2f)) * kPi);
                float mem_th = 0.024f;
                for (int f = 0; f < 3; ++f) {
                    float dm = tri_membrane_sdf(warped_p, wrist, f_tips[f], f_tips[f + 1], mem_th);
                    d = sdf_smooth_min(d, dm, 0.035f);
                }
                float dm_in = tri_membrane_sdf(warped_p, shoulder, wrist, f_tips[3], mem_th);
                d = sdf_smooth_min(d, dm_in, 0.04f);
                g.at(ix, iy, iz) = d;
            }
        }
    }
    return extract_isosurface_surface_nets(g);
}

inline Mesh make_gear_mesh(int teeth = 16, float outer_r = 0.85f, float inner_r = 0.25f, float height = 0.26f) {
    Mesh m;
    teeth = std::clamp(teeth, 6, 48);
    float root_r = outer_r * 0.80f;
    int n_pts = teeth * 4;
    for (int side = 0; side < 2; ++side) {
        float y = (side == 0) ? (-height * 0.5f) : (height * 0.5f);
        for (int i = 0; i < n_pts; ++i) {
            float ang = (2.0f * kPi * i) / n_pts;
            int ph = i % 4;
            float r = (ph == 1 || ph == 2) ? outer_r : root_r;
            if (ph == 1) ang += 0.12f * (2.0f * kPi / n_pts);
            if (ph == 2) ang -= 0.12f * (2.0f * kPi / n_pts);
            m.positions.push_back({r * std::sin(ang), y, r * std::cos(ang)});
        }
        for (int i = 0; i < n_pts; ++i) {
            float ang = (2.0f * kPi * i) / n_pts;
            m.positions.push_back({inner_r * std::sin(ang), y, inner_r * std::cos(ang)});
        }
    }
    uint32_t bo = 0, bi = n_pts, to = 2 * n_pts, ti = 3 * n_pts;
    for (int i = 0; i < n_pts; ++i) {
        uint32_t in = (i + 1) % n_pts;
        // Top cap
        m.faces.push_back({to + i, ti + i, ti + in});
        m.faces.push_back({to + i, ti + in, to + in});
        // Bottom cap
        m.faces.push_back({bo + i, bo + in, bi + in});
        m.faces.push_back({bo + i, bi + in, bi + i});
        // Outer teeth wall
        m.faces.push_back({bo + i, to + i, to + in});
        m.faces.push_back({bo + i, to + in, bo + in});
        // Inner bore wall
        m.faces.push_back({bi + i, bi + in, ti + in});
        m.faces.push_back({bi + i, ti + in, ti + i});
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_lathe_vessel(float height, const std::string& mode, int flutes = 12, float twist = 1.4f) {
    Mesh m;
    int rings = 34, segs = 44;
    for (int i = 0; i < rings; ++i) {
        float t = static_cast<float>(i) / (rings - 1);
        float y = (t - 0.5f) * height;
        float r = 0.4f;
        if (mode == "thruster") {
            r = 0.28f + 0.52f * std::pow(1.0f - t, 1.45f) + 0.05f * std::sin(t * kPi * 3.0f);
        } else if (mode == "chalice") {
            if (t < 0.14f) r = 0.52f - 2.2f * t;
            else if (t < 0.52f) r = 0.14f + 0.04f * std::sin((t - 0.14f) * 20.0f);
            else r = 0.16f + 0.48f * std::sin(((t - 0.52f) / 0.48f) * kPi * 0.65f);
        } else if (mode == "column") {
            r = (t < 0.1f || t > 0.9f) ? 0.56f : (0.42f - 0.05f * t);
        } else {
            r = 0.34f + 0.34f * std::sin(t * kPi * 1.15f) - 0.12f * std::sin(t * kPi * 2.4f);
            if (t > 0.85f) r += (t - 0.85f) * 0.7f;
        }
        r = std::max(0.08f, r);
        for (int j = 0; j < segs; ++j) {
            float ang = (2.0f * kPi * j) / segs;
            float rip = (flutes > 0) ? (1.0f + 0.065f * std::cos(flutes * ang + twist * t * kPi)) : 1.0f;
            float rr = r * rip;
            m.positions.push_back({rr * std::sin(ang), y, rr * std::cos(ang)});
        }
    }
    uint32_t bot_c = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, -0.5f * height, 0.0f});
    uint32_t top_c = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, 0.5f * height, 0.0f});

    for (int i = 0; i < rings - 1; ++i) {
        uint32_t r0 = i * segs, r1 = (i + 1) * segs;
        for (int j = 0; j < segs; ++j) {
            uint32_t jn = (j + 1) % segs;
            m.faces.push_back({r0 + j, r0 + jn, r1 + jn});
            m.faces.push_back({r0 + j, r1 + jn, r1 + j});
        }
    }
    uint32_t last = (rings - 1) * segs;
    for (int j = 0; j < segs; ++j) {
        uint32_t jn = (j + 1) % segs;
        m.faces.push_back({bot_c, jn, static_cast<uint32_t>(j)});
        m.faces.push_back({top_c, last + j, last + jn});
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_wing_mesh(float span = 1.5f, float chord = 0.65f, float thickness = 0.10f, float sweep = 0.5f) {
    Mesh m;
    int slices = 18, ring_pts = 14;
    for (int i = 0; i < slices; ++i) {
        float t = static_cast<float>(i) / (slices - 1);
        float x = t * span;
        float c = chord * (1.0f - 0.68f * std::pow(t, 1.25f));
        float th = thickness * (1.0f - 0.72f * t);
        float z_off = -t * sweep;
        float y_arch = 0.22f * std::sin(t * kPi * 0.85f);
        for (int j = 0; j < ring_pts; ++j) {
            float a = (2.0f * kPi * j) / ring_pts;
            float cz = 0.5f * c * std::cos(a);
            float cy = 0.5f * th * std::sin(a) * (0.65f + 0.35f * std::cos(a * 0.5f));
            m.positions.push_back({x, y_arch + cy, z_off + cz});
        }
    }
    uint32_t root_i = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, 0.0f, 0.0f});
    uint32_t tip_i = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({span + 0.04f, 0.22f * std::sin(0.85f * kPi), -sweep});

    for (int i = 0; i < slices - 1; ++i) {
        uint32_t b0 = i * ring_pts, b1 = (i + 1) * ring_pts;
        for (int j = 0; j < ring_pts; ++j) {
            uint32_t jn = (j + 1) % ring_pts;
            m.faces.push_back({b0 + j, b1 + j, b1 + jn});
            m.faces.push_back({b0 + j, b1 + jn, b0 + jn});
        }
    }
    uint32_t last = (slices - 1) * ring_pts;
    for (int j = 0; j < ring_pts; ++j) {
        uint32_t jn = (j + 1) % ring_pts;
        m.faces.push_back({root_i, static_cast<uint32_t>(j), jn});
        m.faces.push_back({tip_i, last + jn, last + static_cast<uint32_t>(j)});
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_battlement_ring(float radius = 0.84f, float height = 0.34f, int merlons = 8) {
    Mesh m = make_cylinder_mesh(radius, height * 0.45f, 36);
    for (int i = 0; i < merlons; ++i) {
        float ang = (2.0f * kPi * i) / merlons;
        Mesh blk = make_box_mesh(radius * 0.34f, height * 0.65f, radius * 0.24f);
        float r_mid = radius * 0.86f;
        blk.apply_transform(Mat4::trs({r_mid * std::sin(ang), height * 0.48f, r_mid * std::cos(ang)}, {0.0f, ang * 180.0f / kPi, 0.0f}, {1,1,1}));
        m.append(blk);
    }
    return m;
}

inline Mesh make_octa_crystal(float height = 1.3f, float radius = 0.36f, int sides = 6) {
    Mesh m;
    sides = std::max(4, sides);
    float ys[2] = {-0.26f * height, 0.24f * height};
    float rs[2] = {0.85f, 1.0f};
    for (int r = 0; r < 2; ++r) {
        for (int j = 0; j < sides; ++j) {
            float a = (2.0f * kPi * j) / sides;
            m.positions.push_back({radius * rs[r] * std::sin(a), ys[r], radius * rs[r] * std::cos(a)});
        }
    }
    uint32_t bot = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, -0.5f * height, 0.0f});
    uint32_t top = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back({0.0f, 0.5f * height, 0.0f});

    for (int j = 0; j < sides; ++j) {
        uint32_t jn = (j + 1) % sides;
        m.faces.push_back({static_cast<uint32_t>(j), jn, static_cast<uint32_t>(sides) + jn});
        m.faces.push_back({static_cast<uint32_t>(j), static_cast<uint32_t>(sides) + jn, static_cast<uint32_t>(sides) + j});
        m.faces.push_back({bot, jn, static_cast<uint32_t>(j)});
        m.faces.push_back({static_cast<uint32_t>(sides) + j, static_cast<uint32_t>(sides) + jn, top});
    }
    m.recompute_normals();
    return m;
}

inline Mesh make_picatinny_rail_mesh(float length = 0.92f, int slots = 12, float width = 0.085f) {
    Mesh m = make_box_mesh(width, 0.032f, length);
    slots = std::clamp(slots, 4, 28);
    float step = length / static_cast<float>(slots);
    for (int i = 0; i < slots; ++i) {
        float z = -length * 0.5f + (i + 0.5f) * step;
        Mesh tooth = make_box_mesh(width * 1.18f, 0.026f, step * 0.54f);
        tooth.apply_transform(Mat4::trs({0.0f, 0.026f, z}, {0,0,0}, {1,1,1}));
        m.append(tooth);
    }
    return m;
}

inline Mesh make_crystal_cluster_mesh(float scale_h = 0.75f, int count = 7) {
    Mesh m = make_octa_crystal(scale_h, scale_h * 0.24f, 6);
    count = std::clamp(count, 3, 12);
    for (int i = 1; i < count; ++i) {
        float ang = (2.0f * kPi * i) / (count - 1);
        float tilt = 22.0f + 16.0f * ((i % 2 == 0) ? 1.0f : 0.55f);
        float h = scale_h * (0.52f + 0.28f * ((i * 37) % 5) / 4.0f);
        Mesh sub = make_octa_crystal(h, h * 0.25f, 6);
        sub.apply_transform(Mat4::trs(
            {std::sin(ang) * scale_h * 0.18f, -scale_h * 0.12f, std::cos(ang) * scale_h * 0.18f},
            {std::cos(ang) * tilt, ang * 57.2958f, -std::sin(ang) * tilt},
            {1.0f, 1.0f, 1.0f}
        ));
        m.append(sub);
    }
    return m;
}

inline Mesh make_hydraulic_piston_mesh(float length = 0.55f, float radius = 0.055f) {
    Mesh m = make_cylinder_mesh(radius, length * 0.58f, 20);
    m.apply_transform(Mat4::trs({0.0f, -length * 0.18f, 0.0f}, {0,0,0}, {1,1,1}));
    Mesh rod = make_cylinder_mesh(radius * 0.56f, length * 0.62f, 18);
    rod.apply_transform(Mat4::trs({0.0f, length * 0.20f, 0.0f}, {0,0,0}, {1,1,1}));
    m.append(rod);
    for (float y : {-length * 0.45f, 0.08f * length, length * 0.48f}) {
        Mesh col = make_cylinder_mesh(radius * 1.28f, length * 0.08f, 20);
        col.apply_transform(Mat4::trs({0.0f, y, 0.0f}, {0,0,0}, {1,1,1}));
        m.append(col);
    }
    return m;
}

inline Mesh make_ruined_arch_mesh(float width = 1.45f, float height = 1.85f) {
    Mesh m;
    for (float sx : {-1.0f, 1.0f}) {
        Mesh pil = make_lathe_vessel(height * 0.78f, "column", 10, 0.0f);
        pil.apply_transform(Mat4::trs({sx * width * 0.44f, height * 0.39f, 0.0f}, {0,0,0}, {0.38f, 1.0f, 0.38f}));
        m.append(pil);
        Mesh plinth = make_box_mesh(0.38f, 0.14f, 0.38f);
        plinth.apply_transform(Mat4::trs({sx * width * 0.44f, 0.07f, 0.0f}, {0,0,0}, {1,1,1}));
        m.append(plinth);
    }
    int segs = 11;
    for (int i = 0; i < segs; ++i) {
        float t = static_cast<float>(i) / (segs - 1);
        float a = kPi * t;
        float x = -std::cos(a) * width * 0.44f;
        float y = height * 0.76f + std::sin(a) * width * 0.42f;
        Mesh voussoir = make_box_mesh(0.20f, 0.24f, 0.30f);
        voussoir.apply_transform(Mat4::trs({x, y, 0.0f}, {0.0f, 0.0f, (t - 0.5f) * -155.0f}, {1,1,1}));
        m.append(voussoir);
    }
    return m;
}

inline Mesh make_sculpted_sdf_mesh(const JsonValue& params, const std::string& detail_style, int grid_res) {
    int res = std::clamp(grid_res, 56, 152);
    float bounds = static_cast<float>(params.get_num("bounds", 1.45));
    float sp = (2.0f * bounds) / static_cast<float>(res - 1);
    Grid3D grid(res, res, res, {-bounds, -bounds, -bounds}, {sp, sp, sp});

    struct Lobe {
        int shape_type; // 0 = ellipsoid, 1 = round_box, 2 = tapered_capsule
        Vec3 c;         // or p0 for capsule
        Vec3 r;         // or p1 for capsule
        float r0;
        float r1;
        float blend;
        bool is_subtract;
    };
    std::vector<Lobe> lobes;
    const auto& jlobes = params.at_or_null("lobes");
    if (jlobes.is_array() && jlobes.size() > 0) {
        for (size_t i = 0; i < jlobes.size(); ++i) {
            const auto& jl = jlobes[i];
            std::string sh = jl.get_str("shape", "ellipsoid");
            bool sub = jl.get_str("op", "smooth_union") == "subtract";
            float bl = static_cast<float>(jl.get_num("blend", 0.16));

            if (sh == "tapered_capsule") {
                const auto& jp0 = jl.at_or_null("p0");
                const auto& jp1 = jl.at_or_null("p1");
                Vec3 p0{0, -0.3f, 0}, p1{0, 0.3f, 0};
                if (jp0.is_array() && jp0.size() >= 3) p0 = {static_cast<float>(jp0[0].as_num()), static_cast<float>(jp0[1].as_num()), static_cast<float>(jp0[2].as_num())};
                if (jp1.is_array() && jp1.size() >= 3) p1 = {static_cast<float>(jp1[0].as_num()), static_cast<float>(jp1[1].as_num()), static_cast<float>(jp1[2].as_num())};
                lobes.push_back({
                    2, p0, p1,
                    static_cast<float>(jl.get_num("r0", 0.22)),
                    static_cast<float>(jl.get_num("r1", 0.12)),
                    bl, sub
                });
            } else {
                const auto& jc = jl.at_or_null("center");
                const auto& jr = jl.at_or_null("radii");
                Vec3 c{0, 0, 0}, r{0.5f, 0.5f, 0.5f};
                if (jc.is_array() && jc.size() >= 3) c = {static_cast<float>(jc[0].as_num()), static_cast<float>(jc[1].as_num()), static_cast<float>(jc[2].as_num())};
                if (jr.is_array() && jr.size() >= 3) r = {static_cast<float>(jr[0].as_num(0.45)), static_cast<float>(jr[1].as_num(0.45)), static_cast<float>(jr[2].as_num(0.45))};
                lobes.push_back({
                    (sh == "round_box") ? 1 : 0,
                    c,
                    {std::max(r.x, 0.03f), std::max(r.y, 0.03f), std::max(r.z, 0.03f)},
                    0.0f, 0.0f, bl, sub
                });
            }
        }
    } else {
        lobes.push_back({0, {0, 0, 0}, {0.55f, 0.65f, 0.50f}, 0.0f, 0.0f, 0.18f, false});
    }

    int style_code = 0;
    if (detail_style == "scales") style_code = 1;
    else if (detail_style == "stone") style_code = 2;
    else if (detail_style == "scifi_panels") style_code = 3;
    else if (detail_style == "gyroid") style_code = 4;
    else if (detail_style == "organic_muscle") style_code = 5;

    #pragma omp parallel for schedule(static)
    for (int ix = 0; ix < res; ++ix) {
        bool border_x = (ix == 0 || ix == res - 1);
        float px = -bounds + ix * sp;
        for (int iy = 0; iy < res; ++iy) {
            bool border_y = (iy == 0 || iy == res - 1);
            float py = -bounds + iy * sp;
            for (int iz = 0; iz < res; ++iz) {
                if (border_x || border_y || iz == 0 || iz == res - 1) {
                    grid.at(ix, iy, iz) = 1.0f;
                    continue;
                }
                float pz = -bounds + iz * sp;
                Vec3 pt{px, py, pz};
                float d = 10.0f;
                for (const auto& lb : lobes) {
                    float ld;
                    if (lb.shape_type == 2) {
                        ld = sdf_tapered_capsule(pt, lb.c, lb.r, lb.r0, lb.r1);
                    } else if (lb.shape_type == 1) {
                        float dx = (px - lb.c.x) / lb.r.x;
                        float dy = (py - lb.c.y) / lb.r.y;
                        float dz = (pz - lb.c.z) / lb.r.z;
                        float min_r = std::min({lb.r.x, lb.r.y, lb.r.z});
                        float qx = std::max(std::fabs(dx) - 0.80f, 0.0f);
                        float qy = std::max(std::fabs(dy) - 0.80f, 0.0f);
                        float qz = std::max(std::fabs(dz) - 0.80f, 0.0f);
                        float out_d = std::sqrt(qx * qx + qy * qy + qz * qz);
                        float in_d = std::min(std::max({std::fabs(dx) - 0.80f, std::fabs(dy) - 0.80f, std::fabs(dz) - 0.80f}), 0.0f);
                        ld = (out_d + in_d - 0.20f) * min_r;
                    } else {
                        float dx = (px - lb.c.x) / lb.r.x;
                        float dy = (py - lb.c.y) / lb.r.y;
                        float dz = (pz - lb.c.z) / lb.r.z;
                        float min_r = std::min({lb.r.x, lb.r.y, lb.r.z});
                        ld = (std::sqrt(dx * dx + dy * dy + dz * dz) - 1.0f) * min_r;
                    }
                    if (lb.is_subtract) d = sdf_smooth_sub(d, ld, lb.blend);
                    else d = sdf_smooth_min(d, ld, lb.blend);
                }

                if (std::fabs(d) < 0.24f && style_code > 0) {
                    if (style_code == 1) {
                        d -= 0.018f * std::sin(px * 26.0f) * std::sin(py * 26.0f + 1.5f * std::cos(pz * 22.0f));
                    } else if (style_code == 2) {
                        d -= 0.032f * std::sin(px * 8.5f + std::cos(pz * 7.5f)) * std::cos(py * 12.0f)
                           + 0.014f * std::sin(px * 19.0f + py * 17.0f);
                    } else if (style_code == 3) {
                        float gx = (std::fabs(std::sin(px * 10.5f)) > 0.955f) ? 0.018f : 0.0f;
                        float gy = (std::fabs(std::sin(py * 10.5f)) > 0.955f) ? 0.018f : 0.0f;
                        d += std::max(gx, gy);
                    } else if (style_code == 4) {
                        float gyr = std::sin(px * 9.5f) * std::cos(py * 9.5f) +
                                    std::sin(py * 9.5f) * std::cos(pz * 9.5f) +
                                    std::sin(pz * 9.5f) * std::cos(px * 9.5f);
                        d = std::max(d, std::fabs(gyr) * 0.085f - 0.042f);
                    } else if (style_code == 5) {
                        d -= 0.016f * std::cos(px * 11.0f) * std::sin(py * 9.0f) * std::cos(pz * 11.0f);
                    }
                }
                grid.at(ix, iy, iz) = d;
            }
        }
    }

    return extract_isosurface_surface_nets(grid);
}

inline Mesh build_part_primitive(const JsonValue& part, int grid_res) {
    std::string prim = part.get_str("primitive", "round_box");
    const auto& params = part.at_or_null("params");
    std::string detail = part.get_str("detail_style", "smooth");

    if (prim == "blade") {
        return make_blade_mesh(
            static_cast<float>(params.get_num("length", 2.25)),
            static_cast<float>(params.get_num("width", 0.28)),
            static_cast<float>(params.get_num("thickness", 0.085)),
            static_cast<float>(params.get_num("curve", 0.0))
        );
    }
    if (prim == "bat_wing") {
        return make_bat_wing_mesh(
            static_cast<float>(params.get_num("span", 1.85)),
            static_cast<float>(params.get_num("chord", 1.05)),
            grid_res
        );
    }
    if (prim == "wheel_assembly") {
        return make_wheel_assembly_mesh(
            static_cast<float>(params.get_num("radius", 0.30)),
            static_cast<float>(params.get_num("width", 0.24)),
            params.get_int("spokes", 10)
        );
    }
    if (prim == "serrated_nozzle") {
        return make_serrated_nozzle_mesh(
            static_cast<float>(params.get_num("radius", 0.26)),
            static_cast<float>(params.get_num("length", 0.52)),
            params.get_int("petals", 12)
        );
    }
    if (prim == "gear") {
        return make_gear_mesh(
            params.get_int("teeth", 16),
            static_cast<float>(params.get_num("outer_r", 0.85)),
            static_cast<float>(params.get_num("inner_r", 0.25)),
            static_cast<float>(params.get_num("height", 0.26))
        );
    }
    if (prim == "lathe_vase" || prim == "thruster" || prim == "chalice" || prim == "column") {
        std::string mode = params.get_str("mode", (prim == "lathe_vase") ? "vase" : prim);
        return make_lathe_vessel(
            static_cast<float>(params.get_num("height", 1.5)),
            mode,
            params.get_int("flutes", 12),
            static_cast<float>(params.get_num("twist", 1.4))
        );
    }
    if (prim == "wing") {
        return make_wing_mesh(
            static_cast<float>(params.get_num("span", 1.5)),
            static_cast<float>(params.get_num("chord", 0.65)),
            static_cast<float>(params.get_num("thickness", 0.10)),
            static_cast<float>(params.get_num("sweep", 0.5))
        );
    }
    if (prim == "battlement_ring") {
        return make_battlement_ring(
            static_cast<float>(params.get_num("radius", 0.84)),
            static_cast<float>(params.get_num("height", 0.34)),
            params.get_int("merlons", 8)
        );
    }
    if (prim == "octa_crystal") {
        return make_octa_crystal(
            static_cast<float>(params.get_num("height", 1.3)),
            static_cast<float>(params.get_num("radius", 0.36)),
            params.get_int("sides", 6)
        );
    }
    if (prim == "picatinny_rail") {
        return make_picatinny_rail_mesh(
            static_cast<float>(params.get_num("length", 0.92)),
            params.get_int("slots", 12),
            static_cast<float>(params.get_num("width", 0.085))
        );
    }
    if (prim == "crystal_cluster") {
        return make_crystal_cluster_mesh(
            static_cast<float>(params.get_num("height", 0.75)),
            params.get_int("count", 7)
        );
    }
    if (prim == "hydraulic_piston") {
        return make_hydraulic_piston_mesh(
            static_cast<float>(params.get_num("length", 0.55)),
            static_cast<float>(params.get_num("radius", 0.055))
        );
    }
    if (prim == "ruined_arch") {
        return make_ruined_arch_mesh(
            static_cast<float>(params.get_num("width", 1.45)),
            static_cast<float>(params.get_num("height", 1.85))
        );
    }
    if (prim == "sculpted_sdf") {
        return make_sculpted_sdf_mesh(params, detail, grid_res);
    }
    if (prim == "torus") {
        return make_torus_mesh(
            static_cast<float>(params.get_num("major_r", 0.55)),
            static_cast<float>(params.get_num("minor_r", 0.12))
        );
    }
    if (prim == "capsule") {
        return make_sphere_or_capsule(
            static_cast<float>(params.get_num("radius", 0.24)),
            static_cast<float>(params.get_num("height", 0.80))
        );
    }
    if (prim == "sphere") {
        return make_sphere_or_capsule(static_cast<float>(params.get_num("radius", 0.50)), 0.0f);
    }
    if (prim == "cylinder") {
        return make_cylinder_mesh(
            static_cast<float>(params.get_num("radius", 0.45)),
            static_cast<float>(params.get_num("height", 1.0)),
            params.get_int("sections", 36)
        );
    }
    if (prim == "hex_prism") {
        return make_cylinder_mesh(
            static_cast<float>(params.get_num("radius", 0.50)),
            static_cast<float>(params.get_num("height", 0.40)),
            6
        );
    }
    if (prim == "cone") {
        return make_cone_mesh(
            static_cast<float>(params.get_num("radius", 0.55)),
            static_cast<float>(params.get_num("height", 1.0)),
            36
        );
    }

    float ex = 0.8f, ey = 0.8f, ez = 0.8f;
    const auto& jext = params.at_or_null("extents");
    if (jext.is_array() && jext.size() >= 3) {
        ex = static_cast<float>(jext[0].as_num(0.8));
        ey = static_cast<float>(jext[1].as_num(0.8));
        ez = static_cast<float>(jext[2].as_num(0.8));
    }
    if (prim == "box") return make_box_mesh(ex, ey, ez);

    // Beveled round_box via fast SDF
    JsonValue rb = JsonValue::object();
    rb["bounds"] = std::max({ex, ey, ez}) * 0.72f + 0.22f;
    JsonValue l0 = JsonValue::object();
    l0["center"] = JsonValue::array({0.0, 0.0, 0.0});
    l0["radii"] = JsonValue::array({ex * 0.5, ey * 0.5, ez * 0.5});
    l0["shape"] = "round_box";
    l0["blend"] = 0.10;
    rb["lobes"] = JsonValue::array({l0});
    return make_sculpted_sdf_mesh(rb, detail, std::clamp(grid_res, 48, 72));
}

inline Vec3 json_vec3(const JsonValue& arr, Vec3 def) {
    if (arr.is_array() && arr.size() >= 3) {
        return {
            static_cast<float>(arr[0].as_num(def.x)),
            static_cast<float>(arr[1].as_num(def.y)),
            static_cast<float>(arr[2].as_num(def.z))
        };
    }
    return def;
}

inline Scene3D compile_blueprint(const JsonValue& blueprint, int grid_res = 88, const std::string& style = "realistic") {
    Scene3D scene;
    const auto& jparts = blueprint.at_or_null("parts");
    if (!jparts.is_array()) return scene;

    for (size_t i = 0; i < jparts.size(); ++i) {
        const auto& jp = jparts[i];
        if (jp.get_bool("hidden", false)) continue;

        Mesh base = build_part_primitive(jp, grid_res);
        if (style == "low_poly" && base.faces.size() > 600) {
            base.decimate_cluster(0.38f);
        }

        Vec3 pos = json_vec3(jp.at_or_null("position"), {0, 0, 0});
        Vec3 rot = json_vec3(jp.at_or_null("rotation"), {0, 0, 0});
        Vec3 scl = json_vec3(jp.at_or_null("scale"), {1, 1, 1});

        Mesh placed = base;
        placed.apply_transform(Mat4::trs(pos, rot, scl));

        int radial = jp.get_int("radial_count", 1);
        Mesh combined = placed;
        if (radial > 1) {
            for (int r = 1; r < radial; ++r) {
                Mesh inst = placed;
                float ang = (2.0f * kPi * r) / radial;
                inst.apply_transform(Mat4::rot_y(ang));
                combined.append(inst);
            }
        } else if (jp.get_bool("mirror_x", false) && std::fabs(pos.x) > 1e-4f) {
            Mesh mir = base;
            mir.apply_transform(Mat4::trs({-pos.x, pos.y, pos.z}, {rot.x, -rot.y, -rot.z}, {-scl.x, scl.y, scl.z}));
            combined.append(mir);
        }

        combined.id = jp.get_str("id", "part_" + std::to_string(i + 1));
        combined.name = jp.get_str("name", "Part " + std::to_string(i + 1));
        combined.primitive_type = jp.get_str("primitive", "round_box");
        combined.material.name = "Mat_" + combined.id;
        combined.material.color_hex = jp.get_str("color", "#94a3b8");
        combined.material.metallic = static_cast<float>(jp.get_num("metallic", 0.35));
        combined.material.roughness = static_cast<float>(jp.get_num("roughness", 0.42));
        combined.material.emissive_hex = jp.get_str("emissive", "");
        combined.material.emissive_strength = static_cast<float>(jp.get_num("emissive_strength", 0.0));

        // Infer or apply realistic PBR surface finish texture map set
        std::string fin = jp.get_str("finish", "");
        if (fin.empty()) {
            std::string det = jp.get_str("detail_style", "smooth");
            if (det == "scales" || combined.primitive_type == "bat_wing") fin = "dragon_scales";
            else if (det == "stone") fin = "weathered_stone";
            else if (det == "scifi_panels") fin = "painted_armor";
            else if (combined.primitive_type == "blade") fin = "damascus_steel";
            else if (combined.primitive_type == "wheel_assembly") fin = "rubber_tire";
            else if (combined.material.roughness > 0.68f && combined.material.metallic < 0.2f) fin = "wrapped_leather";
            else fin = "forged_metal";
        }
        combined.material.finish = fin;

        scene.parts.push_back(std::move(combined));
    }

    scene.ground_on_y0();
    return scene;
}

inline std::string generate_unity_csharp_script(const JsonValue& blueprint, const std::string& fallback_title = "OpenMeshAsset") {
    std::string title = blueprint.is_object() ? blueprint.get_str("title", fallback_title) : fallback_title;
    std::ostringstream cs;
    cs << "// Auto-generated by OpenMesh Studio 2.0 (C++20 Engine · MIT Open-Source)\n"
       << "// Drop into Assets/Scripts/ in Unity to procedurally spawn the multi-part PBR hierarchy.\n"
       << "using UnityEngine;\n\n"
       << "public class OpenMeshProceduralBuilder : MonoBehaviour\n{\n"
       << "    [ContextMenu(\"Build OpenMesh 3D Asset\")]\n"
       << "    void Start()\n    {\n"
       << "        GameObject root = new GameObject(" << JsonValue::escape_str(title) << ");\n"
       << "        root.transform.SetParent(this.transform, false);\n\n";

    const auto& parts = blueprint.at_or_null("parts");
    if (parts.is_array()) {
        for (size_t i = 0; i < parts.size(); ++i) {
            const auto& p = parts[i];
            std::string pname = p.get_str("name", "Part_" + std::to_string(i + 1));
            std::string prim = p.get_str("primitive", "box");
            Vec3 pos = json_vec3(p.at_or_null("position"), {0, 0, 0});
            Vec3 rot = json_vec3(p.at_or_null("rotation"), {0, 0, 0});
            Vec3 scl = json_vec3(p.at_or_null("scale"), {1, 1, 1});
            auto col = ColorRGBA::from_hex(p.get_str("color", "#94a3b8")).to_float4();
            float met = static_cast<float>(p.get_num("metallic", 0.35));
            float rgh = static_cast<float>(p.get_num("roughness", 0.42));

            std::string u_prim = "PrimitiveType.Cube";
            if (prim == "sphere" || prim == "sculpted_sdf") u_prim = "PrimitiveType.Sphere";
            else if (prim == "capsule") u_prim = "PrimitiveType.Capsule";
            else if (prim == "cylinder" || prim == "gear" || prim == "lathe_vase" || prim == "thruster") u_prim = "PrimitiveType.Cylinder";

            cs << "        {\n"
               << "            GameObject part = GameObject.CreatePrimitive(" << u_prim << ");\n"
               << "            part.name = " << JsonValue::escape_str(pname) << ";\n"
               << "            part.transform.SetParent(root.transform, false);\n"
               << std::fixed << std::setprecision(3)
               << "            part.transform.localPosition = new Vector3(" << pos.x << "f, " << pos.y << "f, " << pos.z << "f);\n"
               << "            part.transform.localEulerAngles = new Vector3(" << rot.x << "f, " << rot.y << "f, " << rot.z << "f);\n"
               << "            part.transform.localScale = new Vector3(" << scl.x << "f, " << scl.y << "f, " << scl.z << "f);\n"
               << "            var mat = new Material(Shader.Find(\"Standard\"));\n"
               << "            mat.color = new Color(" << col[0] << "f, " << col[1] << "f, " << col[2] << "f, 1f);\n"
               << "            mat.SetFloat(\"_Metallic\", " << met << "f);\n"
               << "            mat.SetFloat(\"_Glossiness\", " << (1.0f - rgh) << "f);\n"
               << "            part.GetComponent<Renderer>().sharedMaterial = mat;\n"
               << "        }\n";
        }
    }
    cs << "    }\n}\n";
    return cs.str();
}

} // namespace openmesh
