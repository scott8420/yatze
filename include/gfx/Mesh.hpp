#pragma once
// gfx::Mesh -- geometry made in code (no asset files): a rounded cube for the
// dice. GTK-free and GL-free (plain vertex arrays); DiceView uploads them.
#include "gfx/Math.hpp"

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace yatze::gfx {

struct Vertex { float px, py, pz, nx, ny, nz; };

struct MeshData {
    std::vector<Vertex>        verts;
    std::vector<std::uint32_t> index;
};

// A box with half-extents `half`, centred on the origin, edges rounded to
// `radius` (s009: the tray's rails; the die is the cube case below).
// Each face is an n x n grid, denser toward the edges where the bevel bends;
// every grid point is pushed onto the rounded surface (inner box + radius
// along the normal), so the flats stay flat and the bevels are smooth.
inline MeshData rounded_box(const Vec3& half, float radius, int n) {
    MeshData m;
    const Vec3 in{half.x - radius, half.y - radius, half.z - radius};
    // Grid coordinate -> -1..1, packed toward both ends; scaled per axis below.
    auto coord = [&](int i) {
        const float t = float(i) / float(n) * 2.0f - 1.0f;
        return std::copysign(std::pow(std::fabs(t), 0.6f), t);
    };
    auto scale = [&](const Vec3& v) { return Vec3{v.x * half.x, v.y * half.y, v.z * half.z}; };
    // Face frames: normal axis and the two in-plane axes.
    const Vec3 axes[6][3] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    };
    for (const auto& f : axes) {
        const std::uint32_t base = std::uint32_t(m.verts.size());
        for (int j = 0; j <= n; ++j)
            for (int i = 0; i <= n; ++i) {
                const Vec3 p = scale(f[0] + f[1] * coord(i) + f[2] * coord(j));
                const Vec3 c{std::fmax(-in.x, std::fmin(in.x, p.x)), std::fmax(-in.y, std::fmin(in.y, p.y)),
                             std::fmax(-in.z, std::fmin(in.z, p.z))};
                const Vec3 nrm = normalize(p - c);
                const Vec3 q = c + nrm * radius;
                m.verts.push_back({q.x, q.y, q.z, nrm.x, nrm.y, nrm.z});
            }
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i) {
                const std::uint32_t a = base + std::uint32_t(j * (n + 1) + i);
                const std::uint32_t b = a + 1, c = a + std::uint32_t(n + 1), d = c + 1;
                m.index.insert(m.index.end(), {a, b, d, a, d, c});
            }
    }
    return m;
}

// The dice cup (s010): an open cylinder of radius r, height h, centred on the
// origin, axis +Y (base at -h/2, mouth at +h/2). One thin shell -- the side
// and the base -- plus a rolled rim (a small torus) at the mouth and a rounded
// heel at the base. Drawn two-sided (the shader turns each normal toward the
// eye), so the inside shows when it tips toward you.
inline MeshData cup_mesh(float r, float h, int n = 48) {
    MeshData m;
    struct PN { Vec3 p, n; };                  // a point and its normal (not std::pair: GCC ABI notes)
    constexpr float kTau = 6.2831853f;
    const float lip = 0.07f, heel = 0.12f;
    auto grid = [&](int rows, auto at) {       // at(i, j) -> {pos, normal}; i around, j along
        const std::uint32_t base = std::uint32_t(m.verts.size());
        for (int j = 0; j <= rows; ++j)
            for (int i = 0; i <= n; ++i) {
                const auto [p, q] = at(i, j);
                m.verts.push_back({p.x, p.y, p.z, q.x, q.y, q.z});
            }
        for (int j = 0; j < rows; ++j)
            for (int i = 0; i < n; ++i) {
                const std::uint32_t a = base + std::uint32_t(j * (n + 1) + i);
                const std::uint32_t b = a + 1, c = a + std::uint32_t(n + 1), d = c + 1;
                m.index.insert(m.index.end(), {a, b, d, a, d, c});
            }
    };
    auto ring = [&](int i) { const float t = float(i) / float(n) * kTau; return Vec3{std::cos(t), 0, std::sin(t)}; };
    // The side, from the heel up to the rim.
    grid(8, [&](int i, int j) {
        const Vec3 d = ring(i);
        const float y = -h / 2 + heel + (h - heel - lip) * float(j) / 8.0f;
        return PN{Vec3{d.x * r, y, d.z * r}, d};
    });
    // The heel: a quarter round from the side into the base.
    grid(6, [&](int i, int j) {
        const Vec3 d = ring(i);
        const float a = float(j) / 6.0f * (kTau / 4);                 // 0 = side, 90 deg = base
        const Vec3 nrm = d * std::cos(a) + Vec3{0, -std::sin(a), 0};
        const Vec3 c{d.x * (r - heel), -h / 2 + heel, d.z * (r - heel)};
        return PN{c + nrm * heel, nrm};
    });
    // The base.
    grid(4, [&](int i, int j) {
        const Vec3 d = ring(i);
        const float s = (r - heel) * (1.0f - float(j) / 4.0f);
        return PN{Vec3{d.x * s, -h / 2, d.z * s}, Vec3{0, -1, 0}};
    });
    // The rolled rim: a torus round the mouth.
    grid(10, [&](int i, int j) {
        const Vec3 d = ring(i);
        const float a = float(j) / 10.0f * kTau;
        const Vec3 nrm = d * std::cos(a) + Vec3{0, std::sin(a), 0};
        const Vec3 c{d.x * (r - lip * 0.4f), h / 2 - lip, d.z * (r - lip * 0.4f)};
        return PN{c + nrm * lip, nrm};
    });
    return m;
}

// The die: a cube of side 1, edges rounded to `radius`.
inline MeshData rounded_cube(float radius = 0.12f, int n = 18) {
    return rounded_box({0.5f, 0.5f, 0.5f}, radius, n);
}

}  // namespace yatze::gfx
