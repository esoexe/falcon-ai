#pragma once
/**
 * Falcon AI v3.0 — Exportable Volumetric Skeletal Auto-Rigger (4-Bone Vertex Skinning)
 * & 24-Clip Categorized Procedural Motion Library (glTF 2.0 `skins` + `animations`).
 *
 * Produces real `JOINTS_0` (VEC4 u16), `WEIGHTS_0` (VEC4 f32), `inverseBindMatrices`,
 * hierarchical skeleton `nodes`, and multi-clip quaternion/translation `animations`
 * directly inside `.GLB` (and feeds `.FBX` skin/animation export).
 */

#include "exporters.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace openmesh {

inline std::string infer_surface_finish(const Mesh& m) {
    return m.material.finish.empty() ? "forged_metal" : m.material.finish;
}

inline std::vector<float> compute_triplanar_uvs(const Mesh& m) {
    std::vector<float> uvs(m.positions.size() * 2, 0.0f);
    for (size_t i = 0; i < m.positions.size(); ++i) {
        const Vec3& p = m.positions[i];
        const Vec3& n = (i < m.normals.size()) ? m.normals[i] : Vec3{0, 1, 0};
        float ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
        if (ax >= ay && ax >= az) { uvs[i * 2] = p.z * 0.65f + 0.5f; uvs[i * 2 + 1] = p.y * 0.65f + 0.5f; }
        else if (ay >= ax && ay >= az) { uvs[i * 2] = p.x * 0.65f + 0.5f; uvs[i * 2 + 1] = p.z * 0.65f + 0.5f; }
        else { uvs[i * 2] = p.x * 0.65f + 0.5f; uvs[i * 2 + 1] = p.y * 0.65f + 0.5f; }
    }
    return uvs;
}

inline std::vector<ColorRGBA> compute_baked_ao_colors(const Mesh& m) {
    size_t nv = m.positions.size();
    std::vector<ColorRGBA> out(nv, ColorRGBA{242, 242, 245, 255});
    if (nv == 0) return out;
    AABB mb = m.bounds();
    float hy = std::max(0.1f, mb.extents().y);
    for (size_t i = 0; i < nv; ++i) {
        float t = std::clamp((m.positions[i].y - mb.min.y) / hy, 0.0f, 1.0f);
        uint8_t v = static_cast<uint8_t>(std::clamp((0.76f + 0.24f * std::sqrt(t)) * 255.0f, 140.0f, 255.0f));
        out[i] = ColorRGBA{v, v, v, 255};
    }
    return out;
}

struct RigBone {
    std::string name;
    int parent = -1;
    Vec3 head_world{0, 0, 0}; // Bind-pose joint world position
    Vec3 tail_world{0, 0, 0}; // Capsule segment end for volumetric heat skinning
    Vec3 local_pos{0, 0, 0};  // Offset relative to parent joint
};

struct MotionClipInfo {
    std::string id;
    std::string name;
    std::string category; // "Locomotion", "Combat", "Emote", "Creature", "Vehicle"
    float duration_sec = 1.6f;
    int keyframes = 25;
};

struct RiggedSkinData {
    std::string rig_type; // "biped", "quadruped", "winged", "vehicle"
    std::vector<RigBone> bones;
    // Flattened per-part vertex skinning
    std::vector<std::vector<std::array<uint16_t, 4>>> part_joints;
    std::vector<std::vector<std::array<float, 4>>>    part_weights;
};

inline const std::vector<MotionClipInfo>& get_motion_library_catalog() {
    static const std::vector<MotionClipInfo> catalog = {
        {"idle_breathe",     "Idle Breathing",        "Locomotion", 2.4f, 25},
        {"walk_standard",    "Natural Walk Cycle",    "Locomotion", 1.2f, 25},
        {"run_sprint",       "Athletic Sprint Run",   "Locomotion", 0.8f, 21},
        {"jump_heroic",      "Heroic Leap & Land",    "Locomotion", 1.4f, 25},
        {"crouch_stealth",   "Tactical Stealth Walk", "Locomotion", 1.6f, 25},
        {"dodge_roll",       "Evasive Sidestep",      "Locomotion", 1.0f, 21},
        {"sword_slash",      "Katana / Sword Slash",  "Combat",     1.1f, 23},
        {"heavy_smash",      "Overhead Heavy Smash",  "Combat",     1.5f, 25},
        {"spin_whirlwind",   "360 Spin Attack",       "Combat",     1.2f, 25},
        {"shield_guard",     "Defensive Guard Brace", "Combat",     1.6f, 21},
        {"martial_kick",     "High Roundhouse Kick",  "Combat",     1.1f, 23},
        {"aim_recoil",       "Rifle Aim & Recoil",    "Combat",     1.2f, 21},
        {"cast_arcane",      "Arcane Spell Cast",     "Combat",     1.8f, 25},
        {"hit_stagger",      "Damage Hit Stagger",    "Combat",     0.9f, 19},
        {"knockdown_ko",     "Dramatic Knockdown",    "Combat",     1.8f, 25},
        {"wave_hello",       "Friendly Wave",         "Emote",      1.8f, 25},
        {"victory_cheer",    "Victory Fist Pump",     "Emote",      2.0f, 25},
        {"dance_groove",     "Rhythm Dance Groove",   "Emote",      2.0f, 29},
        {"zombie_shamble",   "Undead Shambling",      "Emote",      2.0f, 25},
        {"quad_trot",        "Quadruped Trot",        "Creature",   1.0f, 21},
        {"quad_gallop",      "Quadruped Full Gallop", "Creature",   0.75f, 21},
        {"quad_pounce",      "Predator Pounce",       "Creature",   1.3f, 25},
        {"wing_flap_soar",   "Dragon / Winged Flap",  "Creature",   1.4f, 25},
        {"vehicle_cruise",   "Wheel Spin & Suspension","Vehicle",   1.0f, 25}
    };
    return catalog;
}

