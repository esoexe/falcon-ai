#pragma once
/**
 * OpenMesh Studio — Native C++20 Image-to-3D & Sketch-to-3D Volumetric Reconstruction.
 * Uses libpng + libjpeg for zero-Python image decoding, exact O(N) Felzenszwalb-Huttenlocher
 * 2D Euclidean Distance Transform, Cushion Inflation / Bas-Relief SDF, and Surface Nets.
 */

#include "marching_cubes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <jpeglib.h>
#include <png.h>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace openmesh {

struct ImageRGBA {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // R, G, B, A (4 bytes per pixel)
};

inline bool decode_png_memory(const uint8_t* data, size_t len, ImageRGBA& out) {
    if (len < 8 || std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0) return false;
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, data, len)) return false;
    image.format = PNG_FORMAT_RGBA;
    out.width = static_cast<int>(image.width);
    out.height = static_cast<int>(image.height);
    out.pixels.resize(PNG_IMAGE_SIZE(image));
    if (!png_image_finish_read(&image, nullptr, out.pixels.data(), 0, nullptr)) {
        png_image_free(&image);
        return false;
    }
    return true;
}

inline bool decode_jpeg_memory(const uint8_t* data, size_t len, ImageRGBA& out) {
    if (len < 3 || data[0] != 0xFF || data[1] != 0xD8) return false;
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, const_cast<unsigned char*>(data), static_cast<unsigned long>(len));
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    out.width = static_cast<int>(cinfo.output_width);
    out.height = static_cast<int>(cinfo.output_height);
    out.pixels.resize(static_cast<size_t>(out.width * out.height * 4), 255);

    std::vector<uint8_t> row(static_cast<size_t>(out.width * 3));
    while (cinfo.output_scanline < cinfo.output_height) {
        int y = static_cast<int>(cinfo.output_scanline);
        JSAMPROW row_ptr = row.data();
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);
        for (int x = 0; x < out.width; ++x) {
            size_t di = static_cast<size_t>((y * out.width + x) * 4);
            out.pixels[di + 0] = row[static_cast<size_t>(x * 3 + 0)];
            out.pixels[di + 1] = row[static_cast<size_t>(x * 3 + 1)];
            out.pixels[di + 2] = row[static_cast<size_t>(x * 3 + 2)];
            out.pixels[di + 3] = 255;
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

inline ImageRGBA decode_image_bytes(const std::vector<uint8_t>& bytes) {
    ImageRGBA img;
    if (decode_png_memory(bytes.data(), bytes.size(), img)) return img;
    if (decode_jpeg_memory(bytes.data(), bytes.size(), img)) return img;
    throw std::runtime_error("Unsupported or corrupt image format (upload PNG or JPEG)");
}

// Bilinear resize into a padded square RGBA canvas
inline ImageRGBA resize_into_padded_square(const ImageRGBA& src, int res) {
    ImageRGBA dst;
    dst.width = res;
    dst.height = res;
    dst.pixels.assign(static_cast<size_t>(res * res * 4), 0);

    bool opaque = true;
    for (size_t i = 3; i < src.pixels.size(); i += 4) {
        if (src.pixels[i] < 245) { opaque = false; break; }
    }
    if (opaque && !src.pixels.empty()) {
        for (int p = 0; p < res * res; ++p) {
            dst.pixels[static_cast<size_t>(p * 4 + 0)] = src.pixels[0];
            dst.pixels[static_cast<size_t>(p * 4 + 1)] = src.pixels[1];
            dst.pixels[static_cast<size_t>(p * 4 + 2)] = src.pixels[2];
            dst.pixels[static_cast<size_t>(p * 4 + 3)] = 255;
        }
    }

    float scale = static_cast<float>(res - 16) / static_cast<float>(std::max({src.width, src.height, 1}));
    int nw = std::max(16, static_cast<int>(std::round(src.width * scale)));
    int nh = std::max(16, static_cast<int>(std::round(src.height * scale)));
    int ox = (res - nw) / 2;
    int oy = (res - nh) / 2;

    for (int y = 0; y < nh; ++y) {
        float sy = (static_cast<float>(y) / std::max(1, nh - 1)) * (src.height - 1);
        int y0 = std::clamp(static_cast<int>(sy), 0, src.height - 1);
        for (int x = 0; x < nw; ++x) {
            float sx = (static_cast<float>(x) / std::max(1, nw - 1)) * (src.width - 1);
            int x0 = std::clamp(static_cast<int>(sx), 0, src.width - 1);
            size_t si = static_cast<size_t>((y0 * src.width + x0) * 4);
            size_t di = static_cast<size_t>(((oy + y) * res + (ox + x)) * 4);
            for (int c = 0; c < 4; ++c) dst.pixels[di + c] = src.pixels[si + c];
        }
    }
    return dst;
}

// Exact O(W*H) 2D Squared Euclidean Distance Transform (Felzenszwalb & Huttenlocher)
inline std::vector<float> edt_2d(const std::vector<uint8_t>& binary_zero_targets, int w, int h) {
    const float INF = 1e12f;
    std::vector<float> f(static_cast<size_t>(w * h), INF);
    for (int i = 0; i < w * h; ++i) {
        if (binary_zero_targets[static_cast<size_t>(i)] == 0) f[static_cast<size_t>(i)] = 0.0f;
    }

    auto edt_1d = [](const std::vector<float>& src, std::vector<float>& dst, int n) {
        std::vector<int> v(static_cast<size_t>(n));
        std::vector<float> z(static_cast<size_t>(n + 1));
        int k = 0;
        v[0] = 0;
        z[0] = -1e20f;
        z[1] = 1e20f;
        for (int q = 1; q < n; ++q) {
            float s = 0.0f;
            while (true) {
                int vk = v[static_cast<size_t>(k)];
                s = ((src[static_cast<size_t>(q)] + q * q) - (src[static_cast<size_t>(vk)] + vk * vk)) / (2.0f * q - 2.0f * vk);
                if (s > z[static_cast<size_t>(k)]) break;
                --k;
            }
            ++k;
            v[static_cast<size_t>(k)] = q;
            z[static_cast<size_t>(k)] = s;
            z[static_cast<size_t>(k + 1)] = 1e20f;
        }
        k = 0;
        for (int q = 0; q < n; ++q) {
            while (z[static_cast<size_t>(k + 1)] < q) ++k;
            int vk = v[static_cast<size_t>(k)];
            dst[static_cast<size_t>(q)] = static_cast<float>((q - vk) * (q - vk)) + src[static_cast<size_t>(vk)];
        }
    };

    // Transform columns
    std::vector<float> col_in(static_cast<size_t>(h)), col_out(static_cast<size_t>(h));
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) col_in[static_cast<size_t>(y)] = f[static_cast<size_t>(y * w + x)];
        edt_1d(col_in, col_out, h);
        for (int y = 0; y < h; ++y) f[static_cast<size_t>(y * w + x)] = col_out[static_cast<size_t>(y)];
    }

    // Transform rows
    std::vector<float> row_in(static_cast<size_t>(w)), row_out(static_cast<size_t>(w));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) row_in[static_cast<size_t>(x)] = f[static_cast<size_t>(y * w + x)];
        edt_1d(row_in, row_out, w);
        for (int x = 0; x < w; ++x) f[static_cast<size_t>(y * w + x)] = std::sqrt(std::max(0.0f, row_out[static_cast<size_t>(x)]));
    }
    return f;
}

