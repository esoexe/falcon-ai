#pragma once
/**
 * OpenMesh Studio — Native C++20 Binary .GLB (glTF 2.0 PBR), .3MF (PKZIP+XML via zlib),
 * Binary .STL, .OBJ, and .PLY Exporters + Binary .GLB Scene Reader.
 */

#include "json.hpp"
#include "mesh.hpp"
#include "pbr_textures.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

namespace openmesh {

namespace detail {
inline void append_u16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
inline void append_u32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
inline void append_f32(std::vector<uint8_t>& b, float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(float));
    append_u32(b, u);
}
} // namespace detail

// ---------------------------------------------------------------------------
// 1. Khronos glTF 2.0 Binary (.GLB) Serializer with Embedded PNG PBR Textures,
//    Box-Projected TEXCOORD_0 UVs, Auto-Crease Normals, and Cavity AO!
// ---------------------------------------------------------------------------
inline std::vector<uint8_t> export_glb_bytes(const Scene3D& scene) {
    JsonValue root = JsonValue::object();
    JsonValue asset = JsonValue::object();
    asset["version"] = "2.0";
    asset["generator"] = "Falcon AI C++20 PBR Engine";
    root["asset"] = asset;
    root["scene"] = 0;

    JsonValue nodes = JsonValue::array();
    JsonValue meshes = JsonValue::array();
    JsonValue materials = JsonValue::array();
    JsonValue accessors = JsonValue::array();
    JsonValue bufferViews = JsonValue::array();
    JsonValue images = JsonValue::array();
    JsonValue textures = JsonValue::array();
    JsonValue samplers = JsonValue::array({
        JsonValue::object({
            {"magFilter", 9729},
            {"minFilter", 9987},
            {"wrapS", 10497},
            {"wrapT", 10497}
        })
    });
    JsonValue scene_nodes = JsonValue::array();

    std::vector<uint8_t> bin;
    auto align4 = [&]() {
        while (bin.size() % 4 != 0) bin.push_back(0);
    };

    auto embed_png_texture = [&](const std::vector<uint8_t>& png_bytes) -> int {
        if (png_bytes.empty()) return -1;
        align4();
        size_t off = bin.size();
        bin.insert(bin.end(), png_bytes.begin(), png_bytes.end());
        size_t bv_idx = bufferViews.size();
        bufferViews.push_back(JsonValue::object({
            {"buffer", 0},
            {"byteOffset", off},
            {"byteLength", png_bytes.size()}
        }));
        size_t img_idx = images.size();
        images.push_back(JsonValue::object({
            {"bufferView", bv_idx},
            {"mimeType", "image/png"}
        }));
        size_t tex_idx = textures.size();
        textures.push_back(JsonValue::object({
            {"sampler", 0},
            {"source", img_idx}
        }));
        return static_cast<int>(tex_idx);
    };

    std::map<std::string, std::array<int, 3>> finish_tex_cache;

    for (size_t i = 0; i < scene.parts.size(); ++i) {
        const auto& m = scene.parts[i];
        if (m.positions.empty() || m.faces.empty()) continue;

        size_t node_idx = nodes.size();
        scene_nodes.push_back(node_idx);

        bool img_vert_colors = m.has_vertex_colors();
        std::string fin = m.material.finish.empty() ? "forged_metal" : m.material.finish;

        JsonValue mat = JsonValue::object();
        mat["name"] = m.material.name;
        mat["doubleSided"] = true;
        mat["extras"] = JsonValue::object({
            {"finish", fin},
            {"ao_baked", !img_vert_colors},
            {"raw_metallic", m.material.metallic},
            {"raw_roughness", m.material.roughness}
        });

        JsonValue pbr = JsonValue::object();
        if (img_vert_colors) {
            pbr["baseColorFactor"] = JsonValue::array({1.0, 1.0, 1.0, 1.0});
            pbr["metallicFactor"] = m.material.metallic;
            pbr["roughnessFactor"] = m.material.roughness;
        } else {
            auto c4 = ColorRGBA::from_hex(m.material.color_hex).to_float4();
            pbr["baseColorFactor"] = JsonValue::array({c4[0], c4[1], c4[2], 1.0});
            pbr["metallicFactor"] = 1.0;
            pbr["roughnessFactor"] = 1.0;

            std::string fkey = fin + "_" + std::to_string(static_cast<int>(std::round(m.material.metallic * 10))) +
                               "_" + std::to_string(static_cast<int>(std::round(m.material.roughness * 10)));
            auto cit = finish_tex_cache.find(fkey);
            if (cit == finish_tex_cache.end()) {
                const auto& bundle = get_cached_pbr_textures(fin, m.material.metallic, m.material.roughness);
                int t_alb = embed_png_texture(bundle.albedo_png);
                int t_orm = embed_png_texture(bundle.orm_png);
                int t_nrm = embed_png_texture(bundle.normal_png);
                cit = finish_tex_cache.emplace(fkey, std::array<int, 3>{t_alb, t_orm, t_nrm}).first;
            }
            const auto& tids = cit->second;
            if (tids[0] >= 0) pbr["baseColorTexture"] = JsonValue::object({{"index", tids[0]}});
            if (tids[1] >= 0) {
                pbr["metallicRoughnessTexture"] = JsonValue::object({{"index", tids[1]}});
                mat["occlusionTexture"] = JsonValue::object({{"index", tids[1]}, {"strength", 0.85}});
            }
            if (tids[2] >= 0) {
                mat["normalTexture"] = JsonValue::object({{"index", tids[2]}, {"scale", 0.42}});
            }
        }
        mat["pbrMetallicRoughness"] = pbr;

        if (!m.material.emissive_hex.empty() && m.material.emissive_strength > 0.01f) {
            auto ec = ColorRGBA::from_hex(m.material.emissive_hex).to_float4();
            float s = std::clamp(m.material.emissive_strength, 0.0f, 1.0f);
            mat["emissiveFactor"] = JsonValue::array({ec[0] * s, ec[1] * s, ec[2] * s});
        }
        size_t mat_idx = materials.size();
        materials.push_back(mat);

        // Laplacian Cavity Ambient Occlusion + 2-Pass Smooth Vertical Sky Bounce
        size_t nv = m.positions.size();
        std::vector<float> v_ao(nv, 0.95f);
        if (!img_vert_colors && nv > 0) {
            std::vector<Vec3> nb_sum(nv, Vec3{0, 0, 0});
            std::vector<uint32_t> nb_cnt(nv, 0);
            for (const auto& f : m.faces) {
                nb_sum[f[0]] += m.positions[f[1]] + m.positions[f[2]]; nb_cnt[f[0]] += 2;
                nb_sum[f[1]] += m.positions[f[0]] + m.positions[f[2]]; nb_cnt[f[1]] += 2;
                nb_sum[f[2]] += m.positions[f[0]] + m.positions[f[1]]; nb_cnt[f[2]] += 2;
            }
            AABB mb = m.bounds();
            float diag = std::max(mb.extents().length(), 1e-3f);
            for (size_t v = 0; v < nv; ++v) {
                if (nb_cnt[v] == 0) continue;
                Vec3 avg = nb_sum[v] / static_cast<float>(nb_cnt[v]);
                Vec3 n = (v < m.normals.size()) ? m.normals[v] : Vec3{0, 1, 0};
                float concav = (avg - m.positions[v]).dot(n) / (diag * 0.025f);
                float ao = std::clamp(0.95f - 0.32f * concav, 0.52f, 1.0f);
                float h_t = std::clamp((m.positions[v].y - mb.min.y) / std::max(mb.extents().y, 0.1f), 0.0f, 1.0f);
                ao *= (0.86f + 0.14f * std::sqrt(h_t));
                v_ao[v] = std::clamp(ao, 0.48f, 1.0f);
            }
            // 2-pass AO neighborhood smoothing to prevent 1-ring stipple
            for (int sp = 0; sp < 2; ++sp) {
                std::vector<float> ao_sum(nv, 0.0f);
                for (const auto& f : m.faces) {
                    ao_sum[f[0]] += v_ao[f[1]] + v_ao[f[2]];
                    ao_sum[f[1]] += v_ao[f[0]] + v_ao[f[2]];
                    ao_sum[f[2]] += v_ao[f[0]] + v_ao[f[1]];
                }
                for (size_t v = 0; v < nv; ++v) {
                    if (nb_cnt[v] > 0) {
                        v_ao[v] = 0.45f * v_ao[v] + 0.55f * (ao_sum[v] / static_cast<float>(nb_cnt[v]));
                    }
                }
            }
        }

        // Auto-Creased Normals + Box-Projected TEXCOORD_0 UVs
        struct RenderVert {
            Vec3 pos;
            Vec3 nrm;
            float u, v;
            ColorRGBA col;
        };
        std::vector<RenderVert> rverts;
        std::vector<std::array<uint32_t, 3>> rfaces;
        rverts.reserve(m.faces.size() * 3);
        rfaces.reserve(m.faces.size());

        size_t nf = m.faces.size();
        std::vector<Vec3> fnrms(nf);
        std::vector<int> faxis(nf, 2);
        for (size_t fi = 0; fi < nf; ++fi) {
            const auto& f = m.faces[fi];
            Vec3 fn = (m.positions[f[1]] - m.positions[f[0]]).cross(m.positions[f[2]] - m.positions[f[0]]).normalized();
            fnrms[fi] = fn;
            float ax = std::abs(fn.x), ay = std::abs(fn.y), az = std::abs(fn.z);
            if (ax >= ay && ax >= az) faxis[fi] = (fn.x >= 0) ? 0 : 1;
            else if (ay >= ax && ay >= az) faxis[fi] = (fn.y >= 0) ? 2 : 3;
            else faxis[fi] = (fn.z >= 0) ? 4 : 5;
        }

        constexpr float kUvScale = 1.75f;
        std::map<std::pair<uint32_t, int>, uint32_t> rv_dedup;

        for (size_t fi = 0; fi < nf; ++fi) {
            const auto& f = m.faces[fi];
            const Vec3& fn = fnrms[fi];
            int ax = faxis[fi];
            std::array<uint32_t, 3> out_f{};

            for (int c = 0; c < 3; ++c) {
                uint32_t vi = f[c];
                const Vec3& p = m.positions[vi];
                Vec3 sn = (vi < m.normals.size()) ? m.normals[vi] : fn;
                bool sharp_crease = (sn.dot(fn) < 0.34f);

                float u = 0.0f, v_coord = 0.0f;
                if (ax == 0 || ax == 1) { u = p.z * kUvScale; v_coord = p.y * kUvScale; }
                else if (ax == 2 || ax == 3) { u = p.x * kUvScale; v_coord = p.z * kUvScale; }
                else { u = p.x * kUvScale; v_coord = p.y * kUvScale; }

                ColorRGBA vc{255, 255, 255, 255};
                if (img_vert_colors) {
                    vc = m.colors[vi];
                } else {
                    uint8_t aob = static_cast<uint8_t>(std::clamp(v_ao[vi] * 255.0f, 0.0f, 255.0f));
                    vc = {aob, aob, aob, 255};
                }

                if (sharp_crease) {
                    uint32_t rvi = static_cast<uint32_t>(rverts.size());
                    rverts.push_back({p, fn, u, v_coord, vc});
                    out_f[c] = rvi;
                } else {
                    auto key = std::make_pair(vi, ax);
                    auto it = rv_dedup.find(key);
                    if (it != rv_dedup.end()) {
                        out_f[c] = it->second;
                    } else {
                        uint32_t rvi = static_cast<uint32_t>(rverts.size());
                        rverts.push_back({p, sn, u, v_coord, vc});
                        rv_dedup[key] = rvi;
                        out_f[c] = rvi;
                    }
                }
            }
            rfaces.push_back(out_f);
        }

        // 1. POSITION
        align4();
        size_t pos_offset = bin.size();
        AABB b = m.bounds();
        for (const auto& rv : rverts) {
            detail::append_f32(bin, rv.pos.x);
            detail::append_f32(bin, rv.pos.y);
            detail::append_f32(bin, rv.pos.z);
        }
        size_t pos_bv = bufferViews.size();
        bufferViews.push_back(JsonValue::object({
            {"buffer", 0}, {"byteOffset", pos_offset}, {"byteLength", bin.size() - pos_offset}, {"target", 34962}
        }));
        size_t pos_acc = accessors.size();
        accessors.push_back(JsonValue::object({
            {"bufferView", pos_bv}, {"componentType", 5126}, {"count", rverts.size()}, {"type", "VEC3"},
            {"min", JsonValue::array({b.min.x, b.min.y, b.min.z})},
            {"max", JsonValue::array({b.max.x, b.max.y, b.max.z})}
        }));

        // 2. NORMAL
        align4();
        size_t nrm_offset = bin.size();
        for (const auto& rv : rverts) {
            detail::append_f32(bin, rv.nrm.x);
            detail::append_f32(bin, rv.nrm.y);
            detail::append_f32(bin, rv.nrm.z);
        }
        size_t nrm_bv = bufferViews.size();
        bufferViews.push_back(JsonValue::object({
            {"buffer", 0}, {"byteOffset", nrm_offset}, {"byteLength", bin.size() - nrm_offset}, {"target", 34962}
        }));
        size_t nrm_acc = accessors.size();
        accessors.push_back(JsonValue::object({
            {"bufferView", nrm_bv}, {"componentType", 5126}, {"count", rverts.size()}, {"type", "VEC3"}
        }));

        // 3. TEXCOORD_0
        align4();
        size_t uv_offset = bin.size();
        for (const auto& rv : rverts) {
            detail::append_f32(bin, rv.u);
            detail::append_f32(bin, rv.v);
        }
        size_t uv_bv = bufferViews.size();
        bufferViews.push_back(JsonValue::object({
            {"buffer", 0}, {"byteOffset", uv_offset}, {"byteLength", bin.size() - uv_offset}, {"target", 34962}
        }));
        size_t uv_acc = accessors.size();
        accessors.push_back(JsonValue::object({
            {"bufferView", uv_bv}, {"componentType", 5126}, {"count", rverts.size()}, {"type", "VEC2"}
        }));

        // 4. COLOR_0
        align4();
        size_t col_offset = bin.size();
        for (const auto& rv : rverts) {
            bin.push_back(rv.col.r);
            bin.push_back(rv.col.g);
            bin.push_back(rv.col.b);
            bin.push_back(rv.col.a);
        }
        size_t col_bv = bufferViews.size();
        bufferViews.push_back(JsonValue::object({
            {"buffer", 0}, {"byteOffset", col_offset}, {"byteLength", bin.size() - col_offset}, {"target", 34962}
        }));
        size_t col_acc = accessors.size();
        accessors.push_back(JsonValue::object({
            {"bufferView", col_bv}, {"componentType", 5121}, {"normalized", true}, {"count", rverts.size()}, {"type", "VEC4"}
        }));

        // 5. INDICES
        align4();
        size_t idx_offset = bin.size();
        for (const auto& rf : rfaces) {
            detail::append_u32(bin, rf[0]);
            detail::append_u32(bin, rf[1]);
            detail::append_u32(bin, rf[2]);
        }
        size_t idx_bv = bufferViews.size();
        bufferViews.push_back(JsonValue::object({
            {"buffer", 0}, {"byteOffset", idx_offset}, {"byteLength", bin.size() - idx_offset}, {"target", 34963}
        }));
        size_t idx_acc = accessors.size();
        accessors.push_back(JsonValue::object({
            {"bufferView", idx_bv}, {"componentType", 5125}, {"count", rfaces.size() * 3}, {"type", "SCALAR"}
        }));

        JsonValue attrs = JsonValue::object();
        attrs["POSITION"] = pos_acc;
        attrs["NORMAL"] = nrm_acc;
        attrs["TEXCOORD_0"] = uv_acc;
        attrs["COLOR_0"] = col_acc;

        JsonValue prim = JsonValue::object({
            {"attributes", attrs},
            {"indices", idx_acc},
            {"material", mat_idx}
        });

        size_t mesh_idx = meshes.size();
        meshes.push_back(JsonValue::object({{"name", m.id}, {"primitives", JsonValue::array({prim})}}));
        nodes.push_back(JsonValue::object({{"name", m.id}, {"mesh", mesh_idx}}));
    }

    align4();
    root["scenes"] = JsonValue::array({JsonValue::object({{"nodes", scene_nodes}})});
    root["nodes"] = nodes;
    root["meshes"] = meshes;
    root["materials"] = materials;
    root["accessors"] = accessors;
    root["bufferViews"] = bufferViews;
    if (images.size() > 0) {
        root["images"] = images;
        root["textures"] = textures;
        root["samplers"] = samplers;
    }
    root["buffers"] = JsonValue::array({JsonValue::object({{"byteLength", bin.size()}})});

    std::string json_str = root.dump(0);
    while (json_str.size() % 4 != 0) json_str.push_back(' ');

    uint32_t total_len = static_cast<uint32_t>(12 + 8 + json_str.size() + 8 + bin.size());
    std::vector<uint8_t> glb;
    glb.reserve(total_len);

    // 12-byte GLB header
    detail::append_u32(glb, 0x46546C67U); // "glTF"
    detail::append_u32(glb, 2U);
    detail::append_u32(glb, total_len);

    // JSON Chunk
    detail::append_u32(glb, static_cast<uint32_t>(json_str.size()));
    detail::append_u32(glb, 0x4E4F534AU); // "JSON"
    glb.insert(glb.end(), json_str.begin(), json_str.end());

    // BIN Chunk
    detail::append_u32(glb, static_cast<uint32_t>(bin.size()));
    detail::append_u32(glb, 0x004E4942U); // "BIN\0"
    glb.insert(glb.end(), bin.begin(), bin.end());

    return glb;
}

