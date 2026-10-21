#include "DiceView.hpp"
#include "Log.hpp"
#include "gfx/DieFaces.hpp"
#include "gfx/Mesh.hpp"
#include "gfx/Take.hpp"

#include <epoxy/gl.h>
#include <gdkmm/cursor.h>
#include <gdkmm/frameclock.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <string>

namespace yatze {

using gfx::Mat4;
using gfx::Quat;
using gfx::Vec3;

namespace {

constexpr float kPi      = 3.14159265f;
constexpr float kFov     = 30.0f * kPi / 180.0f;
constexpr float kRowZ    = 1.2f;    // the presented row's line on the felt
constexpr float kRowGap  = 1.32f;   // centre to centre
constexpr float kHeldFwd = 0.45f;   // a held die steps this far toward you
constexpr float kPickR   = 0.62f;   // a die's pick sphere

// The pace of a roll is drawn per roll (Scott, s004b: "too fast", then
// "maybe it is variable timing so it feels real") -- see plan_roll.
constexpr double kEase  = 8.0;      // how briskly a held die steps forward (per second)

// ── shaders ─────────────────────────────────────────────────────────────────
const char* kDieVS = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
uniform mat4 u_model;
uniform mat4 u_vp;
out vec3 v_lpos; out vec3 v_lnrm; out vec3 v_wpos; out vec3 v_wnrm;
void main() {
    vec4 w = u_model * vec4(a_pos, 1.0);
    v_wpos = w.xyz;
    v_wnrm = mat3(u_model) * a_nrm;
    v_lpos = a_pos;
    v_lnrm = a_nrm;
    gl_Position = u_vp * w;
}
)";

// Pips from the face table (u_face, filled from gfx::face_normal -- the same
// table the selftest checks): which value is on +X, -X, +Y, -Y, +Z, -Z.
const char* kDieFS = R"(#version 330 core
in vec3 v_lpos; in vec3 v_lnrm; in vec3 v_wpos; in vec3 v_wnrm;
uniform vec3  u_eye;
uniform vec3  u_light;
uniform int   u_face[6];
uniform float u_blank;
uniform vec3  u_body;
uniform vec3  u_pip;
uniform float u_gloss;
out vec4 o_col;
float pip(vec2 uv, vec2 c) {
    float d = length(uv - c), r = 0.20, aa = fwidth(d) * 1.2;
    return 1.0 - smoothstep(r - aa, r + aa, d);
}
float pips(int v, vec2 uv) {
    float s = 0.0, k = 0.54;
    if (v == 1 || v == 3 || v == 5) s += pip(uv, vec2(0.0));
    if (v >= 2) { s += pip(uv, vec2(-k, -k)); s += pip(uv, vec2(k, k)); }
    if (v >= 4) { s += pip(uv, vec2(-k, k)); s += pip(uv, vec2(k, -k)); }
    if (v == 6) { s += pip(uv, vec2(-k, 0.0)); s += pip(uv, vec2(k, 0.0)); }
    return clamp(s, 0.0, 1.0);
}
void main() {
    vec3 a = abs(v_lnrm);
    int idx; vec2 uv;
    if (a.x >= a.y && a.x >= a.z) { idx = v_lnrm.x > 0.0 ? 0 : 1; uv = v_lpos.zy; }
    else if (a.y >= a.z)          { idx = v_lnrm.y > 0.0 ? 2 : 3; uv = v_lpos.xz; }
    else                          { idx = v_lnrm.z > 0.0 ? 4 : 5; uv = v_lpos.xy; }
    uv /= 0.5 * 0.80;
    float flatness = smoothstep(0.92, 0.985, max(a.x, max(a.y, a.z)));
    float p = (u_blank > 0.5) ? 0.0 : pips(u_face[idx], uv) * flatness;

    vec3 N = normalize(v_wnrm), L = normalize(u_light), V = normalize(u_eye - v_wpos);
    vec3 H = normalize(L + V);
    vec3 base = mix(u_body, u_pip, p);
    float diff = max(dot(N, L), 0.0);
    float fill = max(dot(N, normalize(vec3(-0.7, 0.35, 0.6))), 0.0) * 0.28;
    float rim  = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.12;
    float spec = pow(max(dot(N, H), 0.0), 56.0) * (1.0 - p) * u_gloss;
    vec3 col = base * (0.30 + 0.72 * diff + fill) + vec3(spec + rim);
    o_col = vec4(col, 1.0);
}
)";

