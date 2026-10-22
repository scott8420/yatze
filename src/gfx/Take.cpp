#include "gfx/Take.hpp"
#include "gfx/Overlap.hpp"
#include "gfx/DieFaces.hpp"

#include <algorithm>
#include <cmath>

namespace yatze::gfx {

namespace {

constexpr float kPi = 3.14159265f;

// ── the physical constants (die = 1 unit; the scale is a toy table's) ──────
constexpr float kDt        = 1.0f / 240.0f;
constexpr float kGravity   = 34.0f;     // brisk but readable
constexpr float kInvMass   = 1.0f;
constexpr float kInvI      = 6.0f;      // a solid cube: I = m s^2 / 6
constexpr float kCorner    = 0.466f;    // the rounded corner's reach along each axis
constexpr float kRestFelt  = 0.32f;     // bounce off the felt
constexpr float kRestDice  = 0.45f;     // dice off dice
constexpr float kMuFelt    = 0.55f;
constexpr float kMuDice    = 0.25f;
constexpr int   kIters     = 4;
constexpr float kCeiling   = 2.1f;   // s019: no die's centre rises above this (see step)
constexpr double kMaxTime  = 7.0;

Quat conj(const Quat& q) { return {q.w, -q.x, -q.y, -q.z}; }

struct Rng {
    std::uint64_t s;
    float next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return float((z ^ (z >> 31)) >> 40) / float(1ull << 24);
    }
    float range(float a, float b) { return a + (b - a) * next(); }
};

struct Body {
    Vec3 x, v, w;
    Quat q;
    bool fixed = false;     // a held die, or a die gone to sleep: in the way, never moved
    float still = 0;        // seconds spent nearly motionless
    int   hops = 0;         // times it was helped off another die
    Vec3  ref_x;            // where it was at the last stillness check
    Quat  ref_q;
    float since = 0;        // time since that check
    float age = 0;          // time awake
};

const std::array<Vec3, 8>& corners() {
    static const std::array<Vec3, 8> c = [] {
        std::array<Vec3, 8> a{};
        int k = 0;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    a[k++] = Vec3{float(sx), float(sy), float(sz)} * kCorner;
        return a;
    }();
    return c;
}

// One contact, resolved as an impulse (normal + Coulomb friction) on A, and
// the opposite on B (if B is a body that moves).
// Returns the impact speed it resolved (0 if the bodies were already parting).
float impulse(Body& a, Body* b, const Vec3& p, const Vec3& n, float e, float mu) {
    const Vec3 ra = p - a.x;
    const Vec3 rb = b ? p - b->x : Vec3{};
    auto vel = [&] {
        Vec3 va = a.v + cross(a.w, ra);
        if (b && !b->fixed) va = va - (b->v + cross(b->w, rb));
        return va;
    };
    const bool bm = b && !b->fixed;
    auto k_of = [&](const Vec3& d) {
        float k = kInvMass + dot(d, cross(cross(ra, d) * kInvI, ra));
        if (bm) k += kInvMass + dot(d, cross(cross(rb, d) * kInvI, rb));
        return k;
    };
    const Vec3 vr = vel();
    const float vn = dot(vr, n);
    if (vn >= 0) return 0.0f;
    const float rest = vn < -0.9f ? e : 0.0f;          // no jitter at rest
    const float j = -(1 + rest) * vn / k_of(n);
    auto apply = [&](const Vec3& imp) {
        a.v = a.v + imp * kInvMass;
        a.w = a.w + cross(ra, imp) * kInvI;
        if (bm) {
            b->v = b->v - imp * kInvMass;
            b->w = b->w - cross(rb, imp) * kInvI;
        }
    };
    apply(n * j);
    // Friction along the sliding direction, capped by mu * the normal impulse.
    const Vec3 vr2 = vel();
    Vec3 t = vr2 - n * dot(vr2, n);
    const float tl = length(t);
    if (tl < 1e-6f) return -vn;
    t = t * (1.0f / tl);
    float jt = -dot(vr2, t) / k_of(t);
    jt = std::clamp(jt, -mu * j, mu * j);
    apply(t * jt);
    return -vn;
}

// A plane the dice cannot pass: the felt (n = +y) and the four rails.
struct Plane { Vec3 n; float d; float e, mu; };   // inside where dot(n, p) >= d

// Who knocked whom, and when, so one bump is one sound (several corners of a
// die touch at once; both dice of a pair see the same knock).
struct Ears {
    std::vector<Contact>* out = nullptr;
    double t = 0;
    std::array<std::array<double, 3>, 5> last{};       // per die, per kind (felt, die, rail)
    std::array<std::array<double, 5>, 5> pair{};       // per pair of dice
    static constexpr float  kAudible = 0.8f;           // slower touches make no sound
    static constexpr double kGap = 0.06;               // one knock per this long
    Ears() { for (auto& r : last) r.fill(-1); for (auto& r : pair) r.fill(-1); }
    void hear(int die, Contact::Kind k, const Vec3& p, float speed, int other = -1) {
        if (!out || speed < kAudible) return;
        double& when = other >= 0 ? pair[std::min(die, other)][std::max(die, other)]
                                  : last[die][int(k)];
        if (t - when < kGap) return;
        when = t;
        out->push_back({t, p, speed, k, die});
    }
};

// s019: no die inside another (see the comment inside). Called at the end
// of every step, and again after the cup's sleeve has moved dice.
void separate(std::vector<Body>& dice, Ears& ears, bool knocks = true) {
    // s019: no die inside another, by the exact test (gfx/Overlap.hpp). The
    // corner impulses above give the bounce and the spin; this pass makes
    // sure nothing is left inside -- an edge across an edge, a face pressed
    // into a face -- by moving the dice apart along the shortest way out and
    // taking away the speed that drives them together. A few passes, so a
    // die between two others settles all its contacts.
    for (int pass = 0; pass < 3; ++pass)
        for (std::size_t i = 0; i < dice.size(); ++i)
            for (std::size_t k = i + 1; k < dice.size(); ++k) {
                Body& a = dice[i];
                Body& b = dice[k];
                if (a.fixed && b.fixed) continue;
                if (length(a.x - b.x) > 1.75f) continue;
                const Overlap o = overlap(a.x, a.q, b.x, b.q);
                if (o.depth <= 0.002f) continue;
                if (pass == 0 && knocks) {
                    // A real knock, not just a shove: an impulse at the contact
                    // (between the two dice's nearest points along the way out)
                    // gives the bounce, the spin and the sound -- edge-on-edge
                    // knocks the corner test never saw.
                    auto support = [](const Body& d, const Vec3& dir) {   // the part of d furthest along dir
                        float best = -1e9f;
                        for (const auto& c : corners()) best = std::max(best, dot(d.q.rotate(c), dir));
                        Vec3 sum{};
                        int cnt = 0;
                        for (const auto& c : corners()) {
                            const Vec3 w = d.q.rotate(c);
                            if (dot(w, dir) > best - 0.03f) { sum = sum + w; ++cnt; }
                        }
                        return d.x + sum * (1.0f / float(cnt));
                    };
                    const Vec3 p = (support(a, o.n * -1.0f) + support(b, o.n)) * 0.5f;
                    const float hit = a.fixed ? impulse(b, &a, p, o.n * -1.0f, kRestDice, kMuDice)
                                              : impulse(a, &b, p, o.n, kRestDice, kMuDice);
                    ears.hear(int(i), Contact::Kind::Die, p, hit, int(k));
                }
                const float sa = a.fixed ? 0.0f : b.fixed ? 1.0f : 0.5f, sb = 1.0f - sa;
                a.x = a.x + o.n * (o.depth * sa);
                b.x = b.x - o.n * (o.depth * sb);
                const float closing = dot(a.v - b.v, o.n);       // < 0: still moving into each other
                if (closing < 0) {
                    if (!a.fixed) a.v = a.v - o.n * (closing * sa);
                    if (!b.fixed) b.v = b.v + o.n * (closing * sb);
                }
            }

}

void step(std::vector<Body>& dice, Ears& ears) {
    const Plane planes[] = {
        {{0, 1, 0}, 0.0f, kRestFelt, kMuFelt},
        {{1, 0, 0}, -kRailX, 0.4f, 0.2f},  {{-1, 0, 0}, -kRailX, 0.4f, 0.2f},
        {{0, 0, 1}, kRailBack, 0.4f, 0.2f}, {{0, 0, -1}, -kRailFront, 0.4f, 0.2f},
    };
    for (auto& b : dice) {
        if (b.fixed) continue;
        b.v.y -= kGravity * kDt;
    }
    for (int it = 0; it < kIters; ++it) {
        for (auto& b : dice) {
            if (b.fixed) continue;
            for (const auto& c : corners()) {
                const Vec3 p = b.x + b.q.rotate(c);
                for (const auto& pl : planes) {
                    const float pen = pl.d - dot(pl.n, p);
                    if (pen <= 0) continue;
                    const float hit = impulse(b, nullptr, p, pl.n, pl.e, pl.mu);
                    // The open front edge makes no sound (nothing is drawn there).
                    if (it == 0 && pl.n.z > -0.5f)
                        ears.hear(int(&b - dice.data()), pl.n.y > 0.5f ? Contact::Kind::Felt : Contact::Kind::Rail, p, hit);
                    if (it == kIters - 1) b.x = b.x + pl.n * (pen * 0.6f);   // push out
                }
            }
        }
        // Dice on dice: each die's corners against the other's box, both ways.
        for (std::size_t i = 0; i < dice.size(); ++i)
            for (std::size_t k = 0; k < dice.size(); ++k) {
                if (i == k) continue;
                Body& a = dice[i];
                Body& b = dice[k];
                if (a.fixed) continue;
                if (length(a.x - b.x) > 1.75f) continue;
                for (const auto& c : corners()) {
                    const Vec3 p = a.x + a.q.rotate(c);
                    const Vec3 l = conj(b.q).rotate(p - b.x);
                    const float ax = std::fabs(l.x), ay = std::fabs(l.y), az = std::fabs(l.z);
                    if (ax >= 0.5f || ay >= 0.5f || az >= 0.5f) continue;
                    Vec3 nl;
                    float pen;
                    if (ax >= ay && ax >= az) { nl = {l.x > 0 ? 1.0f : -1.0f, 0, 0}; pen = 0.5f - ax; }
                    else if (ay >= az)        { nl = {0, l.y > 0 ? 1.0f : -1.0f, 0}; pen = 0.5f - ay; }
                    else                      { nl = {0, 0, l.z > 0 ? 1.0f : -1.0f}; pen = 0.5f - az; }
                    const Vec3 n = b.q.rotate(nl);   // out of B, toward A
                    const float hit = impulse(a, &b, p, n, kRestDice, kMuDice);
                    if (it == 0) ears.hear(int(i), Contact::Kind::Die, p, hit, int(k));
                    if (it == kIters - 1) {
                        const float share = b.fixed ? 1.0f : 0.5f;
                        a.x = a.x + n * (pen * 0.5f * share);
                        if (!b.fixed) b.x = b.x - n * (pen * 0.5f * 0.5f);
                    }
                }
                // A sure bound the corner test cannot give: two dice that do
                // not overlap keep their centres at least 1.0 apart (their
                // inscribed spheres cannot meet), whatever their spin. Corners
                // pressed into faces slipped past the corner test (s005 probe:
                // centres 0.7-0.9 apart). Enforced on position and velocity.
                const Vec3 d = a.x - b.x;
                const float dl = length(d);
                if (dl < 1.0f && dl > 1e-5f) {
                    const Vec3 n = d * (1.0f / dl);
                    const float share = b.fixed ? 1.0f : 0.5f;
                    a.x = a.x + n * ((1.0f - dl) * share);
                    const float va = dot(a.v, n);
                    if (va < 0) a.v = a.v - n * va;
                    if (!b.fixed) {
                        b.x = b.x - n * ((1.0f - dl) * 0.5f);
                        const float vb = dot(b.v, n);
                        if (vb > 0) b.v = b.v - n * vb;
                    }
                }
            }
    }
    for (auto& b : dice) {
        if (b.fixed) continue;
        // Air and rolling drag: a little always, more when resting on something
        // -- the felt or another die (a die spun in place on top of another
        // for seconds with the felt-only rule, s005 probe).
        const bool low = b.x.y < 1.15f;
        b.v = b.v * (low ? 0.9985f : 0.9998f);
        b.w = b.w * (low ? 0.993f : 0.9995f);
        // A soft ceiling (s019): a knock can throw a die high; the drawn tray
        // is only so tall, and a die bouncing off thin air above its wall
        // looked wrong. Rising, a die may never have more upward speed than
        // would carry it to kCeiling -- it just doesn't bounce that high.
        if (b.v.y > 0) {
            const float vmax = std::sqrt(std::max(0.0f, 2.0f * kGravity * (kCeiling - b.x.y)));
            if (b.v.y > vmax) b.v.y = vmax;
        }
        b.x = b.x + b.v * kDt;
        const float wl = length(b.w);
        if (wl > 1e-6f) b.q = normalize(Quat::axis_angle(b.w, wl * kDt) * b.q);
        // Sleep: resting contact jitters forever in any impulse solver (the
        // s005 probe: settled by 1 s, creeping ~0.03 units/s after; a die
        // pressed on a rail vibrated in place -- tiny movement, but a velocity
        // that flips sign every step). So stillness is judged by how far the
        // die actually MOVED over each 0.1 s, as the eye judges it. Still for
        // a quarter second: it freezes and becomes an obstacle.
        b.since += kDt;
        if (b.since >= 0.1f) {
            const Quat dq = b.q * conj(b.ref_q);
            const float turned = 2.0f * std::acos(std::min(1.0f, std::fabs(dq.w)));
            const bool slow = length(b.x - b.ref_x) < 0.03f && turned < 0.06f;
            b.still = slow ? b.still + b.since : 0.0f;
            b.ref_x = b.x;
            b.ref_q = b.q;
            b.since = 0;
        }
        // Wedged between sleeping dice, a die can vibrate for ever (s005
        // probe). After 4.5 s awake it is frozen where it is; the final settle
        // lays it flat and spreads any overlap, so every take ends.
        b.age += kDt;
        if (b.age > 4.5f) { b.fixed = true; b.v = {}; b.w = {}; continue; }
        if (b.still >= 0.25f) {
            if (b.x.y > 0.6f && b.hops < 3) {
                ++b.hops;
                // Not flat on the felt -- propped on another die (or a rail).
                // A real die slides or hops off toward open felt: find the
                // nearest free spot (no die within 1.15, inside the rails) and
                // give it a hop sized to land there. (s005 probe: a push from
                // the die below died in friction and it settled back on top.)
                Vec3 target = b.x;
                float best = 1e9f;
                for (int ring = 0; ring < 4 && best > 1e8f; ++ring)
                    for (int k = 0; k < 16; ++k) {
                        const float ang = float(k) * (2.0f * kPi / 16.0f) + 0.2f * float(ring);
                        const float rad = 1.05f + 0.35f * float(ring);
                        const Vec3 c{b.x.x + std::cos(ang) * rad, 0.5f, b.x.z + std::sin(ang) * rad};
                        if (std::fabs(c.x) > kRailX - 0.6f || c.z < kRailBack + 0.6f || c.z > kRailFront - 0.6f) continue;
                        bool free = true;
                        for (const auto& o : dice)
                            if (&o != &b && length(Vec3{c.x - o.x.x, 0, c.z - o.x.z}) < 1.15f) free = false;
                        if (!free) continue;
                        const float d = length(Vec3{c.x - b.x.x, 0, c.z - b.x.z});
                        if (d < best) { best = d; target = c; }
                    }
                // No free spot near (wedged in a corner): toward the middle.
                if (best > 1e8f) target = Vec3{0, 0.5f, -1.2f};
                Vec3 dir = normalize(Vec3{target.x - b.x.x, 0, target.z - b.x.z});
                if (length(dir) < 0.5f) dir = {1, 0, 0};
                const float hop = 2.4f;                                   // up speed
                const float flight = 2.0f * hop / kGravity + 0.08f;      // time aloft
                const float dist = best < 1e8f ? best : 1.3f;
                b.v = dir * (dist / flight) + Vec3{0, hop, 0};
                b.w = Vec3{dir.z, 0, -dir.x} * 6.0f;
                b.still = 0;
            } else {
                b.fixed = true;
                b.v = {};
                b.w = {};
            }
        }
    }
    separate(dice, ears);
}

// The face-up orientation nearest to q (same spin about the vertical).
Quat squared(const Quat& q) {
    int best = 1;
    float bd = -2;
    for (int v = 1; v <= 6; ++v) {
        const float d = dot(q.rotate(face_normal(v)), Vec3{0, 1, 0});
        if (d > bd) { bd = d; best = v; }
    }
    return normalize(from_to(q.rotate(face_normal(best)), {0, 1, 0}) * q);
}

}  // namespace