inline void box_blur_2d(std::vector<float>& buf, int w, int h, int passes = 2) {
    std::vector<float> tmp(buf.size());
    for (int p = 0; p < passes; ++p) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float sum = 0.0f;
                int cnt = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    int ny = std::clamp(y + dy, 0, h - 1);
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = std::clamp(x + dx, 0, w - 1);
                        sum += buf[static_cast<size_t>(ny * w + nx)];
                        ++cnt;
                    }
                }
                tmp[static_cast<size_t>(y * w + x)] = sum / cnt;
            }
        }
        buf.swap(tmp);
    }
}

inline Scene3D reconstruct_image_to_3d_cpp(
    const std::vector<uint8_t>& image_bytes,
    const std::string& mode = "volumetric",
    float depth_strength = 0.68f,
    float detail_relief = 0.38f,
    int resolution = 116
) {
    int res = std::clamp(resolution, 64, 160);
    ImageRGBA raw = decode_image_bytes(image_bytes);
    ImageRGBA canvas = resize_into_padded_square(raw, res);

    // 1. Segment foreground mask
    int transparent_cnt = 0;
    for (int i = 0; i < res * res; ++i) {
        if (canvas.pixels[static_cast<size_t>(i * 4 + 3)] < 128) ++transparent_cnt;
    }

    std::vector<uint8_t> mask(static_cast<size_t>(res * res), 0);
    if (transparent_cnt > (res * res) / 25) {
        for (int i = 0; i < res * res; ++i) {
            mask[static_cast<size_t>(i)] = (canvas.pixels[static_cast<size_t>(i * 4 + 3)] > 64) ? 1 : 0;
        }
    } else {
        // Estimate corner background RGB
        float br = canvas.pixels[0], bg = canvas.pixels[1], bb = canvas.pixels[2];
        for (int i = 0; i < res * res; ++i) {
            float dr = canvas.pixels[static_cast<size_t>(i * 4 + 0)] - br;
            float dg = canvas.pixels[static_cast<size_t>(i * 4 + 1)] - bg;
            float db = canvas.pixels[static_cast<size_t>(i * 4 + 2)] - bb;
            float dist = std::sqrt(dr * dr + dg * dg + db * db);
            mask[static_cast<size_t>(i)] = (dist > 28.0f) ? 1 : 0;
        }
    }

    // Flood-fill exterior background to close interior holes cleanly
    std::vector<uint8_t> ext_bg(static_cast<size_t>(res * res), 0);
    std::queue<int> q;
    for (int i = 0; i < res; ++i) {
        int borders[4] = {i, (res - 1) * res + i, i * res, i * res + (res - 1)};
        for (int idx : borders) {
            if (mask[static_cast<size_t>(idx)] == 0 && !ext_bg[static_cast<size_t>(idx)]) {
                ext_bg[static_cast<size_t>(idx)] = 1;
                q.push(idx);
            }
        }
    }
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cy = cur / res, cx = cur % res;
        const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (const auto& d : dirs) {
            int nx = cx + d[0], ny = cy + d[1];
            if (nx >= 0 && nx < res && ny >= 0 && ny < res) {
                int ni = ny * res + nx;
                if (mask[static_cast<size_t>(ni)] == 0 && !ext_bg[static_cast<size_t>(ni)]) {
                    ext_bg[static_cast<size_t>(ni)] = 1;
                    q.push(ni);
                }
            }
        }
    }
    int fg_sum = 0;
    for (int i = 0; i < res * res; ++i) {
        mask[static_cast<size_t>(i)] = ext_bg[static_cast<size_t>(i)] ? 0 : 1;
        fg_sum += mask[static_cast<size_t>(i)];
    }
    if (fg_sum < 30) {
        for (int y = 0; y < res; ++y) {
            for (int x = 0; x < res; ++x) {
                float dx = (x - res * 0.5f) / (res * 0.35f);
                float dy = (y - res * 0.5f) / (res * 0.35f);
                mask[static_cast<size_t>(y * res + x)] = (dx * dx + dy * dy <= 1.0f) ? 1 : 0;
            }
        }
    }

    // Propagate foreground RGB outward via BFS so silhouette edge vertices get pure object colors!
    std::vector<ColorRGBA> clean_rgb(static_cast<size_t>(res * res));
    std::vector<uint8_t> visited(static_cast<size_t>(res * res), 0);
    std::queue<int> cq;
    for (int i = 0; i < res * res; ++i) {
        clean_rgb[static_cast<size_t>(i)] = {
            canvas.pixels[static_cast<size_t>(i * 4 + 0)],
            canvas.pixels[static_cast<size_t>(i * 4 + 1)],
            canvas.pixels[static_cast<size_t>(i * 4 + 2)],
            255
        };
        if (mask[static_cast<size_t>(i)]) {
            visited[static_cast<size_t>(i)] = 1;
            cq.push(i);
        }
    }
    while (!cq.empty()) {
        int cur = cq.front(); cq.pop();
        int cy = cur / res, cx = cur % res;
        const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (const auto& d : dirs) {
            int nx = cx + d[0], ny = cy + d[1];
            if (nx >= 0 && nx < res && ny >= 0 && ny < res) {
                int ni = ny * res + nx;
                if (!visited[static_cast<size_t>(ni)]) {
                    visited[static_cast<size_t>(ni)] = 1;
                    clean_rgb[static_cast<size_t>(ni)] = clean_rgb[static_cast<size_t>(cur)];
                    cq.push(ni);
                }
            }
        }
    }

    // Compute luminance & detail relief
    std::vector<float> lum(static_cast<size_t>(res * res));
    for (int i = 0; i < res * res; ++i) {
        const auto& c = clean_rgb[static_cast<size_t>(i)];
        lum[static_cast<size_t>(i)] = (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255.0f;
    }
    std::vector<float> lum_blur = lum;
    box_blur_2d(lum_blur, res, res, 4);

    // Distance inside foreground (zeros outside) & distance outside foreground (zeros inside)
    std::vector<uint8_t> inv_mask(mask.size());
    for (size_t i = 0; i < mask.size(); ++i) inv_mask[i] = mask[i] ? 0 : 1;
    std::vector<float> dist_in = edt_2d(mask, res, res);      // 0 where mask==0, >0 inside
    std::vector<float> dist_out = edt_2d(inv_mask, res, res); // 0 where mask==1, >0 outside

    float max_din = 2.0f;
    for (float v : dist_in) max_din = std::max(max_din, v);

    Mesh mesh;
    if (mode == "medallion") {
        int nz = 48;
        float z_min = -0.10f, z_max = 0.58f;
        float sxy = 2.0f / (res - 1);
        float sz = (z_max - z_min) / (nz - 1);
        Grid3D grid(res, res, nz, {-1.0f, -1.0f, z_min}, {sxy, sxy, sz});

        for (int ix = 0; ix < res; ++ix) {
            bool bx = (ix == 0 || ix == res - 1);
            float px = -1.0f + ix * sxy;
            for (int iy = 0; iy < res; ++iy) {
                bool by = (iy == 0 || iy == res - 1);
                float py = -1.0f + iy * sxy;
                int img_row = (res - 1) - iy;
                size_t pidx = static_cast<size_t>(img_row * res + ix);

                float rad = std::sqrt(px * px + py * py);
                float disc_sdf = rad - 0.88f;
                float h_top = 0.13f;
                if (mask[pidx] && rad < 0.86f) {
                    float hi = lum[pidx] - lum_blur[pidx];
                    h_top += 0.25f * depth_strength * (0.35f + 0.65f * lum[pidx] + 0.55f * detail_relief * hi);
                }
                if (rad >= 0.82f && rad <= 0.88f) h_top = std::max(h_top, 0.21f);

                for (int iz = 0; iz < nz; ++iz) {
                    if (bx || by || iz == 0 || iz == nz - 1) { grid.at(ix, iy, iz) = 1.0f; continue; }
                    float pz = z_min + iz * sz;
                    float z_sdf = std::max(-pz, pz - h_top);
                    grid.at(ix, iy, iz) = std::max(disc_sdf, z_sdf);
                }
            }
        }
        mesh = extract_isosurface_surface_nets(grid);
        mesh.colors.resize(mesh.positions.size());
        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            const auto& p = mesh.positions[i];
            int cx = std::clamp(static_cast<int>(std::round((p.x + 1.0f) * 0.5f * (res - 1))), 0, res - 1);
            int cy = std::clamp(static_cast<int>(std::round((1.0f - (p.y + 1.0f) * 0.5f) * (res - 1))), 0, res - 1);
            size_t pidx = static_cast<size_t>(cy * res + cx);
            if (mask[pidx] && p.z > 0.145f) mesh.colors[i] = clean_rgb[pidx];
            else mesh.colors[i] = {218, 178, 56, 255};
        }
    } else {
        // 360° Volumetric Cushion Inflation
        std::vector<float> half_th(static_cast<size_t>(res * res), 0.0f);
        for (int i = 0; i < res * res; ++i) {
            if (!mask[static_cast<size_t>(i)]) continue;
            float nd = std::clamp(dist_in[static_cast<size_t>(i)] / max_din, 0.0f, 1.0f);
            float hi = lum[static_cast<size_t>(i)] - lum_blur[static_cast<size_t>(i)];
            float th = std::clamp(depth_strength, 0.15f, 1.4f) * 0.48f * std::pow(nd, 0.58f)
                     + detail_relief * 0.10f * (0.35f * (lum[static_cast<size_t>(i)] - 0.5f) + hi);
            half_th[static_cast<size_t>(i)] = std::max(0.03f, th);
        }
        box_blur_2d(half_th, res, res, 2);

        int nz = 68;
        float zb = 0.72f;
        float sxy = 2.0f / (res - 1);
        float sz = (2.0f * zb) / (nz - 1);
        Grid3D grid(res, res, nz, {-1.0f, -1.0f, -zb}, {sxy, sxy, sz});

        #pragma omp parallel for schedule(static)
        for (int ix = 0; ix < res; ++ix) {
            bool bx = (ix == 0 || ix == res - 1);
            for (int iy = 0; iy < res; ++iy) {
                bool by = (iy == 0 || iy == res - 1);
                int img_row = (res - 1) - iy;
                size_t pidx = static_cast<size_t>(img_row * res + ix);
                float xy_sdf = mask[pidx]
                    ? (-dist_in[pidx] * sxy)
                    : (dist_out[pidx] * sxy);
                float hth = half_th[pidx];

                for (int iz = 0; iz < nz; ++iz) {
                    if (bx || by || iz == 0 || iz == nz - 1) { grid.at(ix, iy, iz) = 1.0f; continue; }
                    float pz = -zb + iz * sz;
                    float z_sdf = std::fabs(pz) - hth;
                    grid.at(ix, iy, iz) = std::max(xy_sdf, z_sdf);
                }
            }
        }
        mesh = extract_isosurface_surface_nets(grid);
        mesh.colors.resize(mesh.positions.size());
        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            const auto& p = mesh.positions[i];
            int cx = std::clamp(static_cast<int>(std::round((p.x + 1.0f) * 0.5f * (res - 1))), 0, res - 1);
            int cy = std::clamp(static_cast<int>(std::round((1.0f - (p.y + 1.0f) * 0.5f) * (res - 1))), 0, res - 1);
            mesh.colors[i] = clean_rgb[static_cast<size_t>(cy * res + cx)];
        }
    }

    mesh.id = "reconstructed_3d";
    mesh.name = "Reconstructed 3D Volume";
    mesh.primitive_type = mode;
    mesh.material.name = "VertexColorMat";
    mesh.material.color_hex = "#ffffff";
    mesh.material.metallic = (mode == "medallion") ? 0.55f : 0.12f;
    mesh.material.roughness = (mode == "medallion") ? 0.35f : 0.48f;

    Scene3D sc;
    sc.parts.push_back(std::move(mesh));
    sc.ground_on_y0();
    return sc;
}

} // namespace openmesh