// The felt, and the soft things on it (shadows, the hold ring, the hint glow):
// quads lying on y = 0, coloured by mode.
const char* kFlatVS = R"(#version 330 core
layout(location = 0) in vec2 a_xy;
uniform mat4  u_vp;
uniform vec3  u_center;
uniform float u_size;
out vec2 v_uv; out vec3 v_world;
void main() {
    v_uv = a_xy;
    v_world = u_center + vec3(a_xy.x * u_size, 0.0, a_xy.y * u_size);
    gl_Position = u_vp * vec4(v_world, 1.0);
}
)";
const char* kFlatFS = R"(#version 330 core
in vec2 v_uv; in vec3 v_world;
uniform int   u_mode;      // 0 felt, 1 shadow, 2 ring
uniform vec3  u_color;
uniform float u_strength;
out vec4 o_col;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main() {
    float d = length(v_uv);
    if (u_mode == 0) {
        vec3 felt = vec3(0.075, 0.30, 0.19);
        vec3 dark = vec3(0.025, 0.075, 0.05);            // = the clear colour: no horizon line
        float r = length(v_world.xz - vec2(0.0, -0.2));
        float grain = (hash(floor(v_world.xz * 90.0)) - 0.5) * 0.035;
        vec3 lit = felt + grain;
        o_col = vec4(mix(lit, dark, smoothstep(4.0, 16.0, r)), 1.0);
    } else if (u_mode == 1) {
        float a = pow(1.0 - smoothstep(0.0, 1.0, d), 1.6) * u_strength;
        o_col = vec4(0.0, 0.0, 0.0, a);
    } else {
        float ring = exp(-pow((d - 0.74) / 0.11, 2.0));
        float fill = 0.30 * (1.0 - smoothstep(0.0, 0.78, d));
        o_col = vec4(u_color, (ring + fill) * u_strength);
    }
}
)";

unsigned compile(unsigned type, const char* src, std::string& err) {
    const unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(s, sizeof buf, nullptr, buf);
        err = buf;
        glDeleteShader(s);
        return 0;
    }
    return s;
}
unsigned link(const char* vs, const char* fs, std::string& err) {
    const unsigned v = compile(GL_VERTEX_SHADER, vs, err);
    if (!v) return 0;
    const unsigned f = compile(GL_FRAGMENT_SHADER, fs, err);
    if (!f) { glDeleteShader(v); return 0; }
    const unsigned p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetProgramInfoLog(p, sizeof buf, nullptr, buf);
        err = buf;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}
void set_mat(unsigned prog, const char* name, const Mat4& m) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, m.m.data());
}
void set_vec(unsigned prog, const char* name, const Vec3& v) {
    glUniform3f(glGetUniformLocation(prog, name), v.x, v.y, v.z);
}

// A small seeded generator for the roll's layout and viewpoint.
struct Rng {
    std::uint64_t s;
    float next() {   // 0..1
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return float((z ^ (z >> 31)) >> 40) / float(1ull << 24);
    }
    float range(float a, float b) { return a + (b - a) * next(); }
};

