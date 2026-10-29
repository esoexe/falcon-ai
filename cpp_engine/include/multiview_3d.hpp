#pragma once
/**
 * Falcon AI v3.0 — Multi-View Photo-to-3D Space Carver, Monocular Shape-from-Shading
 * Depth Estimator, Photo RGB Surface Projection & 3D Region Texture Brush/Healer.
 *
 * Supports 1 to 4 reference views (Front, Right/Side, Back, Top) with automatic
 * background matting, 2D Euclidean Distance Fields, intersection visual hull carving,
 * and perspective-free multi-view RGB color blending weighted by surface normal!
 */

#include "image_to_3d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace openmesh {

struct PreparedView2D {
    bool valid = false;
    int N = 128;
    std::vector<float> sdf2d;   // Negative inside silhouette, positive outside (in [-1, 1] units)
    std::vector<float> depth2d; // Normalized [0, 1] monocular shape-from-shading + cushion depth
    std::vector<ColorRGBA> rgb;
};

namespace mv_detail {

inline PreparedView2D prepare_single_view(const std::vector<uint8_t>& img_bytes, int N = 128) {
    PreparedView2D pv;
    if (img_bytes.empty()) return pv;
    ImageRGBA raw = decode_image_bytes(img_bytes);
    if (raw.width < 4 || raw.height < 4) return pv;

    pv.valid = true;
    pv.N = N;
    pv.sdf2d.assign(static_cast<size_t>(N * N), 1.0f);
    pv.depth2d.assign(static_cast<size_t>(N * N), 0.0f);
    pv.rgb.assign(static_cast<size_t>(N * N), ColorRGBA{200, 200, 205, 255});

    // Estimate corner background color for automatic matting when alpha == 255
    auto px_at = [&](int x, int y) -> std::array<float, 4> {
        x = std::clamp(x, 0, raw.width - 1);
        y = std::clamp(y, 0, raw.height - 1);
        size_t i = static_cast<size_t>(y * raw.width + x) * 4;
        return {
            static_cast<float>(raw.pixels[i + 0]),
            static_cast<float>(raw.pixels[i + 1]),
            static_cast<float>(raw.pixels[i + 2]),
            static_cast<float>(raw.pixels[i + 3])
        };
    };

    auto c00 = px_at(0, 0), c10 = px_at(raw.width - 1, 0);
    auto c01 = px_at(0, raw.height - 1), c11 = px_at(raw.width - 1, raw.height - 1);
    float bg_r = 0.25f * (c00[0] + c10[0] + c01[0] + c11[0]);
    float bg_g = 0.25f * (c00[1] + c10[1] + c01[1] + c11[1]);
    float bg_b = 0.25f * (c00[2] + c10[2] + c01[2] + c11[2]);
    bool has_transparency = (c00[3] < 240.0f || c10[3] < 240.0f || c01[3] < 240.0f || c11[3] < 240.0f);

    std::vector<uint8_t> mask(static_cast<size_t>(N * N), 0);
    size_t fg_count = 0;

    for (int y = 0; y < N; ++y) {
        int sy = static_cast<int>((y + 0.5f) * raw.height / N);
        for (int x = 0; x < N; ++x) {
            int sx = static_cast<int>((x + 0.5f) * raw.width / N);
            auto p = px_at(sx, sy);
            size_t idx = static_cast<size_t>(y * N + x);
            pv.rgb[idx] = {
                static_cast<uint8_t>(p[0]),
                static_cast<uint8_t>(p[1]),
                static_cast<uint8_t>(p[2]),
                255
            };

            bool inside = false;
            if (has_transparency) {
                inside = (p[3] > 64.0f);
            } else {
                float diff = std::sqrt((p[0] - bg_r) * (p[0] - bg_r) + (p[1] - bg_g) * (p[1] - bg_g) + (p[2] - bg_b) * (p[2] - bg_b));
                inside = (diff > 28.0f);
            }
            if (inside) {
                mask[idx] = 1;
                ++fg_count;
            }
        }
    }

    // Fallback if solid full-frame photo: treat central 78% subject as foreground
    if (fg_count < static_cast<size_t>(N * N / 40)) {
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                float ux = (x + 0.5f) / N * 2.0f - 1.0f;
                float uy = (y + 0.5f) / N * 2.0f - 1.0f;
                if (ux * ux + uy * uy < 0.72f) mask[static_cast<size_t>(y * N + x)] = 1;
            }
        }
    }

    // Separable 2-pass exact squared Euclidean Distance Transform
    const float INF = 1e12f;
    auto edt_2d = [&](uint8_t target_val) -> std::vector<float> {
        std::vector<float> g(static_cast<size_t>(N * N), INF);
        for (int i = 0; i < N * N; ++i) {
            if (mask[i] == target_val) g[i] = 0.0f;
        }
        // Horizontal pass
        for (int y = 0; y < N; ++y) {
            float* row = &g[static_cast<size_t>(y * N)];
            for (int x = 1; x < N; ++x) row[x] = std::min(row[x], row[x - 1] + 1.0f);
            for (int x = N - 2; x >= 0; --x) row[x] = std::min(row[x], row[x + 1] + 1.0f);
            for (int x = 0; x < N; ++x) row[x] = row[x] * row[x];
        }
        // Vertical pass
        std::vector<float> out(static_cast<size_t>(N * N), INF);
        for (int x = 0; x < N; ++x) {
            for (int y = 0; y < N; ++y) {
                float best = INF;
                for (int ky = 0; ky < N; ++ky) {
                    float dy = static_cast<float>(y - ky);
                    float d2 = g[static_cast<size_t>(ky * N + x)] + dy * dy;
                    if (d2 < best) best = d2;
                }
                out[static_cast<size_t>(y * N + x)] = std::sqrt(best);
            }
        }
        return out;
    };

    auto d_to_out = edt_2d(0); // distance inside foreground to outside
    auto d_to_in  = edt_2d(1); // distance outside foreground to inside

    float max_in = 1.0f;
    for (float v : d_to_out) max_in = std::max(max_in, v);

    float px_to_world = 2.0f / static_cast<float>(N);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            size_t idx = static_cast<size_t>(y * N + x);
            float signed_px = mask[idx] ? -d_to_out[idx] : d_to_in[idx];
            pv.sdf2d[idx] = signed_px * px_to_world;

            // Luminance + cushion depth cue
            const auto& c = pv.rgb[idx];
            float lum = (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255.0f;
            float cushion = mask[idx] ? std::sqrt(std::clamp(d_to_out[idx] / max_in, 0.0f, 1.0f)) : 0.0f;
            pv.depth2d[idx] = 0.72f * cushion + 0.28f * lum;
        }
    }
    return pv;
}