// ---------------------------------------------------------------------------
// Native GLB Loader (reads back OpenMesh .glb scenes for post-processing ops!)
// ---------------------------------------------------------------------------
inline Scene3D load_glb_bytes(const std::vector<uint8_t>& glb) {
    Scene3D sc;
    if (glb.size() < 28) return sc;
    uint32_t magic = 0, json_len = 0;
    std::memcpy(&magic, glb.data(), 4);
    if (magic != 0x46546C67U) return sc;
    std::memcpy(&json_len, glb.data() + 12, 4);
    if (20 + json_len > glb.size()) return sc;

    std::string_view jview(reinterpret_cast<const char*>(glb.data() + 20), json_len);
    JsonValue root = JsonValue::parse(jview);

    size_t bin_chunk_hdr = 20 + json_len;
    if (bin_chunk_hdr + 8 > glb.size()) return sc;
    uint32_t bin_len = 0;
    std::memcpy(&bin_len, glb.data() + bin_chunk_hdr, 4);
    const uint8_t* bin_ptr = glb.data() + bin_chunk_hdr + 8;
    if (bin_chunk_hdr + 8 + bin_len > glb.size()) return sc;

    const auto& nodes = root.at_or_null("nodes");
    const auto& meshes = root.at_or_null("meshes");
    const auto& mats = root.at_or_null("materials");
    const auto& accs = root.at_or_null("accessors");
    const auto& bvs = root.at_or_null("bufferViews");

    auto float_to_hex = [](float r, float g, float b) -> std::string {
        auto cl = [](float v) { return static_cast<int>(std::clamp(std::round(v * 255.0f), 0.0f, 255.0f)); };
        std::ostringstream ss;
        ss << "#" << std::hex << std::setfill('0')
           << std::setw(2) << cl(r) << std::setw(2) << cl(g) << std::setw(2) << cl(b);
        return ss.str();
    };

    for (size_t ni = 0; ni < nodes.size(); ++ni) {
        const auto& jn = nodes[ni];
        int mi = jn.get_int("mesh", -1);
        if (mi < 0 || static_cast<size_t>(mi) >= meshes.size()) continue;
        const auto& jm = meshes[static_cast<size_t>(mi)];
        const auto& prims = jm.at_or_null("primitives");
        if (!prims.is_array() || prims.size() == 0) continue;
        const auto& pr = prims[0];
        const auto& attrs = pr.at_or_null("attributes");

        Mesh m;
        m.id = jn.get_str("name", "part_" + std::to_string(ni));
        m.name = m.id;

        bool ao_baked_only = false;
        int mat_i = pr.get_int("material", -1);
        if (mat_i >= 0 && static_cast<size_t>(mat_i) < mats.size()) {
            const auto& jmat = mats[static_cast<size_t>(mat_i)];
            m.material.name = jmat.get_str("name", "Mat");
            const auto& pbr = jmat.at_or_null("pbrMetallicRoughness");
            const auto& ext = jmat.at_or_null("extras");
            m.material.finish = ext.get_str("finish", "forged_metal");
            ao_baked_only = ext.get_bool("ao_baked", false);
            m.material.metallic = static_cast<float>(ext.has("raw_metallic") ? ext.get_num("raw_metallic") : pbr.get_num("metallicFactor", 0.35));
            m.material.roughness = static_cast<float>(ext.has("raw_roughness") ? ext.get_num("raw_roughness") : pbr.get_num("roughnessFactor", 0.42));
            const auto& bcf = pbr.at_or_null("baseColorFactor");
            if (bcf.is_array() && bcf.size() >= 3) {
                m.material.color_hex = float_to_hex(
                    static_cast<float>(bcf[0].as_num(0.6)),
                    static_cast<float>(bcf[1].as_num(0.6)),
                    static_cast<float>(bcf[2].as_num(0.6))
                );
            }
            const auto& emf = jmat.at_or_null("emissiveFactor");
            if (emf.is_array() && emf.size() >= 3) {
                float er = static_cast<float>(emf[0].as_num(0));
                float eg = static_cast<float>(emf[1].as_num(0));
                float eb = static_cast<float>(emf[2].as_num(0));
                float mx = std::max({er, eg, eb});
                if (mx > 0.01f) {
                    m.material.emissive_strength = mx;
                    m.material.emissive_hex = float_to_hex(er / mx, eg / mx, eb / mx);
                }
            }
        }

        // Read POSITION
        int pos_ai = attrs.get_int("POSITION", -1);
        if (pos_ai >= 0) {
            const auto& acc = accs[static_cast<size_t>(pos_ai)];
            size_t count = static_cast<size_t>(acc.get_int("count", 0));
            const auto& bv = bvs[static_cast<size_t>(acc.get_int("bufferView", 0))];
            size_t off = static_cast<size_t>(bv.get_int("byteOffset", 0));
            m.positions.resize(count);
            std::memcpy(m.positions.data(), bin_ptr + off, count * sizeof(Vec3));
        }

        // Read COLOR_0 if present and NOT just grayscale cavity AO
        int col_ai = attrs.get_int("COLOR_0", -1);
        if (col_ai >= 0 && !ao_baked_only) {
            const auto& acc = accs[static_cast<size_t>(col_ai)];
            size_t count = static_cast<size_t>(acc.get_int("count", 0));
            const auto& bv = bvs[static_cast<size_t>(acc.get_int("bufferView", 0))];
            size_t off = static_cast<size_t>(bv.get_int("byteOffset", 0));
            if (acc.get_int("componentType", 0) == 5121) {
                m.colors.resize(count);
                std::memcpy(m.colors.data(), bin_ptr + off, count * 4);
            }
        }

        // Read INDICES
        int idx_ai = pr.get_int("indices", -1);
        if (idx_ai >= 0) {
            const auto& acc = accs[static_cast<size_t>(idx_ai)];
            size_t count = static_cast<size_t>(acc.get_int("count", 0));
            const auto& bv = bvs[static_cast<size_t>(acc.get_int("bufferView", 0))];
            size_t off = static_cast<size_t>(bv.get_int("byteOffset", 0));
            size_t n_tris = count / 3;
            m.faces.resize(n_tris);
            std::memcpy(m.faces.data(), bin_ptr + off, n_tris * 3 * sizeof(uint32_t));
        }

        if (ao_baked_only) {
            m.weld_vertices(1e-5f); // Re-fuse UV/crease seam splits back into watertight CAD manifold
        } else {
            m.recompute_normals();
        }
        sc.parts.push_back(std::move(m));
    }
    return sc;
}