const Vec3 kLight = gfx::normalize(Vec3{0.45f, 1.0f, 0.55f});

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
DiceView::DiceView() {
    set_name("play.dice3d");
    set_required_version(3, 3);
    set_has_depth_buffer(false);   // we draw into our own multisampled buffer
    set_auto_render(false);
    set_hexpand(true);
    set_vexpand(true);
    set_size_request(560, 300);
    for (int i = 0; i < 5; ++i) m_pose[i] = present_pose(i);
    m_cam = present_cam();

    auto click = Gtk::GestureClick::create();
    click->signal_released().connect([this](int, double x, double y) {
        const int i = pick(x, y);
        if (i >= 0) m_sig_die.emit(i);
    });
    add_controller(click);
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect([this](double x, double y) {
        const int i = pick(x, y);
        if (i != m_hover) {
            m_hover = i;
            set_cursor(i >= 0 ? "pointer" : "default");
            queue_render();
        }
    });
    motion->signal_leave().connect([this] { m_hover = -1; set_cursor("default"); queue_render(); });
    add_controller(motion);
}

DiceView::~DiceView() {
    if (m_tick_id) remove_tick_callback(m_tick_id);
}

// ── the model's dice ────────────────────────────────────────────────────────
void DiceView::set(const model::Dice& dice, const std::array<bool, 5>& held, bool can_hold,
                   std::uint64_t roll_key) {
    const bool rolled_now = model::rolled(dice) && roll_key != m_roll_key;
    m_dice     = dice;
    m_held     = held;
    m_can_hold = can_hold;
    m_roll_key = roll_key;
    if (!model::rolled(dice)) {
        // A fresh turn: blank dice, straight on, no fuss.
        m_phase = Phase::Present;
        m_cam   = present_cam();
        for (int i = 0; i < 5; ++i) { m_pose[i] = present_pose(i); m_lift[i] = 0; }
    } else if (rolled_now) {
        std::uint64_t seed = roll_key * 0x9E3779B97F4A7C15ull;
        for (int f : dice) seed = seed * 7 + std::uint64_t(f);
        plan_roll(seed);
        m_from     = m_pose;
        m_phase    = Phase::Take;
        m_phase_t0 = -1;
        m_hover    = -1;                  // no stale highlight on a rolling die                  // anchored by the next tick (tick())
    }
    start_ticking();
    queue_render();
}

void DiceView::set_red(bool red) {
    if (red == m_red) return;
    m_red = red;
    queue_render();
}

void DiceView::set_left_inset(float px) {
    const float frac = get_width() > 0 ? std::max(0.0f, px) / float(get_width()) : 0.0f;
    if (std::fabs(frac - m_inset_frac) < 1e-3f) return;
    m_inset_frac = frac;
    start_ticking();   // the presented camera eases to the new framing
}

void DiceView::set_right_inset(float px) {
    const float frac = get_width() > 0 ? std::max(0.0f, px) / float(get_width()) : 0.0f;
    if (std::fabs(frac - m_inset_right) < 1e-3f) return;
    m_inset_right = frac;
    start_ticking();
}

void DiceView::set_glow(const std::array<bool, 5>& glow) {
    if (glow == m_glow) return;
    m_glow = glow;
    start_ticking();
    queue_render();
}

// ── layout ──────────────────────────────────────────────────────────────────
DiceView::Pose DiceView::present_pose(int i) const {
    Pose p;
    p.pos = {(float(i) - 2.0f) * kRowGap, 0.5f, kRowZ};
    // The result stays on TOP, squared to the row (Scott, s004b: "the top
    // plane of the dice is what is used"); the camera looks down to read it.
    p.rot = gfx::show(m_dice[i] ? m_dice[i] : 1, {0, 1, 0}, 0.0f);
    return p;
}

