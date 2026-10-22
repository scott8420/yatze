#pragma once
// gfx::Sound -- the sound of a take (s009). GTK-free; the selftest listens.
//
// The take already knows every knock (Take::contacts: when, where, how hard,
// on what). So the whole roll's sound is MIXED IN ONE GO, before it plays,
// into a short stereo clip that starts with the picture -- in step with it by
// construction, no timers to drift.
//
// Each knock is synthesised (no sound files): die on die a bright click, die
// on felt a soft cloth thud, die on a rail a hollow wooden knock; a muffled
// cup rattle as the dice spill out. Loudness follows the impact speed and the
// distance from the camera; each knock is panned by where it lands on screen;
// pitch wanders a little so no two knocks are identical. A soft limiter keeps
// a pile-up from clipping.
#include "gfx/Take.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace yatze::gfx {

struct Audio {
    static constexpr int kRate = 44100;
    std::vector<std::int16_t> pcm;      // interleaved stereo: L R L R ...
    double seconds() const { return double(pcm.size()) / 2.0 / kRate; }
};

// The general mixer: knocks, the camera over time (for panning and distance),
// the clip's length, a seed, and the cup's rattle: from 0 to `rattle_until`
// seconds (0 = none), at `rattle_at` (s010: while the cup is shaken).
Audio mix(const std::vector<Contact>& contacts, const std::function<CamPose(double)>& cam_at,
          float fov, float aspect, double length, std::uint64_t seed,
          double rattle_until = 0.0, Vec3 rattle_at = {});

// A take's sound: its contacts, its own camera, a cup rattle, a short tail.
Audio mix_take(const Take& take, float fov, float aspect, std::uint64_t seed);

// s012: the cheers. A Yatzé: a quick rising arpeggio of small bells. Game
// over: a slower one that ends on a held chord. Made in code, like the knocks.
enum class Cheer { Yatze, GameOver };
Audio chime(Cheer kind);

// A complete WAV file (16-bit PCM, stereo, 44.1 kHz) in memory.
std::vector<std::uint8_t> wav(const Audio& a);

}  // namespace yatze::gfx