// ---------------------------------------------------------------------------
// 2. Binary .STL Exporter (Z-Up Slicer Convention in Exact Millimeters)
// ---------------------------------------------------------------------------
inline std::vector<uint8_t> export_stl_bytes(const Scene3D& scene, float mm_per_unit = kDefaultMmPerUnit) {
    Mesh m = scene.merged();
    // Convert Studio Y-up to Slicer Z-up (+90 deg around X) & scale to mm
    m.apply_transform(Mat4::rot_x(kPi * 0.5f) * Mat4::scale(mm_per_unit, mm_per_unit, mm_per_unit));
    AABB b = m.bounds();
    if (b.valid()) {
        Vec3 c = b.center();
        m.translate(-c.x, -c.y, -b.min.z);
    }

    uint32_t n_tris = static_cast<uint32_t>(m.faces.size());
    std::vector<uint8_t> out(80, 0);
    const char* hdr = "OpenMesh Studio 2.0 C++20 Engine - Binary STL (millimeters)";
    std::memcpy(out.data(), hdr, std::strlen(hdr));
    out.reserve(84 + static_cast<size_t>(n_tris) * 50);
    detail::append_u32(out, n_tris);

    for (const auto& f : m.faces) {
        const Vec3& v0 = m.positions[f[0]];
        const Vec3& v1 = m.positions[f[1]];
        const Vec3& v2 = m.positions[f[2]];
        Vec3 fn = (v1 - v0).cross(v2 - v0).normalized();
        detail::append_f32(out, fn.x); detail::append_f32(out, fn.y); detail::append_f32(out, fn.z);
        detail::append_f32(out, v0.x); detail::append_f32(out, v0.y); detail::append_f32(out, v0.z);
        detail::append_f32(out, v1.x); detail::append_f32(out, v1.y); detail::append_f32(out, v1.z);
        detail::append_f32(out, v2.x); detail::append_f32(out, v2.y); detail::append_f32(out, v2.z);
        detail::append_u16(out, 0);
    }
    return out;
}