// Straight on, a little above the row, far enough back that all five fit the
// view's width with room to spare (the view can be any shape).
DiceView::Cam DiceView::present_cam() const {
    // A drawer over the left of the view (the hints): fit the row to what is
    // still uncovered, and centre it there.
    const float cover = std::clamp(m_inset_frac, 0.0f, 0.6f);
    const float cover_r = std::clamp(m_inset_right, 0.0f, 0.6f);
    const float half = 2.0f * kRowGap + 0.5f + 0.55f;                 // the row, plus margin
    const float th = std::tan(kFov / 2);
    const float aspect = std::max(0.5f, m_aspect * (1.0f - cover - cover_r));
    const float dist = std::max(half / (th * aspect), 1.6f / th);
    const float el = m_present_el;                                      // ~46-54 deg: reading the tops
    const float shift = (cover - cover_r) * th * m_aspect * dist;      // world units at the row
    const Vec3 at{-shift, 0.5f, kRowZ + 0.15f};
    return {at + Vec3{0, std::sin(el), std::cos(el)} * dist, at};
}

// Shoot this roll's take (gfx/Take: the physics run to rest, landing on the
// drawn faces, and the camera planned over it), then draw the pace of what
// follows: the beat on the fallen dice, the glide, the presented height.
void DiceView::plan_roll(std::uint64_t seed) {
    gfx::Shot shot;
    shot.seed   = seed;
    shot.fov    = kFov;
    shot.aspect = m_aspect;
    for (int i = 0; i < 5; ++i) {
        shot.values[i] = m_dice[i];
        shot.held[i]   = m_held[i];
        shot.slots[i]  = present_pose(i).pos + Vec3{0, 0, kHeldFwd};
    }
    m_take = gfx::shoot(shot);
    const auto& end = m_take.frames.back();
    for (int i = 0; i < 5; ++i) m_scatter[i] = {end[i].pos, end[i].rot};
    const auto cam = m_take.cams.back();
    m_roll_cam = {cam.eye, cam.at};

    Rng r{seed ^ 0xA5A5A5A5ull};
    m_rest  = r.range(0.9f, 1.5f);
    m_glide = r.range(1.6f, 2.3f);
    m_present_el = r.range(0.80f, 0.94f);   // the presented height breathes a little too
    if (auto lg = log::get(log::Area::Shell))
        lg->info("take: {:.2f} s, rest {:.2f}, glide {:.2f}", m_take.duration(), m_rest, m_glide);
}

