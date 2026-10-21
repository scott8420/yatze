#pragma once
// gfx::DieFaces -- which number is on which face of a die, and the rotation
// that shows a given number in a given direction. GTK-free; the selftest checks
// every face both ways (on top, facing you) -- a die that shows a 4 when the
// game holds a 3 is the one bug this view must never have.
//
// The die, in its own axes (a standard Western die: opposite faces sum to 7,
// and 1-2-3 run counterclockwise around their shared corner):
//     +Z = 1   -Z = 6   +X = 2   -X = 5   +Y = 3   -Y = 4
// The fragment shader draws pips from the same table (face_of_axis there).
#include "gfx/Math.hpp"

namespace yatze::gfx {

// The outward normal (die axes) of the face showing `value` (1..6).
inline Vec3 face_normal(int value) {
    switch (value) {
        case 1: return {0, 0, 1};
        case 6: return {0, 0, -1};
        case 2: return {1, 0, 0};
        case 5: return {-1, 0, 0};
        case 3: return {0, 1, 0};
        case 4: return {0, -1, 0};
    }
    return {0, 0, 1};
}

// The value on the face whose normal (in world axes) points most along `dir`.
inline int value_facing(const Quat& rot, const Vec3& dir) {
    int best = 1;
    float bd = -2;
    for (int v = 1; v <= 6; ++v) {
        const float d = dot(rot.rotate(face_normal(v)), dir);
        if (d > bd) { bd = d; best = v; }
    }
    return best;
}

// A rotation that turns face `value` toward `dir`, then spins `twist` radians
// about `dir` (a die lying on the felt can face any way).
inline Quat show(int value, const Vec3& dir, float twist = 0.0f) {
    if (value < 1 || value > 6) return Quat::axis_angle(dir, twist);
    const Quat to = from_to(face_normal(value), normalize(dir));
    return Quat::axis_angle(dir, twist) * to;
}

}  // namespace yatze::gfx
