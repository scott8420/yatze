#pragma once
// gfx::Overlap -- do two dice overlap, how deep, and which way out (s019,
// Scott: "check your collision control ... when dice overlap").
//
// The separating-axis test for two cubes (side 1): 15 candidate axes -- each
// die's 3 face normals and the 9 edge-by-edge cross products. If the shadows
// of the two dice on ANY axis do not overlap, they are apart; otherwise the
// axis with the least overlap is the shortest way out. Exact for boxes (our
// dice are rounded, so a reported edge-to-edge touch can still show a hair of
// air -- the safe side).
//
// Until s019 the physics only tested each die's CORNERS against the other
// (plus a centre-distance bound); an edge crossing an edge, or a face pushed
// into a face, slipped through it, and the dice at rest were spread apart by
// centre distance alone -- two dice turned to different angles could still
// sit inside each other. The probe: 63 of 300 takes ENDED with dice
// overlapping (up to 0.22 deep), nearly all overlapped at some moment in
// flight (up to 0.47).
#include "gfx/Math.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace yatze::gfx {

struct Overlap {
    float depth = -1;   // > 0: inside each other by this much; <= 0: apart
    Vec3  n{1, 0, 0};   // unit, from B toward A: move A along n (or B along -n) to part them
};

inline Overlap overlap(const Vec3& pa, const Quat& qa, const Vec3& pb, const Quat& qb, float half = 0.5f) {
    const Vec3 a[3] = {qa.rotate({1, 0, 0}), qa.rotate({0, 1, 0}), qa.rotate({0, 0, 1})};
    const Vec3 b[3] = {qb.rotate({1, 0, 0}), qb.rotate({0, 1, 0}), qb.rotate({0, 0, 1})};
    const Vec3 d = pa - pb;
    Overlap best{1e9f, {1, 0, 0}};
    auto test = [&](Vec3 L, float bias) {
        const float l = length(L);
        if (l < 1e-3f) return true;                 // parallel edges: no axis here
        L = L * (1.0f / l);
        float ra = 0, rb = 0;
        for (int i = 0; i < 3; ++i) { ra += half * std::fabs(dot(a[i], L)); rb += half * std::fabs(dot(b[i], L)); }
        const float s = dot(d, L);
        const float o = ra + rb - std::fabs(s);
        if (o <= 0) { best = {o, s >= 0 ? L : L * -1.0f}; return false; }
        // Face axes win ties (a tiny bias): a resting stack parts along a face.
        if (o + bias < best.depth) best = {o + bias, s >= 0 ? L : L * -1.0f};
        return true;
    };
    for (int i = 0; i < 3; ++i) if (!test(a[i], 0) || !test(b[i], 0)) return best;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!test(cross(a[i], b[j]), 1e-3f)) return best;
    return best;
}

// Two dice lying flat on the felt: squares turned about the vertical. Only
// their four face directions in the floor matter.
inline Overlap overlap_flat(const Vec3& pa, const Quat& qa, const Vec3& pb, const Quat& qb, float half = 0.5f) {
    auto flat = [](Vec3 v) { v.y = 0; const float l = length(v); return l > 1e-4f ? v * (1.0f / l) : Vec3{1, 0, 0}; };
    // The die's own axes that lie in the floor (the one pointing up is skipped).
    auto floor_axes = [&](const Quat& q, Vec3 out[2]) {
        const Vec3 ax[3] = {q.rotate({1, 0, 0}), q.rotate({0, 1, 0}), q.rotate({0, 0, 1})};
        int up = 0;
        for (int i = 1; i < 3; ++i) if (std::fabs(ax[i].y) > std::fabs(ax[up].y)) up = i;
        int k = 0;
        for (int i = 0; i < 3; ++i) if (i != up) out[k++] = flat(ax[i]);
    };
    Vec3 A[2], B[2];
    floor_axes(qa, A);
    floor_axes(qb, B);
    Vec3 d = pa - pb;
    d.y = 0;
    Overlap best{1e9f, {1, 0, 0}};
    for (const Vec3* set : {A, B})
        for (int i = 0; i < 2; ++i) {
            const Vec3 L = set[i];
            const float ra = half * (std::fabs(dot(A[0], L)) + std::fabs(dot(A[1], L)));
            const float rb = half * (std::fabs(dot(B[0], L)) + std::fabs(dot(B[1], L)));
            const float s = dot(d, L);
            const float o = ra + rb - std::fabs(s);
            if (o < best.depth) best = {o, s >= 0 ? L : L * -1.0f};
            if (o <= 0) return best;
        }
    return best;
}

// Part a set of dice that are being MOVED (not simulated) -- the glide into
// the row: any two inside each other (or closer than `air`) are pushed apart
// along the shortest way out; a fixed die (held) never moves; nothing goes
// below the felt. A few passes, so chains settle.
template <std::size_t N>
inline void part(std::array<Vec3, N>& pos, const std::array<Quat, N>& rot, const std::array<bool, N>& fixed,
                 float air = 0.02f, int passes = 6) {
    for (int pass = 0; pass < passes; ++pass)
        for (std::size_t a = 0; a < N; ++a)
            for (std::size_t b = a + 1; b < N; ++b) {
                if (fixed[a] && fixed[b]) continue;
                if (length(pos[a] - pos[b]) > 1.8f) continue;
                const Overlap o = overlap(pos[a], rot[a], pos[b], rot[b], 0.5f + air / 2);
                if (o.depth <= 0) continue;
                float sa = fixed[a] ? 0.0f : fixed[b] ? 1.0f : 0.5f;
                // One passing OVER the other: the upper one rises, the lower
                // one stays on the felt (pushing it down would only be undone
                // by the floor, and the overlap would stay).
                if (std::fabs(o.n.y) > 0.5f) {
                    const bool a_up = o.n.y > 0;           // n points from b to a
                    if (a_up && !fixed[a]) sa = 1.0f;
                    else if (!a_up && !fixed[b]) sa = 0.0f;
                }
                pos[a] = pos[a] + o.n * (o.depth * sa);
                pos[b] = pos[b] - o.n * (o.depth * (1.0f - sa));
                pos[a].y = std::fmax(pos[a].y, 0.5f);
                pos[b].y = std::fmax(pos[b].y, 0.5f);
            }
}

}  // namespace yatze::gfx