inline float sample_bilinear(const std::vector<float>& grid, int N, float u01, float v01) {
    float x = std::clamp(u01, 0.0f, 1.0f) * (N - 1);
    float y = std::clamp(1.0f - v01, 0.0f, 1.0f) * (N - 1); // flip Y so +Y world = top of image
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = std::min(x0 + 1, N - 1);
    int y1 = static_cast<int>(y0 + 1 < N ? y0 + 1 : N - 1);
    float fx = x - x0, fy = y - y0;
    float a = grid[static_cast<size_t>(y0 * N + x0)] * (1.0f - fx) + grid[static_cast<size_t>(y0 * N + x1)] * fx;
    float b = grid[static_cast<size_t>(y1 * N + x0)] * (1.0f - fx) + grid[static_cast<size_t>(y1 * N + x1)] * fx;
    return a * (1.0f - fy) + b * fy;
}

inline ColorRGBA sample_color(const PreparedView2D& pv, float u01, float v01) {
    int x = std::clamp(static_cast<int>(std::round(u01 * (pv.N - 1))), 0, pv.N - 1);
    int y = std::clamp(static_cast<int>(std::round((1.0f - v01) * (pv.N - 1))), 0, pv.N - 1);
    return pv.rgb[static_cast<size_t>(y * pv.N + x)];
}

} // namespace mv_detail