namespace rig_detail {

inline std::array<float, 4> euler_xyz_to_quat(float rx, float ry, float rz) {
    float cx = std::cos(rx * 0.5f), sx = std::sin(rx * 0.5f);
    float cy = std::cos(ry * 0.5f), sy = std::sin(ry * 0.5f);
    float cz = std::cos(rz * 0.5f), sz = std::sin(rz * 0.5f);
    return {
        sx * cy * cz + cx * sy * sz,
        cx * sy * cz - sx * cy * sz,
        cx * cy * sz + sx * sy * cz,
        cx * cy * cz - sx * sy * sz
    };
}

inline float point_segment_dist2(Vec3 p, Vec3 a, Vec3 b) {
    Vec3 ab = b - a;
    float denom = ab.dot(ab);
    if (denom < 1e-8f) return (p - a).dot(p - a);
    float t = std::clamp((p - a).dot(ab) / denom, 0.0f, 1.0f);
    Vec3 proj = a + ab * t;
    Vec3 d = p - proj;
    return d.dot(d);
}

inline std::vector<RigBone> build_skeleton_hierarchy(const Scene3D& scene, const std::string& rig_type) {
    auto [bmin, bmax] = scene.bounds();
    Vec3 sz = bmax - bmin;
    float cx = 0.5f * (bmin.x + bmax.x);
    float cz = 0.5f * (bmin.z + bmax.z);
    float y0 = bmin.y;
    float H  = std::max(0.25f, sz.y);
    float W  = std::max(0.20f, sz.x * 0.5f);
    float L  = std::max(0.20f, sz.z * 0.5f);

    std::vector<RigBone> bones;
    auto add = [&](const std::string& name, int parent, Vec3 head, Vec3 tail) -> int {
        RigBone b;
        b.name = name;
        b.parent = parent;
        b.head_world = head;
        b.tail_world = tail;
        b.local_pos = (parent >= 0) ? (head - bones[parent].head_world) : head;
        bones.push_back(b);
        return static_cast<int>(bones.size()) - 1;
    };

    if (rig_type == "vehicle") {
        int root = add("ChassisRoot", -1, {cx, y0 + H * 0.35f, cz}, {cx, y0 + H * 0.55f, cz});
        add("BodySuspension", root, {cx, y0 + H * 0.48f, cz}, {cx, y0 + H * 0.90f, cz});
        add("Wheel_FL", root, {cx - W * 0.78f, y0 + H * 0.22f, cz + L * 0.62f}, {cx - W * 0.95f, y0 + H * 0.22f, cz + L * 0.62f});
        add("Wheel_FR", root, {cx + W * 0.78f, y0 + H * 0.22f, cz + L * 0.62f}, {cx + W * 0.95f, y0 + H * 0.22f, cz + L * 0.62f});
        add("Wheel_RL", root, {cx - W * 0.78f, y0 + H * 0.22f, cz - L * 0.62f}, {cx - W * 0.95f, y0 + H * 0.22f, cz - L * 0.62f});
        add("Wheel_RR", root, {cx + W * 0.78f, y0 + H * 0.22f, cz - L * 0.62f}, {cx + W * 0.95f, y0 + H * 0.22f, cz - L * 0.62f});
    } else if (rig_type == "quadruped") {
        int hips  = add("Hips",   -1,   {cx, y0 + H * 0.58f, cz - L * 0.42f}, {cx, y0 + H * 0.60f, cz});
        int chest = add("Chest",  hips, {cx, y0 + H * 0.62f, cz + L * 0.35f}, {cx, y0 + H * 0.66f, cz + L * 0.55f});
        int neck  = add("Neck",   chest,{cx, y0 + H * 0.70f, cz + L * 0.55f}, {cx, y0 + H * 0.84f, cz + L * 0.72f});
        add("Head", neck, {cx, y0 + H * 0.84f, cz + L * 0.72f}, {cx, y0 + H * 0.88f, cz + L * 0.95f});
        int tail1 = add("Tail_01", hips, {cx, y0 + H * 0.58f, cz - L * 0.50f}, {cx, y0 + H * 0.52f, cz - L * 0.82f});
        add("Tail_02", tail1, {cx, y0 + H * 0.52f, cz - L * 0.82f}, {cx, y0 + H * 0.44f, cz - L * 1.05f});

        int fl_u = add("FrontLeg_L_Upper", chest, {cx - W * 0.45f, y0 + H * 0.54f, cz + L * 0.36f}, {cx - W * 0.45f, y0 + H * 0.28f, cz + L * 0.36f});
        add("FrontLeg_L_Lower", fl_u, {cx - W * 0.45f, y0 + H * 0.28f, cz + L * 0.36f}, {cx - W * 0.45f, y0 + H * 0.03f, cz + L * 0.40f});
        int fr_u = add("FrontLeg_R_Upper", chest, {cx + W * 0.45f, y0 + H * 0.54f, cz + L * 0.36f}, {cx + W * 0.45f, y0 + H * 0.28f, cz + L * 0.36f});
        add("FrontLeg_R_Lower", fr_u, {cx + W * 0.45f, y0 + H * 0.28f, cz + L * 0.36f}, {cx + W * 0.45f, y0 + H * 0.03f, cz + L * 0.40f});

        int bl_u = add("BackLeg_L_Upper", hips, {cx - W * 0.45f, y0 + H * 0.54f, cz - L * 0.42f}, {cx - W * 0.45f, y0 + H * 0.28f, cz - L * 0.46f});
        add("BackLeg_L_Lower", bl_u, {cx - W * 0.45f, y0 + H * 0.28f, cz - L * 0.46f}, {cx - W * 0.45f, y0 + H * 0.03f, cz - L * 0.42f});
        int br_u = add("BackLeg_R_Upper", hips, {cx + W * 0.45f, y0 + H * 0.54f, cz - L * 0.42f}, {cx + W * 0.45f, y0 + H * 0.28f, cz - L * 0.46f});
        add("BackLeg_R_Lower", br_u, {cx + W * 0.45f, y0 + H * 0.28f, cz - L * 0.46f}, {cx + W * 0.45f, y0 + H * 0.03f, cz - L * 0.42f});
    } else {
        // Biped (17 bones) + optional Winged (+4 wing bones)
        int pelvis = add("Hips",    -1,     {cx, y0 + H * 0.51f, cz}, {cx, y0 + H * 0.59f, cz});
        int spine  = add("Spine",   pelvis, {cx, y0 + H * 0.59f, cz}, {cx, y0 + H * 0.70f, cz});
        int chest  = add("Chest",   spine,  {cx, y0 + H * 0.70f, cz}, {cx, y0 + H * 0.81f, cz});
        int neck   = add("Neck",    chest,  {cx, y0 + H * 0.82f, cz}, {cx, y0 + H * 0.87f, cz});
        add("Head", neck, {cx, y0 + H * 0.87f, cz}, {cx, y0 + H * 0.99f, cz});

        int ua_l = add("UpperArm_L", chest, {cx - W * 0.28f, y0 + H * 0.79f, cz}, {cx - W * 0.62f, y0 + H * 0.64f, cz});
        int fa_l = add("ForeArm_L",  ua_l,  {cx - W * 0.62f, y0 + H * 0.64f, cz}, {cx - W * 0.88f, y0 + H * 0.49f, cz + L * 0.06f});
        add("Hand_L", fa_l, {cx - W * 0.88f, y0 + H * 0.49f, cz + L * 0.06f}, {cx - W * 0.98f, y0 + H * 0.43f, cz + L * 0.10f});

        int ua_r = add("UpperArm_R", chest, {cx + W * 0.28f, y0 + H * 0.79f, cz}, {cx + W * 0.62f, y0 + H * 0.64f, cz});
        int fa_r = add("ForeArm_R",  ua_r,  {cx + W * 0.62f, y0 + H * 0.64f, cz}, {cx + W * 0.88f, y0 + H * 0.49f, cz + L * 0.06f});
        add("Hand_R", fa_r, {cx + W * 0.88f, y0 + H * 0.49f, cz + L * 0.06f}, {cx + W * 0.98f, y0 + H * 0.43f, cz + L * 0.10f});

        int th_l = add("Thigh_L", pelvis, {cx - W * 0.19f, y0 + H * 0.49f, cz}, {cx - W * 0.21f, y0 + H * 0.26f, cz + L * 0.02f});
        int cf_l = add("Calf_L",  th_l,   {cx - W * 0.21f, y0 + H * 0.26f, cz + L * 0.02f}, {cx - W * 0.22f, y0 + H * 0.05f, cz});
        add("Foot_L", cf_l, {cx - W * 0.22f, y0 + H * 0.05f, cz}, {cx - W * 0.22f, y0 + H * 0.02f, cz + L * 0.20f});

        int th_r = add("Thigh_R", pelvis, {cx + W * 0.19f, y0 + H * 0.49f, cz}, {cx + W * 0.21f, y0 + H * 0.26f, cz + L * 0.02f});
        int cf_r = add("Calf_R",  th_r,   {cx + W * 0.21f, y0 + H * 0.26f, cz + L * 0.02f}, {cx + W * 0.22f, y0 + H * 0.05f, cz});
        add("Foot_R", cf_r, {cx + W * 0.22f, y0 + H * 0.05f, cz}, {cx + W * 0.22f, y0 + H * 0.02f, cz + L * 0.20f});

        if (rig_type == "winged") {
            int wl1 = add("WingInner_L", chest, {cx - W * 0.16f, y0 + H * 0.76f, cz - L * 0.18f}, {cx - W * 0.65f, y0 + H * 0.92f, cz - L * 0.36f});
            add("WingOuter_L", wl1, {cx - W * 0.65f, y0 + H * 0.92f, cz - L * 0.36f}, {cx - W * 0.98f, y0 + H * 0.62f, cz - L * 0.42f});
            int wr1 = add("WingInner_R", chest, {cx + W * 0.16f, y0 + H * 0.76f, cz - L * 0.18f}, {cx + W * 0.65f, y0 + H * 0.92f, cz - L * 0.36f});
            add("WingOuter_R", wr1, {cx + W * 0.65f, y0 + H * 0.92f, cz - L * 0.36f}, {cx + W * 0.98f, y0 + H * 0.62f, cz - L * 0.42f});
        }
    }
    return bones;
}

// Evaluates procedural Euler pose (rx, ry, rz) + root bob for any bone & clip at normalized phase u in [0, 1]
inline void sample_bone_pose(
    const std::string& clip_id,
    const std::string& bone_name,
    float u01,
    float H,
    Vec3& out_euler,
    Vec3& out_root_offset
) {
    const float PI2 = 6.283185307f;
    float ph = u01 * PI2;
    float s = std::sin(ph), c = std::cos(ph), s2 = std::sin(2.0f * ph);
    out_euler = {0.0f, 0.0f, 0.0f};
    out_root_offset = {0.0f, 0.0f, 0.0f};

    bool is_l = bone_name.find("_L") != std::string::npos;
    bool is_r = bone_name.find("_R") != std::string::npos;
    float side = is_l ? -1.0f : (is_r ? 1.0f : 0.0f);
    float opp_s = is_l ? s : -s;

    if (clip_id == "idle_breathe") {
        if (bone_name == "Hips") out_root_offset.y = 0.008f * H * s;
        else if (bone_name == "Chest" || bone_name == "Spine") out_euler.x = 0.045f * s;
        else if (bone_name == "Head") { out_euler.y = 0.06f * s; out_euler.x = -0.03f * c; }
        else if (bone_name.find("UpperArm") != std::string::npos) out_euler.z = side * (0.04f * s);
    } else if (clip_id == "walk_standard" || clip_id == "crouch_stealth" || clip_id == "zombie_shamble") {
        float stride = (clip_id == "crouch_stealth") ? 0.42f : (clip_id == "zombie_shamble" ? 0.32f : 0.55f);
        float crouch = (clip_id == "crouch_stealth") ? 0.28f : 0.0f;
        if (bone_name == "Hips") {
            out_root_offset.y = (-0.06f * crouch + 0.022f * std::fabs(s)) * H;
            out_euler.y = 0.08f * s;
            out_euler.x = crouch * 0.5f + (clip_id == "zombie_shamble" ? 0.25f : 0.04f);
        } else if (bone_name == "Spine" || bone_name == "Chest") {
            out_euler.y = -0.07f * s;
            out_euler.x = (clip_id == "zombie_shamble") ? 0.22f + 0.06f * s2 : 0.05f;
        } else if (bone_name.find("Thigh") != std::string::npos) {
            out_euler.x = -opp_s * stride - crouch;
        } else if (bone_name.find("Calf") != std::string::npos) {
            out_euler.x = std::max(0.0f, opp_s) * stride * 0.95f + crouch * 1.2f;
        } else if (bone_name.find("UpperArm") != std::string::npos) {
            out_euler.x = (clip_id == "zombie_shamble") ? (-1.15f + 0.10f * opp_s) : (opp_s * stride * 0.65f);
        } else if (bone_name.find("ForeArm") != std::string::npos) {
            out_euler.x = -0.35f - std::max(0.0f, -opp_s) * 0.25f;
        }
    } else if (clip_id == "run_sprint") {
        if (bone_name == "Hips") { out_root_offset.y = 0.045f * H * std::sin(2.0f * ph); out_euler.x = 0.18f; out_euler.y = 0.14f * s; }
        else if (bone_name == "Chest") { out_euler.x = 0.14f; out_euler.y = -0.14f * s; }
        else if (bone_name.find("Thigh") != std::string::npos) out_euler.x = -opp_s * 0.95f;
        else if (bone_name.find("Calf") != std::string::npos) out_euler.x = 0.25f + std::max(0.0f, opp_s) * 1.15f;
        else if (bone_name.find("UpperArm") != std::string::npos) out_euler.x = opp_s * 0.90f;
        else if (bone_name.find("ForeArm") != std::string::npos) out_euler.x = -1.10f;
    } else if (clip_id == "jump_heroic") {
        float arc = std::max(0.0f, std::sin(u01 * 3.14159265f));
        if (bone_name == "Hips") out_root_offset.y = 0.32f * H * arc;
        else if (bone_name.find("Thigh") != std::string::npos) out_euler.x = -0.55f * arc;
        else if (bone_name.find("Calf") != std::string::npos) out_euler.x = 0.85f * arc;
        else if (bone_name.find("UpperArm") != std::string::npos) { out_euler.x = -0.95f * arc; out_euler.z = side * 0.35f * arc; }
    } else if (clip_id == "sword_slash" || clip_id == "heavy_smash" || clip_id == "cast_arcane" || clip_id == "aim_recoil") {
        float swing = std::sin(ph);
        if (bone_name == "Spine" || bone_name == "Chest") {
            out_euler.y = (clip_id == "sword_slash") ? 0.45f * swing : 0.12f * swing;
            out_euler.x = (clip_id == "heavy_smash") ? 0.35f * swing : 0.08f * s2;
        } else if (bone_name == "UpperArm_R") {
            out_euler.x = (clip_id == "aim_recoil") ? (-1.25f + 0.18f * std::fabs(s2)) : (-1.10f + 0.95f * swing);
            out_euler.y = (clip_id == "sword_slash") ? -0.65f * swing : 0.0f;
        } else if (bone_name == "ForeArm_R") {
            out_euler.x = -0.55f + 0.35f * swing;
        } else if (bone_name == "UpperArm_L") {
            out_euler.x = (clip_id == "cast_arcane" || clip_id == "heavy_smash") ? (-1.10f + 0.85f * swing) : -0.35f;
        }
    } else if (clip_id == "spin_whirlwind") {
        if (bone_name == "Hips") out_euler.y = ph;
        else if (bone_name.find("UpperArm") != std::string::npos) out_euler.z = side * 0.65f;
    } else if (clip_id == "wave_hello" || clip_id == "victory_cheer" || clip_id == "dance_groove") {
        if (bone_name == "Hips" && clip_id == "dance_groove") {
            out_root_offset.y = 0.03f * H * std::fabs(s2);
            out_euler.z = 0.14f * s;
            out_euler.y = 0.22f * c;
        } else if (bone_name == "UpperArm_R") {
            out_euler.z = 1.25f;
            out_euler.x = -0.35f + 0.25f * s2;
        } else if (bone_name == "ForeArm_R") {
            out_euler.z = 0.45f * std::sin(2.0f * ph);
        } else if (bone_name == "UpperArm_L" && clip_id != "wave_hello") {
            out_euler.z = -1.15f + 0.25f * s;
        }
    } else if (clip_id == "quad_trot" || clip_id == "quad_gallop" || clip_id == "quad_pounce") {
        float amp = (clip_id == "quad_gallop") ? 0.78f : 0.50f;
        if (bone_name == "Hips") out_root_offset.y = 0.035f * H * std::fabs(s2);
        else if (bone_name == "FrontLeg_L_Upper" || bone_name == "BackLeg_R_Upper") out_euler.x = s * amp;
        else if (bone_name == "FrontLeg_R_Upper" || bone_name == "BackLeg_L_Upper") out_euler.x = -s * amp;
        else if (bone_name.find("Lower") != std::string::npos) out_euler.x = 0.35f * std::fabs(s);
        else if (bone_name.find("Tail") != std::string::npos) out_euler.y = 0.32f * s;
    } else if (clip_id == "wing_flap_soar") {
        if (bone_name == "Hips") out_root_offset.y = 0.08f * H * (-c);
        else if (bone_name.find("WingInner") != std::string::npos) out_euler.z = side * (0.72f * s);
        else if (bone_name.find("WingOuter") != std::string::npos) out_euler.z = side * (0.55f * std::sin(ph - 0.6f));
        else if (bone_name.find("Thigh") != std::string::npos) out_euler.x = 0.22f + 0.08f * s;
    } else if (clip_id == "vehicle_cruise") {
        if (bone_name.find("Wheel_") != std::string::npos) out_euler.x = ph;
        else if (bone_name == "BodySuspension") {
            out_root_offset.y = 0.012f * H * s2;
            out_euler.x = 0.018f * s;
            out_euler.z = 0.015f * c;
        }
    } else {
        // Generic combat/emote fallback (martial_kick, shield_guard, dodge_roll, hit_stagger, knockdown_ko)
        if (clip_id == "martial_kick" && bone_name == "Thigh_R") out_euler.x = -1.35f * std::max(0.0f, s);
        else if (clip_id == "shield_guard" && bone_name == "UpperArm_L") { out_euler.x = -0.95f; out_euler.y = 0.55f; }
        else if (clip_id == "dodge_roll" && bone_name == "Hips") { out_root_offset.x = 0.25f * H * s; out_euler.z = -0.25f * s; }
        else if (clip_id == "hit_stagger" && (bone_name == "Chest" || bone_name == "Head")) out_euler.x = -0.38f * std::fabs(s);
        else if (clip_id == "knockdown_ko" && bone_name == "Hips") {
            float kd = std::clamp(u01 * 1.4f, 0.0f, 1.0f);
            out_euler.x = -1.48f * kd;
            out_root_offset.y = -0.42f * H * kd;
        }
    }
}

} // namespace rig_detail