// Everything's place at time t. The Present phase eases toward its targets
// (so a hold slides a die forward); the roll phases follow the timeline.
void DiceView::update_poses(double t, double dt) {
    const float k = 1.0f - float(std::exp(-dt * kEase));
    for (int i = 0; i < 5; ++i) m_lift[i] += ((m_held[i] ? 1.0f : 0.0f) - m_lift[i]) * k;

    // Each check measures time in ITS OWN phase (s005 log: with one shared
    // elapsed time, a take longer than the rest skipped the rest entirely).
    const auto e_now = [&] { return t - m_phase_t0; };
    const auto note = [](const char* what, double at) {
        if (auto lg = log::get(log::Area::Shell)) lg->debug("dice view: {} at {:.3f}", what, at);
    };
    if (m_phase == Phase::Take && e_now() >= m_take.duration()) { m_phase = Phase::Rolled; m_phase_t0 = t; note("rest", t); }
    if (m_phase == Phase::Rolled && e_now() >= m_rest) { m_phase = Phase::Glide; m_phase_t0 = t; note("glide", t); }
    if (m_phase == Phase::Glide && e_now() >= m_glide) { m_phase = Phase::Present; m_settled_pending = true; note("presented", t); }
    const double pe = t - m_phase_t0;

    switch (m_phase) {
        case Phase::Present: {
            const Cam pc = present_cam();
            m_cam.eye = gfx::lerp(m_cam.eye, pc.eye, k);
            m_cam.at  = gfx::lerp(m_cam.at, pc.at, k);
            for (int i = 0; i < 5; ++i) {
                Pose target = present_pose(i);
                target.pos.z += kHeldFwd * m_lift[i];
                m_pose[i].pos = gfx::lerp(m_pose[i].pos, target.pos, k);
                m_pose[i].rot = gfx::slerp(m_pose[i].rot, target.rot, k);
            }
            break;
        }
        case Phase::Take: {
            // The take, played back: the physics' dice, the planned camera.
            const auto dice = m_take.dice_at(pe);
            for (int i = 0; i < 5; ++i) m_pose[i] = {dice[i].pos, dice[i].rot};
            const auto c = m_take.cam_at(pe);
            m_cam = {c.eye, c.at};
            break;
        }
        case Phase::Rolled: {
            // A breath of handheld drift while you look.
            const float d = float(pe);
            m_cam = m_roll_cam;
            m_cam.eye = m_cam.eye + Vec3{std::sin(d * 1.7f) * 0.06f, std::sin(d * 1.1f) * 0.04f, 0};
            for (int i = 0; i < 5; ++i) m_pose[i] = m_scatter[i];
            break;
        }
        case Phase::Glide: {
            const float u = gfx::ease_in_out(float(pe / m_glide));
            const Cam pc = present_cam();
            // The camera swings on an arc: direction slerped, distance lerped.
            const Vec3 o1 = m_roll_cam.eye - m_roll_cam.at, o2 = pc.eye - pc.at;
            const Quat turn = gfx::from_to(gfx::normalize(o1), gfx::normalize(o2));
            const Quat part = gfx::slerp(Quat{}, turn, u);
            const float len = gfx::length(o1) + (gfx::length(o2) - gfx::length(o1)) * u;
            m_cam.at  = gfx::lerp(m_roll_cam.at, pc.at, u);
            m_cam.eye = m_cam.at + gfx::normalize(part.rotate(o1)) * len;
            for (int i = 0; i < 5; ++i) {
                Pose target = present_pose(i);
                target.pos.z += kHeldFwd * m_lift[i];
                // Each die arcs at its own height, so crossing paths pass over
                // one another instead of through (s005 frames).
                const float lift = m_held[i] ? 0.0f : std::sin(u * kPi) * (0.35f + 0.18f * float(i));
                m_pose[i].pos = gfx::lerp(m_scatter[i].pos, target.pos, u) + Vec3{0, lift, 0};
                m_pose[i].rot = gfx::slerp(m_scatter[i].rot, target.rot, u);
            }
            // Dice slide into line AROUND each other, not through (s005 frames
            // showed paths crossing): any two at about the same height that come
            // closer than a die's width are eased apart on the felt. Held dice
            // stand firm. Fades out as they reach the row (spaced 1.32 anyway).
            {
                const float strength = 1.0f - gfx::smoothstep01((u - 0.75f) / 0.25f);
                for (int pass = 0; pass < 6 && strength > 0; ++pass)
                    for (int a = 0; a < 5; ++a)
                        for (int b = a + 1; b < 5; ++b) {
                            Vec3& pa = m_pose[a].pos;
                            Vec3& pb = m_pose[b].pos;
                            if (std::fabs(pa.y - pb.y) > 0.95f) continue;
                            Vec3 d{pa.x - pb.x, 0, pa.z - pb.z};
                            const float dl = gfx::length(d);
                            if (dl >= 1.08f) continue;
                            d = dl > 1e-4f ? d * (1.0f / dl) : Vec3{1, 0, 0};
                            const float gap = (1.08f - dl) * strength;
                            const float sa = m_held[a] ? 0.0f : m_held[b] ? 1.0f : 0.5f;
                            const float sb = m_held[b] ? 0.0f : m_held[a] ? 1.0f : 0.5f;
                            pa = pa + d * (gap * sa);
                            pb = pb - d * (gap * sb);
                        }
            }
            break;
        }
    }
}

// ── the clock ───────────────────────────────────────────────────────────────
void DiceView::start_ticking() {
    if (m_tick_id || m_failed) return;
    m_last = -1;
    m_tick_id = add_tick_callback(sigc::mem_fun(*this, &DiceView::tick));
}