inline Scene3D reconstruct_multiview_to_3d_cpp(
    const std::vector<uint8_t>& front_bytes,
    const std::vector<uint8_t>& side_bytes,
    const std::vector<uint8_t>& back_bytes,
    const std::vector<uint8_t>& top_bytes,
    int resolution = 96,
    float depth_inflation = 0.68f
) {
    int grid_n = std::clamp(resolution, 48, 128);
    int img_n = 128;

    PreparedView2D v_front = mv_detail::prepare_single_view(front_bytes, img_n);
    PreparedView2D v_side  = mv_detail::prepare_single_view(side_bytes, img_n);
    PreparedView2D v_back  = mv_detail::prepare_single_view(back_bytes, img_n);
    PreparedView2D v_top   = mv_detail::prepare_single_view(top_bytes, img_n);

    if (!v_front.valid && v_side.valid) v_front = v_side;
    if (!v_front.valid) throw std::runtime_error("At least a Front reference image is required");

    bool multi_view = v_side.valid || v_top.valid;

    Vec3 orig{-1.15f, -1.15f, -1.15f};
    float span = 2.30f;
    float sp = span / static_cast<float>(grid_n - 1);
    Grid3D grid(grid_n, grid_n, grid_n, orig, {sp, sp, sp});

    #pragma omp parallel for schedule(static)
    for (int ix = 0; ix < grid_n; ++ix) {
        for (int iy = 0; iy < grid_n; ++iy) {
            for (int iz = 0; iz < grid_n; ++iz) {
                Vec3 p = grid.pos(static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz));
                float ux = p.x * 0.5f + 0.5f;
                float uy = p.y * 0.5f + 0.5f;
                float uz = p.z * 0.5f + 0.5f;

                float d_front = mv_detail::sample_bilinear(v_front.sdf2d, img_n, ux, uy);
                if (v_back.valid && p.z < 0.0f) {
                    float d_back = mv_detail::sample_bilinear(v_back.sdf2d, img_n, 1.0f - ux, uy);
                    d_front = 0.5f * (d_front + d_back);
                }

                float sdf = d_front;
                if (multi_view) {
                    // True Multi-View Visual Hull Intersection with smooth bevel rounding!
                    if (v_side.valid) {
                        float d_side = mv_detail::sample_bilinear(v_side.sdf2d, img_n, uz, uy);
                        // Smooth intersection (smax)
                        float k = 0.06f;
                        float h = std::clamp(0.5f + 0.5f * (d_side - sdf) / k, 0.0f, 1.0f);
                        sdf = d_side * h + sdf * (1.0f - h) + k * h * (1.0f - h);
                    }
                    if (v_top.valid) {
                        float d_top = mv_detail::sample_bilinear(v_top.sdf2d, img_n, ux, uz);
                        sdf = std::max(sdf, d_top);
                    }
                    // Subtle monocular relief modulation from front/back luminance
                    float rel = mv_detail::sample_bilinear(v_front.depth2d, img_n, ux, uy);
                    sdf -= 0.025f * (rel - 0.5f);
                } else {
                    // Single-view + optional Back-view cushion + shape-from-shading inflation
                    float f_dep = mv_detail::sample_bilinear(v_front.depth2d, img_n, ux, uy);
                    float b_dep = v_back.valid
                        ? mv_detail::sample_bilinear(v_back.depth2d, img_n, 1.0f - ux, uy)
                        : f_dep;
                    float half_thick = std::max(0.02f, depth_inflation * 0.55f * (0.5f * (f_dep + b_dep)));
                    float z_sdf = std::fabs(p.z) - half_thick;
                    sdf = std::max(d_front, z_sdf);
                }
                grid.at(ix, iy, iz) = sdf;
            }
        }
    }

    Mesh mesh = extract_isosurface_surface_nets(std::move(grid));
    mesh.taubin_smooth(4);
    mesh.recompute_normals();

    // Multi-View Normal-Weighted Photo RGB Surface Projection!
    mesh.colors.resize(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        const Vec3& p = mesh.positions[i];
        const Vec3& n = mesh.normals[i];
        float ux = std::clamp(p.x * 0.5f + 0.5f, 0.0f, 1.0f);
        float uy = std::clamp(p.y * 0.5f + 0.5f, 0.0f, 1.0f);
        float uz = std::clamp(p.z * 0.5f + 0.5f, 0.0f, 1.0f);

        float wf = std::max(0.05f, n.z);
        float wb = std::max(0.05f, -n.z);
        float ws = v_side.valid ? std::fabs(n.x) * 0.85f : 0.0f;
        float wt = v_top.valid ? std::max(0.0f, n.y) * 0.75f : 0.0f;

        ColorRGBA cf = mv_detail::sample_color(v_front, ux, uy);
        ColorRGBA cb = v_back.valid ? mv_detail::sample_color(v_back, 1.0f - ux, uy) : cf;
        ColorRGBA cs = v_side.valid ? mv_detail::sample_color(v_side, uz, uy) : cf;
        ColorRGBA ct = v_top.valid  ? mv_detail::sample_color(v_top, ux, uz)  : cf;

        float wsum = wf + wb + ws + wt;
        float r = (wf * cf.r + wb * cb.r + ws * cs.r + wt * ct.r) / wsum;
        float g = (wf * cf.g + wb * cb.g + ws * cs.g + wt * ct.g) / wsum;
        float b = (wf * cf.b + wb * cb.b + ws * cs.b + wt * ct.b) / wsum;

        mesh.colors[i] = {
            static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)),
            255
        };
    }

    mesh.id = "multiview_projected_mesh";
    mesh.name = "Multi-View Photo-Textured 3D Mesh";
    mesh.material = {"PhotoProjMat", "#ffffff", 0.08f, 0.52f, "", 0.0f, "organic_skin", 1024};

    Scene3D sc;
    sc.parts.push_back(std::move(mesh));
    sc.ground_on_y0();
    return sc;
}