// ---------------------------------------------------------------------------
// 3. Standards-Compliant .3MF Archive Writer (PKZIP + Raw DEFLATE via zlib)
// ---------------------------------------------------------------------------
namespace zip_util {
struct ZipEntry {
    std::string filename;
    std::vector<uint8_t> compressed;
    uint32_t crc = 0;
    uint32_t uncomp_size = 0;
    uint16_t method = 8; // DEFLATE
    uint32_t local_hdr_offset = 0;
};

inline std::vector<uint8_t> raw_deflate(const std::string& input) {
    z_stream zs{};
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());
    std::vector<uint8_t> out(deflateBound(&zs, zs.avail_in));
    zs.next_out = out.data();
    zs.avail_out = static_cast<uInt>(out.size());
    deflate(&zs, Z_FINISH);
    out.resize(zs.total_out);
    deflateEnd(&zs);
    return out;
}

inline std::vector<uint8_t> build_zip(const std::vector<std::pair<std::string, std::string>>& files) {
    std::vector<uint8_t> archive;
    std::vector<ZipEntry> entries;

    for (const auto& kv : files) {
        ZipEntry e;
        e.filename = kv.first;
        e.uncomp_size = static_cast<uint32_t>(kv.second.size());
        e.crc = static_cast<uint32_t>(crc32(0L, reinterpret_cast<const Bytef*>(kv.second.data()), e.uncomp_size));
        e.compressed = raw_deflate(kv.second);
        e.local_hdr_offset = static_cast<uint32_t>(archive.size());

        // Local file header (PK\x03\x04)
        detail::append_u32(archive, 0x04034b50U);
        detail::append_u16(archive, 20);
        detail::append_u16(archive, 0);
        detail::append_u16(archive, e.method);
        detail::append_u16(archive, 0);
        detail::append_u16(archive, 0);
        detail::append_u32(archive, e.crc);
        detail::append_u32(archive, static_cast<uint32_t>(e.compressed.size()));
        detail::append_u32(archive, e.uncomp_size);
        detail::append_u16(archive, static_cast<uint16_t>(e.filename.size()));
        detail::append_u16(archive, 0);
        archive.insert(archive.end(), e.filename.begin(), e.filename.end());
        archive.insert(archive.end(), e.compressed.begin(), e.compressed.end());
        entries.push_back(std::move(e));
    }

    uint32_t cd_offset = static_cast<uint32_t>(archive.size());
    for (const auto& e : entries) {
        detail::append_u32(archive, 0x02014b50U); // Central directory header
        detail::append_u16(archive, 20);
        detail::append_u16(archive, 20);
        detail::append_u16(archive, 0);
        detail::append_u16(archive, e.method);
        detail::append_u16(archive, 0);
        detail::append_u16(archive, 0);
        detail::append_u32(archive, e.crc);
        detail::append_u32(archive, static_cast<uint32_t>(e.compressed.size()));
        detail::append_u32(archive, e.uncomp_size);
        detail::append_u16(archive, static_cast<uint16_t>(e.filename.size()));
        detail::append_u16(archive, 0);
        detail::append_u16(archive, 0);
        detail::append_u16(archive, 0);
        detail::append_u16(archive, 0);
        detail::append_u32(archive, 0);
        detail::append_u32(archive, e.local_hdr_offset);
        archive.insert(archive.end(), e.filename.begin(), e.filename.end());
    }

    uint32_t cd_size = static_cast<uint32_t>(archive.size()) - cd_offset;
    // End of Central Directory (PK\x05\x06)
    detail::append_u32(archive, 0x06054b50U);
    detail::append_u16(archive, 0);
    detail::append_u16(archive, 0);
    detail::append_u16(archive, static_cast<uint16_t>(entries.size()));
    detail::append_u16(archive, static_cast<uint16_t>(entries.size()));
    detail::append_u32(archive, cd_size);
    detail::append_u32(archive, cd_offset);
    detail::append_u16(archive, 0);
    return archive;
}
} // namespace zip_util