bool DiceView::tick(const Glib::RefPtr<Gdk::FrameClock>& clock) {
    // YATZE_SPEED=n plays everything n times faster -- for testing only.
    static const double speed = [] {
        const char* e = std::getenv("YATZE_SPEED");
        const double v = e ? std::atof(e) : 1.0;
        return v > 0 ? v : 1.0;
    }();
    const double t = double(clock->get_frame_time()) / 1e6 * speed;
    const double dt = m_last < 0 ? 1.0 / 60 : std::min(0.1, t - m_last);
    m_last = t;
    if (m_phase_t0 < 0) {                 // a roll starts on the first frame after it arrives
        m_phase_t0 = t;
        if (auto lg = log::get(log::Area::Shell)) lg->debug("dice view: take starts at {:.3f}", t);
    }
    m_now = t;
    update_poses(t, dt);
    queue_render();
    if (m_settled_pending) { m_settled_pending = false; m_sig_settled.emit(); }

    // Keep ticking while anything moves (or glows).
    bool moving = m_phase != Phase::Present;
    const bool glowing = std::any_of(m_glow.begin(), m_glow.end(), [](bool b) { return b; });
    if (!moving) {
        const Cam pc = present_cam();
        if (gfx::length(m_cam.eye - pc.eye) > 1e-3f) moving = true;
        for (int i = 0; i < 5 && !moving; ++i) {
            Pose target = present_pose(i);
            target.pos.z += kHeldFwd * (m_held[i] ? 1.0f : 0.0f);
            if (gfx::length(m_pose[i].pos - target.pos) > 1e-3f) moving = true;
            if (std::fabs(m_lift[i] - (m_held[i] ? 1.0f : 0.0f)) > 1e-3f) moving = true;
        }
    }
    if (moving || glowing) return true;
    m_tick_id = 0;
    return false;
}

// ── picking: a ray from the eye through the pointer, against each die ──────
int DiceView::pick(double x, double y) const {
    if (!m_can_hold || m_phase != Phase::Present || get_width() <= 0 || get_height() <= 0) return -1;
    const float aspect = float(get_width()) / float(get_height());
    const float nx = float(2.0 * x / get_width() - 1.0), ny = float(1.0 - 2.0 * y / get_height());
    const Vec3 f = gfx::normalize(m_cam.at - m_cam.eye);
    const Vec3 s = gfx::normalize(gfx::cross(f, {0, 1, 0}));
    const Vec3 u = gfx::cross(s, f);
    const float th = std::tan(kFov / 2);
    const Vec3 dir = gfx::normalize(f + s * (nx * th * aspect) + u * (ny * th));
    int best = -1;
    float best_t = 1e9f;
    for (int i = 0; i < 5; ++i) {
        const Vec3 oc = m_cam.eye - m_pose[i].pos;
        const float b = gfx::dot(oc, dir), c = gfx::dot(oc, oc) - kPickR * kPickR;
        const float disc = b * b - c;
        if (disc < 0) continue;
        const float hit = -b - std::sqrt(disc);
        if (hit > 0 && hit < best_t) { best_t = hit; best = i; }
    }
    return best;
}

Mat4 DiceView::view_proj(const Cam& c, float aspect) const {
    return Mat4::perspective(kFov, aspect, 0.1f, 80.0f) * Mat4::look_at(c.eye, c.at, {0, 1, 0});
}

