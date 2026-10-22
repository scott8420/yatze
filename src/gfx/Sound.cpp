#include "gfx/Sound.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace yatze::gfx {

namespace {

constexpr float kPi = 3.14159265f;
constexpr int   kRate = Audio::kRate;

struct Rng {
    std::uint64_t s;
    float next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return float((z ^ (z >> 31)) >> 40) / float(1ull << 24);
    }
    float range(float a, float b) { return a + (b - a) * next(); }
    float noise() { return next() * 2.0f - 1.0f; }
};

// A damped partial: amplitude, frequency, decay (seconds to 1/e).
struct Partial { float amp, freq, decay; };

// Render one knock (mono) into `out` (length decides the tail).
void knock(std::vector<float>& out, Contact::Kind kind, float pitch, Rng& r, bool muffled) {
    std::vector<Partial> parts;
    float noise_amp = 0, noise_decay = 0.0015f, lowpass = 1.0f;
    float len = 0.04f;
    switch (kind) {
        case Contact::Kind::Die:     // two hard plastic corners: a bright, short click
            parts = {{0.50f, 2400 * pitch, 0.007f}, {0.35f, 3700 * pitch, 0.005f}, {0.25f, 5300 * pitch, 0.003f}};
            noise_amp = 0.60f; noise_decay = 0.0012f; len = 0.035f;
            break;
        case Contact::Kind::Felt:    // a die landing on cloth: a soft thud, a faint corner tick
            parts = {{0.35f, 110 * pitch, 0.025f}, {0.15f, 1800 * pitch, 0.003f}};
            noise_amp = 1.40f; noise_decay = 0.009f; lowpass = 0.08f; len = 0.07f;
            break;
        case Contact::Kind::Rail:    // the wooden rail: a hollow knock
            parts = {{0.60f, 380 * pitch, 0.030f}, {0.35f, 820 * pitch, 0.018f}, {0.20f, 1650 * pitch, 0.009f}};
            noise_amp = 0.50f; noise_decay = 0.002f; len = 0.09f;
            break;
    }
    if (muffled) { lowpass = std::min(lowpass, 0.12f); for (auto& p : parts) p.amp *= 0.6f; }
    const int n = int(len * kRate);
    out.assign(std::size_t(n), 0.0f);
    float lp = 0;
    for (int i = 0; i < n; ++i) {
        const float t = float(i) / kRate;
        float x = 0;
        for (const auto& p : parts) x += p.amp * std::sin(2 * kPi * p.freq * t) * std::exp(-t / p.decay);
        lp += lowpass * (r.noise() - lp);   // one-pole lowpass on the noise
        x += noise_amp * lp * std::exp(-t / noise_decay);
        // A 1 ms fade-in so the start never clicks digitally.
        x *= std::min(1.0f, float(i) / (0.001f * kRate));
        out[std::size_t(i)] = x;
    }
}

}  // namespace

Audio mix(const std::vector<Contact>& contacts, const std::function<CamPose(double)>& cam_at,
          float fov, float aspect, double clip_seconds, std::uint64_t seed, double rattle_until, Vec3 rattle_at) {
    Rng r{seed ? seed : 1};
    const std::size_t frames = std::size_t(std::max(0.0, clip_seconds) * kRate);
    std::vector<float> left(frames, 0.0f), right(frames, 0.0f);
    std::vector<float> one;

    auto place = [&](double t, const Vec3& pos, float gain, Contact::Kind kind, float pitch, bool muffled) {
        const CamPose c = cam_at(t);
        const Vec3 f = normalize(c.at - c.eye);
        const Vec3 s = normalize(cross(f, {0, 1, 0}));
        const float dist = std::max(1.0f, length(pos - c.eye));
        // Where it is across the screen: -1 left edge .. +1 right edge.
        const float depth = std::max(0.5f, dot(pos - c.eye, f));
        float pan = dot(pos - c.eye, s) / (depth * std::tan(fov / 2) * std::max(0.4f, aspect));
        pan = std::clamp(pan, -1.0f, 1.0f) * 0.85f;
        const float near = std::clamp(8.0f / dist, 0.45f, 1.25f);
        const float g = gain * near;
        const float gl = g * std::cos((pan + 1) * kPi / 4), gr = g * std::sin((pan + 1) * kPi / 4);   // equal power
        knock(one, kind, pitch, r, muffled);
        const std::size_t at = std::size_t(std::max(0.0, t) * kRate);
        for (std::size_t i = 0; i < one.size() && at + i < frames; ++i) {
            left[at + i]  += one[i] * gl;
            right[at + i] += one[i] * gr;
        }
    };

    // The cup: muffled clicks while it is shaken, bunched on each shake,
    // swelling as it tips and the dice slide to the mouth.
    if (rattle_until > 0.0) {
        const double beat = 1.0 / r.range(7.5f, 10.0f);
        for (double t0 = 0.02; t0 < rattle_until; t0 += beat) {
            const float u = float(t0 / rattle_until);
            const int clicks = 1 + int(r.next() * 3.0f) + int(u * 2.0f);
            for (int k = 0; k < clicks; ++k)
                place(t0 + double(r.range(0.0f, 0.05f)), rattle_at + Vec3{r.range(-0.6f, 0.6f), 0, 0},
                      r.range(0.10f, 0.20f) * (0.7f + 0.5f * u), Contact::Kind::Die, r.range(0.62f, 0.8f), true);
        }
    }
    for (const auto& c : contacts) {
        // Harder knocks are louder, and not linearly: a light touch is quiet.
        const float g = std::pow(std::clamp(c.speed / 9.0f, 0.06f, 1.0f), 1.25f);
        place(c.t, c.pos, g, c.kind, r.range(0.92f, 1.09f), false);
    }

    // Master gain and a soft limiter: a pile-up never clips.
    Audio a;
    a.pcm.resize(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
        const float l = std::tanh(left[i] * 1.1f) * 0.92f, rr = std::tanh(right[i] * 1.1f) * 0.92f;
        a.pcm[2 * i]     = std::int16_t(std::lround(l * 32767.0f));
        a.pcm[2 * i + 1] = std::int16_t(std::lround(rr * 32767.0f));
    }
    return a;
}