// ── playback helpers ────────────────────────────────────────────────────────
std::array<DiePose, 5> Take::dice_at(double t) const {
    if (frames.empty()) return {};
    const double f = std::clamp(t * kFps, 0.0, double(frames.size() - 1));
    const std::size_t i = std::size_t(f);
    const std::size_t j = std::min(i + 1, frames.size() - 1);
    const float u = float(f - double(i));
    std::array<DiePose, 5> out{};
    for (int d = 0; d < 5; ++d)
        out[d] = {lerp(frames[i][d].pos, frames[j][d].pos, u), slerp(frames[i][d].rot, frames[j][d].rot, u)};
    return out;
}

CamPose Take::cam_at(double t) const {
    if (cams.empty()) return {};
    const double f = std::clamp(t * kFps, 0.0, double(cams.size() - 1));
    const std::size_t i = std::size_t(f);
    const std::size_t j = std::min(i + 1, cams.size() - 1);
    const float u = float(f - double(i));
    return {lerp(cams[i].eye, cams[j].eye, u), lerp(cams[i].at, cams[j].at, u)};
}

CupPose Take::cup_at(double t) const {
    if (cups.empty()) return {{}, {}, false};
    const double f = std::clamp(t * kFps, 0.0, double(cups.size() - 1));
    const std::size_t i = std::size_t(f);
    const std::size_t j = std::min(i + 1, cups.size() - 1);
    const float u = float(f - double(i));
    return {lerp(cups[i].pos, cups[j].pos, u), slerp(cups[i].rot, cups[j].rot, u), cups[i].shown && cups[j].shown};
}