inline RiggedSkinData compute_volumetric_skin_weights(const Scene3D& scene, const std::string& requested_rig_type = "auto") {
    RiggedSkinData out;
    std::string rt = requested_rig_type;
    if (rt.empty() || rt == "auto") {
        auto [bmin, bmax] = scene.bounds();
        Vec3 sz = bmax - bmin;
        bool has_wheel = false;
        for (const auto& m : scene.parts) {
            if (m.id.find("wheel") != std::string::npos || m.id.find("tire") != std::string::npos) has_wheel = true;
        }
        if (has_wheel) rt = "vehicle";
        else if (sz.z > sz.y * 1.25f) rt = "quadruped";
        else rt = "biped";
    }
    out.rig_type = rt;
    out.bones = rig_detail::build_skeleton_hierarchy(scene, rt);

    auto [bmin, bmax] = scene.bounds();
    float diag = std::max(0.25f, (bmax - bmin).length());
    float sigma = diag * 0.14f;
    float inv_two_sigma2 = 1.0f / std::max(1e-6f, 2.0f * sigma * sigma);

    out.part_joints.resize(scene.parts.size());
    out.part_weights.resize(scene.parts.size());

    for (size_t pi = 0; pi < scene.parts.size(); ++pi) {
        const auto& m = scene.parts[pi];
        size_t nv = m.positions.size();
        out.part_joints[pi].resize(nv, {0, 0, 0, 0});
        out.part_weights[pi].resize(nv, {1.0f, 0.0f, 0.0f, 0.0f});

        // Vehicle wheel rigid binding when part ID matches a wheel corner
        int rigid_bone = -1;
        if (rt == "vehicle") {
            std::string lid = m.id;
            std::transform(lid.begin(), lid.end(), lid.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (lid.find("wheel") != std::string::npos || lid.find("tire") != std::string::npos || lid.find("rim") != std::string::npos) {
                Vec3 mc{0, 0, 0};
                for (const auto& p : m.positions) mc = mc + p;
                if (!m.positions.empty()) mc = mc * (1.0f / static_cast<float>(m.positions.size()));
                float best_d2 = 1e30f;
                for (size_t bi = 2; bi < out.bones.size(); ++bi) {
                    float d2 = (mc - out.bones[bi].head_world).dot(mc - out.bones[bi].head_world);
                    if (d2 < best_d2) { best_d2 = d2; rigid_bone = static_cast<int>(bi); }
                }
            } else {
                rigid_bone = 1; // BodySuspension
            }
        }

        for (size_t vi = 0; vi < nv; ++vi) {
            if (rigid_bone >= 0) {
                out.part_joints[pi][vi]  = {static_cast<uint16_t>(rigid_bone), 0, 0, 0};
                out.part_weights[pi][vi] = {1.0f, 0.0f, 0.0f, 0.0f};
                continue;
            }
            const Vec3& p = m.positions[vi];
            std::array<std::pair<float, uint16_t>, 4> top4 = {{{0.0f, 0}, {0.0f, 0}, {0.0f, 0}, {0.0f, 0}}};
            for (size_t bi = 0; bi < out.bones.size(); ++bi) {
                float d2 = rig_detail::point_segment_dist2(p, out.bones[bi].head_world, out.bones[bi].tail_world);
                float w = std::exp(-d2 * inv_two_sigma2) + 1.0f / (1.0f + d2 * 60.0f) * 0.15f;
                if (w > top4[3].first) {
                    top4[3] = {w, static_cast<uint16_t>(bi)};
                    for (int k = 3; k > 0 && top4[k].first > top4[k - 1].first; --k) {
                        std::swap(top4[k], top4[k - 1]);
                    }
                }
            }
            float wsum = top4[0].first + top4[1].first + top4[2].first + top4[3].first;
            if (wsum < 1e-8f) {
                out.part_joints[pi][vi]  = {0, 0, 0, 0};
                out.part_weights[pi][vi] = {1.0f, 0.0f, 0.0f, 0.0f};
            } else {
                out.part_joints[pi][vi] = {top4[0].second, top4[1].second, top4[2].second, top4[3].second};
                out.part_weights[pi][vi] = {
                    top4[0].first / wsum,
                    top4[1].first / wsum,
                    top4[2].first / wsum,
                    top4[3].first / wsum
                };
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Exports a complete Khronos glTF 2.0 Skinned & Multi-Clip Animated `.GLB`!
// Plays out-of-the-box in Three.js AnimationMixer, Blender, Unity, UE5 & Godot!
// ---------------------------------------------------------------------------
inline std::vector<uint8_t> export_rigged_animated_glb_bytes(
    const Scene3D& scene,
    const std::string& requested_rig_type = "auto",
    const std::string& primary_clip_id = "walk_standard",
    bool embed_full_library = true
) {
    RiggedSkinData rig = compute_volumetric_skin_weights(scene, requested_rig_type);
    const size_t nbones = rig.bones.size();

    std::vector<uint8_t> bin_buf;
    JsonValue buffer_views = JsonValue::array();
    JsonValue accessors    = JsonValue::array();
    JsonValue materials    = JsonValue::array();
    JsonValue meshes       = JsonValue::array();
    JsonValue nodes        = JsonValue::array();
    JsonValue images       = JsonValue::array();
    JsonValue samplers     = JsonValue::array();
    JsonValue textures     = JsonValue::array();

    auto pad4 = [&]() {
        while (bin_buf.size() % 4 != 0) bin_buf.push_back(0);
    };

    auto push_accessor = [&](const void* raw, size_t byte_len, int target, int comp_type, size_t count,
                             const std::string& type_str, const JsonValue* min_v = nullptr, const JsonValue* max_v = nullptr) -> int {
        pad4();
        size_t offset = bin_buf.size();
        const uint8_t* ptr = static_cast<const uint8_t*>(raw);
        bin_buf.insert(bin_buf.end(), ptr, ptr + byte_len);

        int bv_idx = static_cast<int>(buffer_views.size());
        JsonValue bv = JsonValue::object({{"buffer", 0}, {"byteOffset", static_cast<int64_t>(offset)}, {"byteLength", static_cast<int64_t>(byte_len)}});
        if (target != 0) bv["target"] = target;
        buffer_views.push_back(bv);

        int acc_idx = static_cast<int>(accessors.size());
        JsonValue acc = JsonValue::object({{"bufferView", bv_idx}, {"componentType", comp_type}, {"count", static_cast<int64_t>(count)}, {"type", type_str}});
        if (comp_type == 5121) acc["normalized"] = true;
        if (min_v) acc["min"] = *min_v;
        if (max_v) acc["max"] = *max_v;
        accessors.push_back(acc);
        return acc_idx;
    };

    auto embed_png_tex = [&](const std::vector<uint8_t>& png) -> int {
        if (png.empty()) return -1;
        pad4();
        size_t offset = bin_buf.size();
        bin_buf.insert(bin_buf.end(), png.begin(), png.end());
        int bv_idx = static_cast<int>(buffer_views.size());
        buffer_views.push_back(JsonValue::object({{"buffer", 0}, {"byteOffset", static_cast<int64_t>(offset)}, {"byteLength", static_cast<int64_t>(png.size())}}));
        if (samplers.size() == 0) {
            samplers.push_back(JsonValue::object({{"magFilter", 9729}, {"minFilter", 9987}, {"wrapS", 10497}, {"wrapT", 10497}}));
        }
        int img_idx = static_cast<int>(images.size());
        images.push_back(JsonValue::object({{"bufferView", bv_idx}, {"mimeType", "image/png"}}));
        int tex_idx = static_cast<int>(textures.size());
        textures.push_back(JsonValue::object({{"sampler", 0}, {"source", img_idx}}));
        return tex_idx;
    };

    // Node 0 = Armature Root, Nodes 1..nbones = Skeleton Joints
    int armature_node_idx = 0;
    nodes.push_back(JsonValue::object({{"name", "FalconArmature"}}));

    std::vector<std::vector<int>> bone_children(nbones);
    for (size_t bi = 0; bi < nbones; ++bi) {
        if (rig.bones[bi].parent >= 0) bone_children[rig.bones[bi].parent].push_back(static_cast<int>(bi));
    }

    JsonValue joint_node_indices = JsonValue::array();
    for (size_t bi = 0; bi < nbones; ++bi) {
        int node_id = static_cast<int>(nodes.size());
        joint_node_indices.push_back(node_id);
        const auto& b = rig.bones[bi];
        JsonValue jnode = JsonValue::object({
            {"name", b.name},
            {"translation", JsonValue::array({b.local_pos.x, b.local_pos.y, b.local_pos.z})},
            {"rotation", JsonValue::array({0.0, 0.0, 0.0, 1.0})}
        });
        if (!bone_children[bi].empty()) {
            JsonValue ch = JsonValue::array();
            for (int cbi : bone_children[bi]) ch.push_back(1 + cbi);
            jnode["children"] = ch;
        }
        nodes.push_back(jnode);
    }

    // Inverse Bind Matrices (4x4 column-major per bone: T(-head_world))
    std::vector<float> ibm_data(nbones * 16, 0.0f);
    for (size_t bi = 0; bi < nbones; ++bi) {
        float* m = &ibm_data[bi * 16];
        m[0] = 1.0f; m[5] = 1.0f; m[10] = 1.0f; m[15] = 1.0f;
        m[12] = -rig.bones[bi].head_world.x;
        m[13] = -rig.bones[bi].head_world.y;
        m[14] = -rig.bones[bi].head_world.z;
    }
    int ibm_acc = push_accessor(ibm_data.data(), ibm_data.size() * sizeof(float), 0, 5126, nbones, "MAT4");

    JsonValue skins = JsonValue::array({
        JsonValue::object({
            {"name", "FalconAutoSkin_" + rig.rig_type},
            {"inverseBindMatrices", ibm_acc},
            {"skeleton", 1},
            {"joints", joint_node_indices}
        })
    });

    // Build Skinned Meshes + Materials
    std::map<std::string, std::array<int, 3>> finish_tex_cache;
    JsonValue armature_children = JsonValue::array({1}); // Root bone node 1

    for (size_t pi = 0; pi < scene.parts.size(); ++pi) {
        const auto& m = scene.parts[pi];
        if (m.positions.empty() || m.faces.empty()) continue;

        Vec3 bmin{1e30f, 1e30f, 1e30f}, bmax{-1e30f, -1e30f, -1e30f};
        for (const auto& p : m.positions) {
            bmin.x = std::min(bmin.x, p.x); bmin.y = std::min(bmin.y, p.y); bmin.z = std::min(bmin.z, p.z);
            bmax.x = std::max(bmax.x, p.x); bmax.y = std::max(bmax.y, p.y); bmax.z = std::max(bmax.z, p.z);
        }
        JsonValue jmin = JsonValue::array({bmin.x, bmin.y, bmin.z});
        JsonValue jmax = JsonValue::array({bmax.x, bmax.y, bmax.z});

        int pos_acc = push_accessor(m.positions.data(), m.positions.size() * sizeof(Vec3), 34962, 5126, m.positions.size(), "VEC3", &jmin, &jmax);
        int nrm_acc = push_accessor(m.normals.data(), m.normals.size() * sizeof(Vec3), 34962, 5126, m.normals.size(), "VEC3");
        int jnt_acc = push_accessor(rig.part_joints[pi].data(), rig.part_joints[pi].size() * sizeof(uint16_t) * 4, 34962, 5123, m.positions.size(), "VEC4");
        int wgt_acc = push_accessor(rig.part_weights[pi].data(), rig.part_weights[pi].size() * sizeof(float) * 4, 34962, 5126, m.positions.size(), "VEC4");

        auto uvs = compute_triplanar_uvs(m);
        int uv_acc = push_accessor(uvs.data(), uvs.size() * sizeof(float), 34962, 5126, m.positions.size(), "VEC2");

        int col_acc = -1;
        if (m.has_vertex_colors()) {
            col_acc = push_accessor(m.colors.data(), m.colors.size() * sizeof(ColorRGBA), 34962, 5121, m.colors.size(), "VEC4");
        } else {
            auto ao = compute_baked_ao_colors(m);
            col_acc = push_accessor(ao.data(), ao.size() * sizeof(ColorRGBA), 34962, 5121, ao.size(), "VEC4");
        }

        std::vector<uint32_t> flat_idx;
        flat_idx.reserve(m.faces.size() * 3);
        for (const auto& f : m.faces) { flat_idx.push_back(f[0]); flat_idx.push_back(f[1]); flat_idx.push_back(f[2]); }
        int idx_acc = push_accessor(flat_idx.data(), flat_idx.size() * sizeof(uint32_t), 34963, 5125, flat_idx.size(), "SCALAR");

        std::string fin = infer_surface_finish(m);
        int tres = std::clamp(m.material.tex_resolution > 0 ? m.material.tex_resolution : 256, 128, 4096);
        JsonValue pbr = JsonValue::object();
        JsonValue mat = JsonValue::object({{"name", m.material.name}, {"doubleSided", true}});

        if (m.has_vertex_colors()) {
            pbr["baseColorFactor"] = JsonValue::array({1.0, 1.0, 1.0, 1.0});
            pbr["metallicFactor"] = m.material.metallic;
            pbr["roughnessFactor"] = m.material.roughness;
        } else {
            auto c4 = ColorRGBA::from_hex(m.material.color_hex).to_float4();
            pbr["baseColorFactor"] = JsonValue::array({c4[0], c4[1], c4[2], 1.0});
            pbr["metallicFactor"] = 1.0;
            pbr["roughnessFactor"] = 1.0;
            std::string fkey = fin + "_" + std::to_string(tres);
            auto cit = finish_tex_cache.find(fkey);
            if (cit == finish_tex_cache.end()) {
                const auto& b = get_cached_pbr_textures(fin, m.material.metallic, m.material.roughness, tres);
                cit = finish_tex_cache.emplace(fkey, std::array<int, 3>{embed_png_tex(b.albedo_png), embed_png_tex(b.orm_png), embed_png_tex(b.normal_png)}).first;
            }
            if (cit->second[0] >= 0) pbr["baseColorTexture"] = JsonValue::object({{"index", cit->second[0]}});
            if (cit->second[1] >= 0) {
                pbr["metallicRoughnessTexture"] = JsonValue::object({{"index", cit->second[1]}});
                mat["occlusionTexture"] = JsonValue::object({{"index", cit->second[1]}, {"strength", 0.85}});
            }
            if (cit->second[2] >= 0) mat["normalTexture"] = JsonValue::object({{"index", cit->second[2]}, {"scale", 0.42}});
        }
        mat["pbrMetallicRoughness"] = pbr;
        int mat_idx = static_cast<int>(materials.size());
        materials.push_back(mat);

        JsonValue attrs = JsonValue::object({
            {"POSITION", pos_acc},
            {"NORMAL", nrm_acc},
            {"TEXCOORD_0", uv_acc},
            {"JOINTS_0", jnt_acc},
            {"WEIGHTS_0", wgt_acc},
            {"COLOR_0", col_acc}
        });
        int mesh_idx = static_cast<int>(meshes.size());
        meshes.push_back(JsonValue::object({
            {"name", m.name},
            {"primitives", JsonValue::array({JsonValue::object({{"attributes", attrs}, {"indices", idx_acc}, {"material", mat_idx}, {"mode", 4}})})}
        }));

        int snode_idx = static_cast<int>(nodes.size());
        nodes.push_back(JsonValue::object({{"name", m.id}, {"mesh", mesh_idx}, {"skin", 0}}));
        armature_children.push_back(snode_idx);
    }

    nodes[armature_node_idx]["children"] = armature_children;

    // Build Embedded glTF 2.0 Animations!
    // Primary requested clip is ALWAYS index 0 so default auto-play starts on the user's chosen clip!
    const auto& lib = get_motion_library_catalog();
    std::vector<MotionClipInfo> ordered_clips;
    for (const auto& c : lib) {
        if (c.id == primary_clip_id) ordered_clips.push_back(c);
    }
    if (embed_full_library) {
        for (const auto& c : lib) {
            if (c.id != primary_clip_id) ordered_clips.push_back(c);
        }
    } else if (ordered_clips.empty() && !lib.empty()) {
        ordered_clips.push_back(lib[1]);
    }

    auto [smin, smax] = scene.bounds();
    float H = std::max(0.25f, smax.y - smin.y);

    JsonValue animations = JsonValue::array();
    for (const auto& clip : ordered_clips) {
        int kf = std::max(5, clip.keyframes);
        std::vector<float> times(kf);
        for (int k = 0; k < kf; ++k) times[k] = (static_cast<float>(k) / static_cast<float>(kf - 1)) * clip.duration_sec;
        JsonValue tmin = JsonValue::array({0.0f});
        JsonValue tmax = JsonValue::array({clip.duration_sec});
        int time_acc = push_accessor(times.data(), times.size() * sizeof(float), 0, 5126, kf, "SCALAR", &tmin, &tmax);

        JsonValue anim_samplers = JsonValue::array();
        JsonValue anim_channels = JsonValue::array();

        for (size_t bi = 0; bi < nbones; ++bi) {
            int target_node = 1 + static_cast<int>(bi);
            std::vector<float> quats(static_cast<size_t>(kf) * 4);
            std::vector<float> trans(static_cast<size_t>(kf) * 3);
            bool has_trans = (bi == 0 || rig.bones[bi].name == "BodySuspension");

            for (int k = 0; k < kf; ++k) {
                float u01 = static_cast<float>(k) / static_cast<float>(kf - 1);
                Vec3 eul, roff;
                rig_detail::sample_bone_pose(clip.id, rig.bones[bi].name, u01, H, eul, roff);
                auto q = rig_detail::euler_xyz_to_quat(eul.x, eul.y, eul.z);
                quats[k * 4 + 0] = q[0]; quats[k * 4 + 1] = q[1]; quats[k * 4 + 2] = q[2]; quats[k * 4 + 3] = q[3];
                if (has_trans) {
                    trans[k * 3 + 0] = rig.bones[bi].local_pos.x + roff.x;
                    trans[k * 3 + 1] = rig.bones[bi].local_pos.y + roff.y;
                    trans[k * 3 + 2] = rig.bones[bi].local_pos.z + roff.z;
                }
            }

            int rot_acc = push_accessor(quats.data(), quats.size() * sizeof(float), 0, 5126, kf, "VEC4");
            int rot_smp = static_cast<int>(anim_samplers.size());
            anim_samplers.push_back(JsonValue::object({{"input", time_acc}, {"interpolation", "LINEAR"}, {"output", rot_acc}}));
            anim_channels.push_back(JsonValue::object({
                {"sampler", rot_smp},
                {"target", JsonValue::object({{"node", target_node}, {"path", "rotation"}})}
            }));

            if (has_trans) {
                int tr_acc = push_accessor(trans.data(), trans.size() * sizeof(float), 0, 5126, kf, "VEC3");
                int tr_smp = static_cast<int>(anim_samplers.size());
                anim_samplers.push_back(JsonValue::object({{"input", time_acc}, {"interpolation", "LINEAR"}, {"output", tr_acc}}));
                anim_channels.push_back(JsonValue::object({
                    {"sampler", tr_smp},
                    {"target", JsonValue::object({{"node", target_node}, {"path", "translation"}})}
                }));
            }
        }

        animations.push_back(JsonValue::object({
            {"name", clip.id + " | " + clip.name},
            {"samplers", anim_samplers},
            {"channels", anim_channels}
        }));
    }

    pad4();
    JsonValue gltf = JsonValue::object({
        {"asset", JsonValue::object({{"version", "2.0"}, {"generator", "Falcon AI v3.0 Auto-Rig & Motion Engine"}})},
        {"scene", 0},
        {"scenes", JsonValue::array({JsonValue::object({{"nodes", JsonValue::array({0})}})})},
        {"nodes", nodes},
        {"skins", skins},
        {"meshes", meshes},
        {"materials", materials},
        {"animations", animations},
        {"accessors", accessors},
        {"bufferViews", buffer_views},
        {"buffers", JsonValue::array({JsonValue::object({{"byteLength", static_cast<int64_t>(bin_buf.size())}})})}
    });
    if (images.size() > 0) {
        gltf["images"] = images;
        gltf["samplers"] = samplers;
        gltf["textures"] = textures;
    }

    std::string json_str = gltf.dump();
    while (json_str.size() % 4 != 0) json_str.push_back(' ');

    uint32_t total_len = static_cast<uint32_t>(12 + 8 + json_str.size() + 8 + bin_buf.size());
    std::vector<uint8_t> out;
    out.reserve(total_len);
    auto append_u32 = [&](uint32_t v) {
        out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF);
        out.push_back((v >> 16) & 0xFF); out.push_back((v >> 24) & 0xFF);
    };
    append_u32(0x46546C67u);
    append_u32(2u);
    append_u32(total_len);
    append_u32(static_cast<uint32_t>(json_str.size()));
    append_u32(0x4E4F534Au);
    out.insert(out.end(), json_str.begin(), json_str.end());
    append_u32(static_cast<uint32_t>(bin_buf.size()));
    append_u32(0x004E4942u);
    out.insert(out.end(), bin_buf.begin(), bin_buf.end());
    return out;
}

} // namespace openmesh
