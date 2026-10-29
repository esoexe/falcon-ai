#pragma once
/**
 * OpenMesh Studio — High-Performance C++20 3D Math, Matrix4x4, Bounding Box & Color Kernel.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace openmesh {

constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { float inv = 1.0f / s; return {x * inv, y * inv, z * inv}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    float length_sq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(length_sq()); }
    Vec3 normalized() const {
        float l = length();
        return (l > 1e-8f) ? (*this / l) : Vec3{0.0f, 1.0f, 0.0f};
    }
};

struct ColorRGBA {
    uint8_t r = 156, g = 163, b = 175, a = 255;

    static ColorRGBA from_hex(const std::string& hex, uint8_t alpha = 255) {
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h.erase(0, 1);
        if (h.size() == 3) {
            std::string full;
            for (char c : h) { full.push_back(c); full.push_back(c); }
            h = full;
        }
        if (h.size() != 6) return {156, 163, 175, alpha};
        try {
            unsigned int v = std::stoul(h, nullptr, 16);
            return {
                static_cast<uint8_t>((v >> 16) & 0xFF),
                static_cast<uint8_t>((v >> 8) & 0xFF),
                static_cast<uint8_t>(v & 0xFF),
                alpha
            };
        } catch (...) {
            return {156, 163, 175, alpha};
        }
    }

    std::array<float, 4> to_float4() const {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }
};

struct AABB {
    Vec3 min{1e30f, 1e30f, 1e30f};
    Vec3 max{-1e30f, -1e30f, -1e30f};

    void expand(const Vec3& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    bool valid() const { return min.x <= max.x; }
    Vec3 extents() const { return valid() ? (max - min) : Vec3{1.0f, 1.0f, 1.0f}; }
    Vec3 center() const { return valid() ? ((min + max) * 0.5f) : Vec3{0.0f, 0.0f, 0.0f}; }
};

struct Mat4 {
    float m[4][4] = {
        {1,0,0,0},
        {0,1,0,0},
        {0,0,1,0},
        {0,0,0,1}
    };

    static Mat4 identity() { return Mat4{}; }

    static Mat4 translation(float tx, float ty, float tz) {
        Mat4 r;
        r.m[0][3] = tx; r.m[1][3] = ty; r.m[2][3] = tz;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r;
        r.m[0][0] = sx; r.m[1][1] = sy; r.m[2][2] = sz;
        return r;
    }

    static Mat4 rot_x(float rad) {
        Mat4 r;
        float c = std::cos(rad), s = std::sin(rad);
        r.m[1][1] = c; r.m[1][2] = -s;
        r.m[2][1] = s; r.m[2][2] = c;
        return r;
    }

    static Mat4 rot_y(float rad) {
        Mat4 r;
        float c = std::cos(rad), s = std::sin(rad);
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }

    static Mat4 rot_z(float rad) {
        Mat4 r;
        float c = std::cos(rad), s = std::sin(rad);
        r.m[0][0] = c; r.m[0][1] = -s;
        r.m[1][0] = s; r.m[1][1] = c;
        return r;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 out;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out.m[i][j] = m[i][0] * b.m[0][j] + m[i][1] * b.m[1][j] +
                              m[i][2] * b.m[2][j] + m[i][3] * b.m[3][j];
            }
        }
        return out;
    }

    Vec3 transform_point(const Vec3& p) const {
        return {
            m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
            m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
            m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]
        };
    }

    static Mat4 trs(const Vec3& pos, const Vec3& rot_deg, const Vec3& scl) {
        float rx = rot_deg.x * (kPi / 180.0f);
        float ry = rot_deg.y * (kPi / 180.0f);
        float rz = rot_deg.z * (kPi / 180.0f);
        return translation(pos.x, pos.y, pos.z) * rot_z(rz) * rot_y(ry) * rot_x(rx) * scale(scl.x, scl.y, scl.z);
    }

    float determinant3x3() const {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }
};

} // namespace openmesh
