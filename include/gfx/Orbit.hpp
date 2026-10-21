#pragma once
// gfx::Orbit -- the viewer's own view of the table (s008, Scott: "an orb with
// the x, y, z that the user can pivot ... and reset view to a standard").
// GTK-free; the selftest checks the directions the orb shows and the snaps.
//
// The presented camera sits on a sphere around the row: AZIMUTH around the
// vertical (0 = your side of the table, +Z), ELEVATION above the felt (never
// below it -- there is no under the table). The orb draws the world axes as
// this camera sees them; dragging it changes az / el; clicking an axis ball
// looks along that axis. World axes: X across the table (left -> right as you
// sit), Y up, Z toward you.
#include "gfx/Math.hpp"

#include <cmath>

namespace yatze::gfx {

inline constexpr float kOrbitElMin = 0.10f;    // ~6 deg: just above the felt
inline constexpr float kOrbitElMax = 1.553f;   // ~89 deg: straight down (not quite, so "up" stays defined)

struct Orbit {
    float az = 0.0f;    // radians, wrapped to (-pi, pi]
    float el = 0.87f;   // radians, clamped to [kOrbitElMin, kOrbitElMax]
};

inline Orbit clamp(Orbit o) {
    constexpr float pi = 3.14159265f;
    while (o.az > pi) o.az -= 2 * pi;
    while (o.az <= -pi) o.az += 2 * pi;
    o.el = std::fmin(kOrbitElMax, std::fmax(kOrbitElMin, o.el));
    return o;
}

// From the target toward the eye.
inline Vec3 orbit_dir(const Orbit& o) {
    return {std::sin(o.az) * std::cos(o.el), std::sin(o.el), std::cos(o.az) * std::cos(o.el)};
}

// Dragging the orb: pixels -> radians (drag right turns the table to the
// right under you, as grabbing a globe does; drag down looks from higher).
inline Orbit drag(Orbit o, float dx_px, float dy_px) {
    o.az -= dx_px * 0.010f;
    o.el += dy_px * 0.008f;
    return clamp(o);
}

// A world axis as the camera sees it: right / up on screen, and depth
// (positive = pointing away from the viewer, drawn behind the orb).
struct AxisOnScreen { float x, y, depth; };
inline AxisOnScreen project_axis(const Vec3& axis, const Vec3& eye, const Vec3& at) {
    const Vec3 f = normalize(at - eye);
    const Vec3 s = normalize(cross(f, {0, 1, 0}));
    const Vec3 u = cross(s, f);
    return {dot(axis, s), dot(axis, u), dot(axis, f)};
}

// The six axis balls: +X, -X, +Y, -Y, +Z, -Z. Clicking one looks ALONG it
// toward the table: +X from the right side, +Y from straight above, +Z from
// your side, at eye level with the dice. -Y would be from under the felt, so
// it gives the top view too.
inline Vec3 axis_vec(int i) {
    static const Vec3 a[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    return a[(i >= 0 && i < 6) ? i : 4];
}
inline Orbit snap(int i, const Orbit& now) {
    constexpr float pi = 3.14159265f, low = 0.14f;
    switch (i) {
        case 0: return clamp({ pi / 2, low});
        case 1: return clamp({-pi / 2, low});
        case 2:
        case 3: return clamp({now.az, kOrbitElMax});
        case 4: return clamp({0.0f, low});
        case 5: return clamp({pi, low});
    }
    return now;
}

}  // namespace yatze::gfx