inline std::vector<uint8_t> export_3mf_bytes(const Scene3D& scene, const std::string& title = "OpenMesh_Asset", float mm_per_unit = kDefaultMmPerUnit) {
    Mesh m = scene.merged();
    m.apply_transform(Mat4::rot_x(kPi * 0.5f) * Mat4::scale(mm_per_unit, mm_per_unit, mm_per_unit));
    AABB b = m.bounds();
    if (b.valid()) {
        Vec3 c = b.center();
        m.translate(-c.x, -c.y, -b.min.z);
    }

    std::string content_types =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>"
        "</Types>";

    std::string rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>"
        "</Relationships>";

    std::ostringstream xml;
    xml << std::fixed << std::setprecision(4);
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        << "<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">"
        << "<metadata name=\"Application\">OpenMesh Studio 2.0 C++20 Engine</metadata>"
        << "<resources><object id=\"1\" type=\"model\"><mesh><vertices>";
    for (const auto& v : m.positions) {
        xml << "<vertex x=\"" << v.x << "\" y=\"" << v.y << "\" z=\"" << v.z << "\"/>";
    }
    xml << "</vertices><triangles>";
    for (const auto& f : m.faces) {
        xml << "<triangle v1=\"" << f[0] << "\" v2=\"" << f[1] << "\" v3=\"" << f[2] << "\"/>";
    }
    xml << "</triangles></mesh></object></resources><build><item objectid=\"1\"/></build></model>";

    return zip_util::build_zip({
        {"[Content_Types].xml", content_types},
        {"_rels/.rels", rels},
        {"3D/3dmodel.model", xml.str()}
    });
}

