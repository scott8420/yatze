#pragma once
// gfx::Math -- the little linear algebra the dice table needs: Vec3, Quat,
// Mat4 (column-major, OpenGL order). Header-only, GTK-free, no glm.
#include <array>
#include <cmath>

namespace yatze::gfx {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
};
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3  cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3  normalize(const Vec3& v) { const float l = length(v); return l > 0 ? v * (1.0f / l) : v; }
inline Vec3  lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

struct Quat {
    float w = 1, x = 0, y = 0, z = 0;
    static Quat axis_angle(const Vec3& axis, float rad) {
        const Vec3 a = normalize(axis);
        const float s = std::sin(rad * 0.5f);
        return {std::cos(rad * 0.5f), a.x * s, a.y * s, a.z * s};
    }
    Quat operator*(const Quat& q) const {
        return {w * q.w - x * q.x - y * q.y - z * q.z,
                w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w};
    }
    Vec3 rotate(const Vec3& v) const {
        const Vec3 u{x, y, z};
        const Vec3 t = cross(u, v) * 2.0f;
        return v + t * w + cross(u, t);
    }
};
inline Quat normalize(const Quat& q) {
    const float l = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / l, q.x / l, q.y / l, q.z / l};
}
// Shortest-arc rotation taking unit vector a onto unit vector b.
inline Quat from_to(const Vec3& a, const Vec3& b) {
    const float d = dot(a, b);
    if (d > 0.99999f) return {};
    if (d < -0.99999f) {
        Vec3 axis = cross({1, 0, 0}, a);
        if (length(axis) < 1e-3f) axis = cross({0, 1, 0}, a);
        return Quat::axis_angle(axis, 3.14159265f);
    }
    const Vec3 c = cross(a, b);
    return normalize(Quat{1 + d, c.x, c.y, c.z});
}
inline Quat slerp(Quat a, const Quat& b, float t) {
    float d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (d < 0) { a = {-a.w, -a.x, -a.y, -a.z}; d = -d; }
    if (d > 0.9995f)
        return normalize(Quat{a.w + (b.w - a.w) * t, a.x + (b.x - a.x) * t,
                              a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t});
    const float th = std::acos(d), s = std::sin(th);
    const float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
    return {a.w * wa + b.w * wb, a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb};
}

// Column-major 4x4: m[col * 4 + row].
struct Mat4 {
    std::array<float, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + rr] * o.m[c * 4 + k];
                r.m[c * 4 + rr] = s;
            }
        return r;
    }
    static Mat4 translate(const Vec3& t) { Mat4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
    static Mat4 scale(float s) { Mat4 r; r.m[0] = r.m[5] = r.m[10] = s; return r; }
    static Mat4 rotate(const Quat& q) {
        Mat4 r;
        const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        r.m = {1 - 2 * (yy + zz), 2 * (xy + wz),     2 * (xz - wy),     0,
               2 * (xy - wz),     1 - 2 * (xx + zz), 2 * (yz + wx),     0,
               2 * (xz + wy),     2 * (yz - wx),     1 - 2 * (xx + yy), 0,
               0, 0, 0, 1};
        return r;
    }
    static Mat4 perspective(float fovy_rad, float aspect, float n, float f) {
        Mat4 r;
        const float t = 1.0f / std::tan(fovy_rad / 2);
        r.m = {t / aspect, 0, 0, 0, 0, t, 0, 0, 0, 0, (f + n) / (n - f), -1, 0, 0, 2 * f * n / (n - f), 0};
        return r;
    }
    // Orthographic, symmetric: half-width w, half-height h (s011: the light's view).
    static Mat4 ortho(float w, float h, float n, float f) {
        Mat4 r;
        r.m = {1 / w, 0, 0, 0, 0, 1 / h, 0, 0, 0, 0, -2 / (f - n), 0, 0, 0, -(f + n) / (f - n), 1};
        return r;
    }
    static Mat4 look_at(const Vec3& eye, const Vec3& at, const Vec3& up) {
        const Vec3 f = normalize(at - eye), s = normalize(cross(f, up)), u = cross(s, f);
        Mat4 r;
        r.m = {s.x, u.x, -f.x, 0, s.y, u.y, -f.y, 0, s.z, u.z, -f.z, 0,
               -dot(s, eye), -dot(u, eye), dot(f, eye), 1};
        return r;
    }
};

inline float smoothstep01(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); }
inline float ease_in_out(float t) {   // smootherstep: gentle start and landing
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * t * (t * (t * 6 - 15) + 10);
}

}  // namespace yatze::gfx