// ---------------------------------------------------------------------------
// Meshy 7 Style Region-Level Texture Edit / 3D Brush Inpainting
// Repaints or heals a spatial 3D sphere region (or part) with smooth falloff
// without regenerating geometry!
// ---------------------------------------------------------------------------
inline size_t paint_mesh_region_3d(
    Scene3D& scene,
    Vec3 center,
    float radius,
    const std::string& hex_color,
    float feather = 0.35f,
    const std::string& target_part_id = ""
) {
    ColorRGBA target_c = ColorRGBA::from_hex(hex_color);
    size_t painted_verts = 0;
    float r_max = std::max(0.05f, radius);
    float r_inner = r_max * (1.0f - std::clamp(feather, 0.05f, 0.95f));

    for (auto& m : scene.parts) {
        if (!target_part_id.empty() && m.id != target_part_id) continue;
        // Initialize vertex colors from base material if not already per-vertex painted
        if (!m.has_vertex_colors()) {
            ColorRGBA base_c = ColorRGBA::from_hex(m.material.color_hex);
            m.colors.assign(m.positions.size(), base_c);
        }
        for (size_t i = 0; i < m.positions.size(); ++i) {
            float d = (m.positions[i] - center).length();
            if (d > r_max) continue;
            float t = (d <= r_inner) ? 1.0f : (1.0f - (d - r_inner) / std::max(1e-4f, r_max - r_inner));
            t = t * t * (3.0f - 2.0f * t);
            auto& vc = m.colors[i];
            vc.r = static_cast<uint8_t>(std::clamp(vc.r * (1.0f - t) + target_c.r * t, 0.0f, 255.0f));
            vc.g = static_cast<uint8_t>(std::clamp(vc.g * (1.0f - t) + target_c.g * t, 0.0f, 255.0f));
            vc.b = static_cast<uint8_t>(std::clamp(vc.b * (1.0f - t) + target_c.b * t, 0.0f, 255.0f));
            ++painted_verts;
        }
    }
    return painted_verts;
}

} // namespace openmesh