// ---------------------------------------------------------------------------
// 4. Wavefront .OBJ & Stanford .PLY Exporters
// ---------------------------------------------------------------------------
inline std::vector<uint8_t> export_obj_bytes(const Scene3D& scene) {
    std::ostringstream oss;
    oss << "# OpenMesh Studio 2.0 (C++20 Engine)\n";
    uint32_t v_offset = 1;
    for (const auto& m : scene.parts) {
        oss << "o " << m.id << "\n";
        for (const auto& v : m.positions) {
            oss << "v " << v.x << " " << v.y << " " << v.z << "\n";
        }
        for (const auto& n : m.normals) {
            oss << "vn " << n.x << " " << n.y << " " << n.z << "\n";
        }
        for (const auto& f : m.faces) {
            uint32_t a = f[0] + v_offset, b = f[1] + v_offset, c = f[2] + v_offset;
            oss << "f " << a << "//" << a << " " << b << "//" << b << " " << c << "//" << c << "\n";
        }
        v_offset += static_cast<uint32_t>(m.positions.size());
    }
    std::string s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::vector<uint8_t> export_ply_bytes(const Scene3D& scene) {
    Mesh m = scene.merged();
    bool has_col = m.has_vertex_colors();
    ColorRGBA def_col = ColorRGBA::from_hex(m.material.color_hex);

    std::ostringstream oss;
    oss << "ply\nformat ascii 1.0\ncomment OpenMesh Studio 2.0 C++20 Engine\n"
        << "element vertex " << m.positions.size() << "\n"
        << "property float x\nproperty float y\nproperty float z\n"
        << "property uchar red\nproperty uchar green\nproperty uchar blue\n"
        << "element face " << m.faces.size() << "\n"
        << "property list uchar int vertex_indices\nend_header\n";

    for (size_t i = 0; i < m.positions.size(); ++i) {
        const auto& v = m.positions[i];
        ColorRGBA c = has_col ? m.colors[i] : def_col;
        oss << v.x << " " << v.y << " " << v.z << " "
            << static_cast<int>(c.r) << " " << static_cast<int>(c.g) << " " << static_cast<int>(c.b) << "\n";
    }
    for (const auto& f : m.faces) {
        oss << "3 " << f[0] << " " << f[1] << " " << f[2] << "\n";
    }
    std::string s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

} // namespace openmesh