Audio mix_take(const Take& take, float fov, float aspect, std::uint64_t seed) {
    return mix(take.contacts, [&take](double t) { return take.cam_at(t); }, fov, aspect,
               take.duration() + 0.35, seed, take.spill, take.cup_at(take.spill * 0.5).pos);
}

Audio chime(Cheer kind) {
    // Notes as (start s, frequency Hz, length s, gain).
    struct Note { double t; float f, len, g; };
    std::vector<Note> notes;
    if (kind == Cheer::Yatze) {
        const float f[] = {1046.5f, 1318.5f, 1568.0f, 2093.0f, 1568.0f, 2093.0f};
        for (int i = 0; i < 6; ++i) notes.push_back({0.085 * i, f[i], i == 5 ? 1.2f : 0.5f, i == 5 ? 0.30f : 0.22f});
    } else {
        const float f[] = {523.25f, 659.25f, 783.99f};
        for (int i = 0; i < 3; ++i) notes.push_back({0.16 * i, f[i], 0.6f, 0.24f});
        for (float c : {1046.5f, 1318.5f, 1568.0f}) notes.push_back({0.52, c, 1.8f, 0.17f});
    }
    double end = 0;
    for (const auto& n : notes) end = std::max(end, n.t + n.len);
    const std::size_t frames = std::size_t((end + 0.15) * kRate);
    std::vector<float> mono(frames, 0.0f);
    // A small bell: a struck partial set (1, 2, 2.76, 5.4) dying away, the
    // higher ones faster; a 4 ms attack so it doesn't click.
    const float part[4][2] = {{1.0f, 1.0f}, {2.0f, 0.45f}, {2.76f, 0.30f}, {5.4f, 0.12f}};
    for (const auto& n : notes) {
        const std::size_t at = std::size_t(n.t * kRate), len = std::size_t(n.len * kRate);
        for (std::size_t i = 0; i < len && at + i < frames; ++i) {
            const float t = float(i) / kRate;
            const float attack = std::min(1.0f, t / 0.004f);
            float s = 0;
            for (const auto& p : part)
                s += std::sin(2 * kPi * n.f * p[0] * t) * p[1] * std::exp(-t * (2.2f + 3.5f * p[0]) / n.len);
            mono[at + i] += s * n.g * attack;
        }
    }
    Audio a;
    a.pcm.resize(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
        const auto v = std::int16_t(std::lround(std::tanh(mono[i] * 1.2f) * 0.85f * 32767.0f));
        a.pcm[2 * i] = a.pcm[2 * i + 1] = v;
    }
    return a;
}

std::vector<std::uint8_t> wav(const Audio& a) {
    const std::uint32_t data = std::uint32_t(a.pcm.size() * 2);
    std::vector<std::uint8_t> out(44 + data);
    auto put32 = [&](std::size_t at, std::uint32_t v) { for (int k = 0; k < 4; ++k) out[at + std::size_t(k)] = std::uint8_t(v >> (8 * k)); };
    auto put16 = [&](std::size_t at, std::uint16_t v) { out[at] = std::uint8_t(v); out[at + 1] = std::uint8_t(v >> 8); };
    std::memcpy(out.data(), "RIFF", 4);
    put32(4, 36 + data);
    std::memcpy(out.data() + 8, "WAVEfmt ", 8);
    put32(16, 16);                      // fmt chunk size
    put16(20, 1);                       // PCM
    put16(22, 2);                       // stereo
    put32(24, Audio::kRate);
    put32(28, Audio::kRate * 4);        // byte rate
    put16(32, 4);                       // block align
    put16(34, 16);                      // bits
    std::memcpy(out.data() + 36, "data", 4);
    put32(40, data);
    std::memcpy(out.data() + 44, a.pcm.data(), data);   // little-endian host (x86 / ARM)
    return out;
}

}  // namespace yatze::gfx