// ── the shoot ───────────────────────────────────────────────────────────────
Take shoot(const Shot& shot) {
    Rng r{shot.seed ? shot.seed : 1};
    Take take;

    // 1. The throw: the cup at your side, a little left or right; the dice
    //    in a loose clump, tossed toward the far side, spinning.
    std::vector<Body> dice(5);
    // With dice held at your edge the cup pours from a little further in, so
    // it clears them (s010: the cup is drawn now).
    const bool any_held = std::any_of(shot.held.begin(), shot.held.end(), [](bool h) { return h; });
    const Vec3 cup{r.range(-1.3f, 1.3f), r.range(1.2f, 1.6f), r.range(0.15f, 0.55f) - (any_held ? 0.9f : 0.0f)};
    const float power = r.range(0.75f, 1.2f);                       // soft .. hard throws
    const float heading = r.range(-0.35f, 0.35f);                   // a little across the table
    // The dice as they lie in the cup: two pairs side by side along the
    // cup's length and one on top -- packed close enough to sit INSIDE it
    // (s019: the old loose clump, 1.75 apart, poked through the cup's wall),
    // then parted by the exact test so none starts inside another whatever
    // its spin. (An overlapping start shot one die off at 34 units/s -- the
    // s005 probe.)
    // (Centred where the old clump was -- 0.7 up, 0.64 in -- so the cup's
    // path, which clears dice held at your edge, is unchanged.)
    const Vec3 clump[5] = {{-0.55f, 0.5f, -0.04f}, {0.55f, 0.5f, -0.04f}, {-0.55f, 0.5f, -1.24f},
                           {0.55f, 0.5f, -1.24f}, {0, 1.45f, -0.64f}};
    const float group_speed = power * r.range(4.2f, 6.0f), group_up = r.range(0.8f, 2.0f);
    int slot = 0;
    for (int i = 0; i < 5; ++i) {
        Body& b = dice[i];
        if (shot.held[i]) {
            b.fixed = true;
            b.x = shot.slots[i];
            b.q = show(shot.values[i], {0, 1, 0});
            continue;
        }
        const Vec3 jitter{r.range(-0.03f, 0.03f), r.range(-0.03f, 0.03f), r.range(-0.03f, 0.03f)};
        b.x = cup + clump[slot++] + jitter;
        b.q = normalize(Quat{r.range(-1, 1), r.range(-1, 1), r.range(-1, 1), r.range(-1, 1)});
        // Tossed onto the felt to tumble into the back wall, not thrown at it
        // (s009: harder throws hit the wall in flight, high above any rail).
        // They leave the cup TOGETHER (s019: packed in the cup now, dice
        // thrown each its own way met at once and sprayed over the walls):
        // one throw for all, a little spread each.
        const float speed = group_speed + r.range(-0.5f, 0.5f);
        b.v = Vec3{std::sin(heading) * speed + r.range(-0.35f, 0.35f), group_up + r.range(-0.25f, 0.25f),
                   -std::cos(heading) * speed + r.range(-0.3f, 0.3f)};
        b.ref_x = b.x;
        b.ref_q = b.q;
        b.w = Vec3{r.range(-1, 1), r.range(-1, 1), r.range(-1, 1)} * r.range(10.0f, 20.0f);
    }

    {   // Part the packed dice before the first frame.
        std::array<Vec3, 5> pos;
        std::array<Quat, 5> rot;
        std::array<bool, 5> still{};
        for (int i = 0; i < 5; ++i) { pos[std::size_t(i)] = dice[i].x; rot[std::size_t(i)] = dice[i].q; still[std::size_t(i)] = dice[i].fixed; }
        part(pos, rot, still, 0.02f, 20);
        for (int i = 0; i < 5; ++i) { dice[i].x = pos[std::size_t(i)]; dice[i].ref_x = dice[i].x; }
    }

    // The cup after the spill (s010; planned here, before the physics, since
    // s019): placed on the whole clump (however many dice roll), so where it
    // pours does not depend on what you held. First it slides BACK along its
    // own length -- the mouth withdraws from the dice pouring out of it --
    // then rises and leaves sideways (up alone is toward a high camera).
    const Vec3 fwd{std::sin(heading), 0, -std::cos(heading)};           // toward the far side
    auto axis = [&](float tilt) { return normalize(Vec3{0, std::cos(tilt), 0} + fwd * std::sin(tilt)); };
    const float tilt_spill = r.range(1.62f, 1.78f);                      // mouth a little below level
    Vec3 g{};
    for (const auto& c : clump) g = g + c;
    g = cup + g * 0.2f;
    const Vec3 cup_centre = g - axis(tilt_spill) * 0.35f + Vec3{0, 0.4f, 0};
    const float lift = 0.85f;                                             // the follow-through, seconds
    auto cup_after = [&](float te) {
        const float u = std::clamp(te / lift, 0.0f, 1.0f);
        const float e = u * u * (3 - 2 * u);
        const float back = 1.0f - (1.0f - u) * (1.0f - u);               // quick, then easing
        // Lifted a little as it slides back (clear of dice held at your
        // edge), then properly once the dice are out, then away sideways.
        const float rise = 0.3f * back + 2.8f * smoothstep01((u - 0.25f) / 0.55f);
        const float side = smoothstep01((u - 0.45f) / 0.55f);
        CupPose p;
        p.pos = cup_centre - axis(tilt_spill) * (kCupHeight * back) +
                Vec3{(cup.x >= 0 ? 1.0f : -1.0f) * 8.0f * side, rise, 0};
        p.rot = from_to({0, 1, 0}, axis(tilt_spill + 0.35f * e));
        p.shown = u < 1.0f;
        return p;
    };
    // While it withdraws, the cup is solid to the dice still in it: they
    // slide along its inside and leave by the mouth, never through the wall
    // or the base (s019 probe: the dice fell through its lower wall as it
    // lifted, in every take).
    auto sleeve = [&](float te) {
        if (te >= lift) return;
        const CupPose c = cup_after(te), c0 = cup_after(te - kDt);
        const Vec3 a = c.axis(), cv = (c.pos - c0.pos) * (1.0f / kDt);
        const float inner = kCupRadius - 0.55f, floor_at = -kCupHeight / 2 + 0.55f;
        for (auto& b : dice) {
            if (b.fixed) continue;
            const Vec3 d = b.x - c.pos;
            const float ax = dot(d, a);
            if (ax > kCupHeight / 2) continue;                            // out of the mouth: free
            const Vec3 rad = d - a * ax;
            const float rl = length(rad);
            if (rl > inner && rl < kCupRadius + 0.6f) {
                const Vec3 n = rad * (1.0f / rl);
                b.x = b.x - n * (rl - inner);
                const float vn = dot(b.v - cv, n);
                if (vn > 0) b.v = b.v - n * vn;
            }
            if (ax < floor_at && ax > -kCupHeight / 2 - 0.6f && rl < kCupRadius) {
                b.x = b.x + a * (floor_at - ax);
                const float va = dot(b.v - cv, a);
                if (va < 0) b.v = b.v - a * va;
            }
        }
    };

    // 2. Run to rest, recording at 60 Hz (every 4th physics step).
    const int per_frame = int(std::lround(1.0 / (Take::kFps * kDt)));
    auto record = [&] {
        std::array<DiePose, 5> f{};
        for (int i = 0; i < 5; ++i) f[i] = {dice[i].x, dice[i].q};
        take.frames.push_back(f);
    };
    Ears ears;
    for (int rep = 0; rep < 3; ++rep) {   // the packed dice start inside the cup, too, and apart
        sleeve(kDt);
        separate(dice, ears, false);
    }
    record();
    ears.out = &take.contacts;
    for (int s = 1; s < int(kMaxTime / kDt); ++s) {
        ears.t = double(s) * kDt;
        step(dice, ears);
        // The cup's inside, then the dice apart again (the sleeve may have
        // pressed one into another), twice over.
        for (int rep = 0; rep < 2; ++rep) {
            sleeve(float(s) * kDt);
            separate(dice, ears, false);
        }
        if (s % per_frame == 0) record();
        const bool all_still = std::all_of(dice.begin(), dice.end(), [](const Body& b) { return b.fixed; });
        if (all_still && s % per_frame == 0) break;
    }

    // 3. Settle flat over a quarter second (a die stopped on an edge falls).
    std::array<DiePose, 5> last = take.frames.back();
    std::array<DiePose, 5> flat = last;
    for (int i = 0; i < 5; ++i) {
        if (shot.held[i]) continue;
        flat[i].rot = squared(last[i].rot);
        flat[i].pos.y = 0.5f;
    }
    // Flat dice can still touch -- or, laid flat from a lean, sit inside each
    // other: spread them by the exact test for squares turned on the felt
    // (s019; until then by centre distance, which let two dice at different
    // angles overlap), to a hair of air, inside the rails. Held dice stay.
    constexpr float kAir = 0.03f;
    for (int pass = 0; pass < 80; ++pass) {
        bool moved = false;
        for (int i = 0; i < 5; ++i)
            for (int j = i + 1; j < 5; ++j) {
                const bool hi = shot.held[i], hj = shot.held[j];
                if (hi && hj) continue;
                const Overlap o = overlap_flat(flat[i].pos, flat[i].rot, flat[j].pos, flat[j].rot);
                if (o.depth <= -kAir) continue;
                const float gap = o.depth + kAir + 0.002f;
                const float si = hj ? 1.0f : hi ? 0.0f : 0.5f, sj = 1.0f - si;
                flat[i].pos = flat[i].pos + o.n * (gap * si);
                flat[j].pos = flat[j].pos - o.n * (gap * sj);
                moved = true;
            }
        for (int i = 0; i < 5; ++i) {
            if (shot.held[i]) continue;
            flat[i].pos.x = std::clamp(flat[i].pos.x, -kRailX + 0.5f, kRailX - 0.5f);
            flat[i].pos.z = std::clamp(flat[i].pos.z, kRailBack + 0.5f, kRailFront - 0.5f);
        }
        if (!moved) break;
    }
    const int settle = int(0.25 * Take::kFps);
    for (int k = 1; k <= settle; ++k) {
        const float u = ease_in_out(float(k) / float(settle));
        std::array<DiePose, 5> f{};
        // Across first, then down: a die left propped on another slides off
        // to its clear spot before it drops (s019: it came straight down
        // through the die below).
        const float t = float(k) / float(settle);
        const float uh = ease_in_out(std::min(1.0f, t * 1.6f)), uv = ease_in_out(std::max(0.0f, (t - 0.35f) / 0.65f));
        for (int i = 0; i < 5; ++i) {
            Vec3 p = lerp(last[i].pos, flat[i].pos, uh);
            p.y = last[i].pos.y + (flat[i].pos.y - last[i].pos.y) * (last[i].pos.y > 0.6f ? uv : u);
            f[i] = {p, slerp(last[i].rot, flat[i].rot, u)};
        }
        // Turning flat, two neighbours can swing into each other: part them
        // on the felt (s019). The last frame is the spread one, already clear.
        for (int pass = 0; pass < 4 && k < settle; ++pass)
            for (int i = 0; i < 5; ++i)
                for (int j = i + 1; j < 5; ++j) {
                    if (shot.held[i] && shot.held[j]) continue;
                    const Overlap o = overlap(f[i].pos, f[i].rot, f[j].pos, f[j].rot);
                    if (o.depth <= 0.002f) continue;
                    Vec3 n{o.n.x, 0, o.n.z};
                    const float nl = length(n);
                    n = nl > 1e-3f ? n * (1.0f / nl) : Vec3{1, 0, 0};
                    const float si = shot.held[j] ? 1.0f : shot.held[i] ? 0.0f : 0.5f;
                    f[i].pos = f[i].pos + n * (o.depth * si);
                    f[j].pos = f[j].pos - n * (o.depth * (1.0f - si));
                }
        take.frames.push_back(f);
    }

    // 4. Relabel: turn each rolled die in its own frame (a symmetry of the
    //    cube -- it looks and moves the same) so the drawn face ends on top.
    for (int i = 0; i < 5; ++i) {
        if (shot.held[i]) continue;
        const int landed = value_facing(flat[i].rot, {0, 1, 0});
        const Quat relabel = from_to(face_normal(shot.values[i]), face_normal(landed));
        for (auto& f : take.frames) f[i].rot = normalize(f[i].rot * relabel);
    }

    // 5. The cup (s010). A lead-in is put in front of the physics: the cup at
    //    your side, shaken and tipping, the dice in it (the first frame held);
    //    at the spill it points at the table, mouth around the clump, and the
    //    physics begins; then it follows through and lifts up out of the shot.
    {
        const float lead = r.range(0.45f, 0.65f);
        const int lead_n = int(std::lround(lead * Take::kFps));
        take.spill = double(lead_n) / Take::kFps;
        const auto first = take.frames.front();
        take.frames.insert(take.frames.begin(), std::size_t(lead_n), first);
        for (auto& c : take.contacts) c.t += take.spill;

        const Vec3 mouth_spill = cup_centre + axis(tilt_spill) * (kCupHeight / 2);
        const float shake_hz = r.range(7.5f, 10.0f);
        take.cups.resize(take.frames.size());
        for (std::size_t f = 0; f < take.frames.size(); ++f) {
            const float t = float(double(f) / Take::kFps);
            CupPose& p = take.cups[f];
            p.shown = true;
            if (t < float(take.spill)) {
                // Poured, not swung: the MOUTH comes down from above toward the
                // spot while the cup tips past level (the base rises behind it),
                // so it stays clear of the dice held at your edge.
                const float u = t / float(take.spill), v = 1.0f - u * u;
                const float tilt = 0.9f + (tilt_spill - 0.9f) * u;
                const float shake = std::sin(t * shake_hz * 6.2831853f) * 0.09f * (1.0f - u);
                const Vec3 mouth = mouth_spill + Vec3{0, 2.5f, 0.0f} * v + Vec3{shake, shake * 0.4f, 0};
                p.pos = mouth - axis(tilt) * (kCupHeight / 2);
                p.rot = from_to({0, 1, 0}, axis(tilt));
            } else {
                p = cup_after(t - float(take.spill));
            }
        }
    }

    // 6. The camera, planned over the whole take.
    const std::size_t n = take.frames.size();
    std::vector<Vec3> centre(n);
    std::vector<float> spread(n);
    for (std::size_t f = 0; f < n; ++f) {
        Vec3 c{};
        for (const auto& d : take.frames[f]) c = c + d.pos;
        c = c * 0.2f;
        float rad = 0;
        for (const auto& d : take.frames[f]) rad = std::max(rad, length(d.pos - c) + 0.75f);
        if (double(f) / Take::kFps < take.spill + 0.1)                 // the cup in the shot too
            rad = std::max(rad, length(take.cups[f].pos - c) + kCupRadius + 0.4f);
        centre[f] = c;
        spread[f] = rad;
    }
    // Look ahead: the spread to fit is the largest coming in the next 0.7 s;
    // then both tracks are smoothed BOTH ways in time (a planned move, no lag).
    const int ahead = int(0.7 * Take::kFps);
    std::vector<float> fit(n);
    for (std::size_t f = 0; f < n; ++f) {
        float m = 0;
        for (std::size_t g = f; g < std::min(n, f + std::size_t(ahead)); ++g) m = std::max(m, spread[g]);
        fit[f] = m;
    }
    const int sigma = int(0.35 * Take::kFps);
    auto blur = [&](auto get, auto& out) {
        for (std::size_t f = 0; f < n; ++f) {
            double wsum = 0;
            auto acc = get(0) * 0.0f;
            for (int k = -3 * sigma; k <= 3 * sigma; ++k) {
                const long g = std::clamp(long(f) + k, 0L, long(n) - 1);
                const float wgt = std::exp(-0.5f * float(k * k) / float(sigma * sigma));
                acc = acc + get(std::size_t(g)) * wgt;
                wsum += wgt;
            }
            out[f] = acc * float(1.0 / wsum);
        }
    };
    std::vector<Vec3> c_s(n);
    blur([&](std::size_t f) { return centre[f]; }, c_s);
    std::vector<float> fit_s(n);
    for (std::size_t f = 0; f < n; ++f) fit_s[f] = fit[f];
    {
        std::vector<float> tmp(n);
        for (std::size_t f = 0; f < n; ++f) {
            double wsum = 0, acc = 0;
            for (int k = -3 * sigma; k <= 3 * sigma; ++k) {
                const long g = std::clamp(long(f) + k, 0L, long(n) - 1);
                const double wgt = std::exp(-0.5 * double(k * k) / double(sigma * sigma));
                acc += fit[std::size_t(g)] * wgt;
                wsum += wgt;
            }
            tmp[f] = std::max(fit[f], float(acc / wsum));   // smoothing never shrinks below the need
        }
        fit_s = tmp;
    }

    // The seat: a lean, a height (35 deg across the felt .. 78 deg nose down),
    // and a slow drift + push-in over the take.
    const float az0 = r.range(-0.42f, 0.42f), az1 = az0 + r.range(-0.14f, 0.14f);
    const float el0 = r.range(0.62f, 1.36f),  el1 = std::clamp(el0 + r.range(-0.10f, 0.10f), 0.6f, 1.4f);
    const float push = r.range(0.0f, 0.08f);
    const float tv = std::tan(shot.fov / 2), th = tv * std::max(0.4f, shot.aspect);
    const float t_min = std::min(tv, th);
    take.cams.resize(n);
    for (std::size_t f = 0; f < n; ++f) {
        const float u = n > 1 ? float(f) / float(n - 1) : 1.0f;
        const float s = ease_in_out(u);
        const float az = az0 + (az1 - az0) * s, el = el0 + (el1 - el0) * s;
        const float dist = std::max(fit_s[f] / t_min * 1.06f, 5.0f) * (1.0f - push * s);
        const Vec3 at = c_s[f] + Vec3{0, 0.25f, 0};
        const Vec3 dir{std::sin(az) * std::cos(el), std::sin(el), std::cos(az) * std::cos(el)};
        take.cams[f] = {at + dir * dist, at};
    }
    (void)kPi;
    return take;
}

}  // namespace yatze::gfx
