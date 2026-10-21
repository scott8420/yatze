#pragma once
// gfx::Mesh -- geometry made in code (no asset files): a rounded cube for the
// dice. GTK-free and GL-free (plain vertex arrays); DiceView uploads them.
#include "gfx/Math.hpp"

#include <cstdint>
#include <vector>

namespace yatze::gfx {

struct Vertex { float px, py, pz, nx, ny, nz; };

struct MeshData {
    std::vector<Vertex>        verts;
    std::vector<std::uint32_t> index;
};

// A cube of side 1 centred on the origin with edges rounded to `radius`.
// Each face is an n x n grid, denser toward the edges where the bevel bends;
// every grid point is pushed onto the rounded surface (inner box + radius
// along the normal), so the flats stay flat and the bevels are smooth.
inline MeshData rounded_cube(float radius = 0.12f, int n = 18) {
    MeshData m;
    const float h = 0.5f, in = h - radius;
    // Grid coordinate -> position on [-h, h], packed toward both ends.
    auto coord = [&](int i) {
        const float t = float(i) / float(n) * 2.0f - 1.0f;           // -1..1
        const float s = std::copysign(std::pow(std::fabs(t), 0.6f), t);
        return s * h;
    };
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
                const Vec3 p = f[0] * h + f[1] * coord(i) + f[2] * coord(j);
                const Vec3 c{std::fmax(-in, std::fmin(in, p.x)), std::fmax(-in, std::fmin(in, p.y)),
                             std::fmax(-in, std::fmin(in, p.z))};
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

}  // namespace yatze::gfx