// ── GL lifetime ─────────────────────────────────────────────────────────────
void DiceView::on_realize() {
    Gtk::GLArea::on_realize();
    make_current();
    if (has_error() || !init_gl()) {
        m_failed = true;
        if (auto lg = log::get(log::Area::Shell)) lg->warn("dice view: OpenGL unavailable -- using the flat dice");
        m_sig_failed.emit();
        return;
    }
    if (auto lg = log::get(log::Area::Shell))
        lg->info("dice view: {} / {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                 reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
}

void DiceView::on_unrealize() {
    make_current();
    if (!has_error()) free_gl();
    Gtk::GLArea::on_unrealize();
}

bool DiceView::init_gl() {
    if (epoxy_gl_version() < 33) return false;
    std::string err;
    m_die_prog = link(kDieVS, kDieFS, err);
    if (!m_die_prog) { if (auto lg = log::get(log::Area::Shell)) lg->error("die shader: {}", err); return false; }
    m_flat_prog = link(kFlatVS, kFlatFS, err);
    if (!m_flat_prog) { if (auto lg = log::get(log::Area::Shell)) lg->error("felt shader: {}", err); return false; }

    // The face table, from the one place it is written down.
    glUseProgram(m_die_prog);
    const Vec3 axes[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int a = 0; a < 6; ++a)
        for (int v = 1; v <= 6; ++v)
            if (gfx::dot(gfx::face_normal(v), axes[a]) > 0.5f)
                glUniform1i(glGetUniformLocation(m_die_prog, ("u_face[" + std::to_string(a) + "]").c_str()), v);

    const auto mesh = gfx::rounded_cube();
    m_die_count = int(mesh.index.size());
    glGenVertexArrays(1, &m_die_vao);
    glBindVertexArray(m_die_vao);
    glGenBuffers(1, &m_die_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_die_vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(mesh.verts.size() * sizeof(gfx::Vertex)), mesh.verts.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &m_die_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_die_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(mesh.index.size() * 4), mesh.index.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(gfx::Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(gfx::Vertex), reinterpret_cast<void*>(3 * sizeof(float)));

    const float quad[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &m_quad_vao);
    glBindVertexArray(m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);

    GLint max_samples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    m_samples = std::clamp(max_samples, 0, 4);
    return glGetError() == GL_NO_ERROR;
}

void DiceView::free_gl() {
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_fbo_color) glDeleteRenderbuffers(1, &m_fbo_color);
    if (m_fbo_depth) glDeleteRenderbuffers(1, &m_fbo_depth);
    if (m_die_vbo) glDeleteBuffers(1, &m_die_vbo);
    if (m_die_ibo) glDeleteBuffers(1, &m_die_ibo);
    if (m_quad_vbo) glDeleteBuffers(1, &m_quad_vbo);
    if (m_die_vao) glDeleteVertexArrays(1, &m_die_vao);
    if (m_quad_vao) glDeleteVertexArrays(1, &m_quad_vao);
    if (m_die_prog) glDeleteProgram(m_die_prog);
    if (m_flat_prog) glDeleteProgram(m_flat_prog);
    m_fbo = m_fbo_color = m_fbo_depth = m_die_vbo = m_die_ibo = m_quad_vbo = 0;
    m_die_vao = m_quad_vao = m_die_prog = m_flat_prog = 0;
    m_fbo_w = m_fbo_h = 0;
}

void DiceView::ensure_fbo(int w, int h) {
    if (m_fbo && w == m_fbo_w && h == m_fbo_h) return;
    if (!m_fbo) {
        glGenFramebuffers(1, &m_fbo);
        glGenRenderbuffers(1, &m_fbo_color);
        glGenRenderbuffers(1, &m_fbo_depth);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, m_fbo_color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_RGBA8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, m_fbo_depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_DEPTH_COMPONENT24, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_fbo_color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_fbo_depth);
    m_fbo_w = w;
    m_fbo_h = h;
}

// ── a frame ─────────────────────────────────────────────────────────────────
bool DiceView::on_render(const Glib::RefPtr<Gdk::GLContext>&) {
    if (m_failed) return false;
    GLint target = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);   // GTK's framebuffer
    const int scale = get_scale_factor();
    const int w = std::max(1, get_width() * scale), h = std::max(1, get_height() * scale);
    ensure_fbo(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, w, h);
    glClearColor(0.025f, 0.075f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const Mat4 vp = view_proj(m_cam, float(w) / float(h));
    if (std::fabs(m_aspect - float(w) / float(h)) > 1e-3f) {
        m_aspect = float(w) / float(h);   // the presented camera refits; ease there
        start_ticking();
    }

    // The felt.
    glUseProgram(m_flat_prog);
    set_mat(m_flat_prog, "u_vp", vp);
    glBindVertexArray(m_quad_vao);
    glDisable(GL_CULL_FACE);   // the quad's winding is not worth caring about
    glUniform1i(glGetUniformLocation(m_flat_prog, "u_mode"), 0);
    set_vec(m_flat_prog, "u_center", {0, 0, 0});
    glUniform1f(glGetUniformLocation(m_flat_prog, "u_size"), 60.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // On the felt: shadows, hold rings, the hint's glow.
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    // Colour blends; the frame's ALPHA stays 1. (GTK composites the GLArea with
    // its alpha: a blended alpha below 1 let the window show through and the
    // shadows came out lighter than the felt -- s004's bug.)
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    const auto decal = [&](const Vec3& c, float size, int mode, const Vec3& color, float strength) {
        glUniform1i(glGetUniformLocation(m_flat_prog, "u_mode"), mode);
        set_vec(m_flat_prog, "u_center", {c.x, 0.004f, c.z});
        glUniform1f(glGetUniformLocation(m_flat_prog, "u_size"), size);
        set_vec(m_flat_prog, "u_color", color);
        glUniform1f(glGetUniformLocation(m_flat_prog, "u_strength"), strength);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };
    for (int i = 0; i < 5; ++i) {
        const Vec3 p = m_pose[i].pos;
        const float height = std::max(0.0f, p.y - 0.5f);
        const Vec3 at = p - Vec3{kLight.x, 0, kLight.z} * (0.22f + height * 0.4f);
        decal(at, 0.85f + height * 0.35f, 1, {}, 0.55f / (1.0f + height * 0.8f));
    }
    for (int i = 0; i < 5; ++i)
        if (m_lift[i] > 0.02f)
            decal(m_pose[i].pos, 0.98f, 2, {0.25f, 0.55f, 1.0f}, 0.85f * m_lift[i]);
    if (m_phase == Phase::Present) {
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);   // the glow adds light
        const float pulse = 0.55f + 0.30f * std::sin(float(m_now) * 4.0f);
        for (int i = 0; i < 5; ++i)
            if (m_glow[i]) decal(m_pose[i].pos, 0.90f, 2, {1.0f, 0.70f, 0.22f}, pulse);
    }
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    // The dice.
    glEnable(GL_CULL_FACE);
    glUseProgram(m_die_prog);
    set_mat(m_die_prog, "u_vp", vp);
    set_vec(m_die_prog, "u_eye", m_cam.eye);
    set_vec(m_die_prog, "u_light", kLight);
    glBindVertexArray(m_die_vao);
    // Red: classic casino red, white pips, glossy. Ivory: warm white, black pips.
    const bool red = m_red;
    const Vec3 body_c = red ? Vec3{0.72f, 0.05f, 0.06f} : Vec3{0.97f, 0.95f, 0.89f};
    const Vec3 pip_c  = red ? Vec3{0.97f, 0.96f, 0.93f} : Vec3{0.07f, 0.07f, 0.09f};
    set_vec(m_die_prog, "u_pip", pip_c);
    glUniform1f(glGetUniformLocation(m_die_prog, "u_gloss"), red ? 0.65f : 0.40f);
    for (int i = 0; i < 5; ++i) {
        const bool blank = m_dice[i] == 0;
        Vec3 body = blank ? body_c * 0.72f : body_c;   // not rolled yet: a little dim, no pips
        if (i == m_hover) body = body * 1.06f;
        glUniform1f(glGetUniformLocation(m_die_prog, "u_blank"), blank ? 1.0f : 0.0f);
        set_vec(m_die_prog, "u_body", body);
        set_mat(m_die_prog, "u_model", Mat4::translate(m_pose[i].pos) * Mat4::rotate(m_pose[i].rot));
        glDrawElements(GL_TRIANGLES, m_die_count, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);

    // Resolve the multisampled frame into GTK's.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(target));
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(target));
    return true;
}

}  // namespace yatze
