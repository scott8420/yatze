#pragma once
// gfx::Take -- a roll, filmed (s005). GTK-free and GL-free; the selftest
// shoots hundreds of takes headless.
//
// Scott: "like sitting and watching the dice roll out of the cup" and "you
// should be able to buffer the animation". So a roll is shot in full BEFORE
// anything is shown:
//
// 1. THE PHYSICS. The rolled dice leave a cup at your side of the table --
//    tossed, spinning -- and tumble across the felt: gravity, bounces,
//    friction, dice knocking into each other, invisible rails far enough out
//    that they rarely matter. Fixed 240 Hz steps, seeded, so a take replays
//    exactly. It runs until every die is still (a hard throw skitters longer
//    than a soft one), then each die settles flat.
//
// 2. THE RESULT. The game already drew the faces. The physics lands each die
//    with SOME face up; the die is then relabelled -- turned in its own frame
//    by one of the cube's own symmetries, so it looks and moves exactly the
//    same -- so that the face that ends on top is the drawn one. The take
//    keeps its look; the result is the game's.
//
// 3. THE CAMERA, planned knowing the ending. A seated viewpoint (a lean,
//    a height from low across the felt to nose-down, drawn per take), then a
//    slow drift and push-in over the take; the target follows the dice a
//    little AHEAD (the track is smoothed both ways in time) and the distance
//    is fitted to the spread the dice are about to reach, so it never crops.
//
// Held dice do not roll: they wait at their slots at the front, result up.
//
// 4. THE CUP (s010). The take opens with the cup at your side, shaken and
//    tipping (the rattle plays here), the dice inside it; at the SPILL they
//    leave its mouth and the physics begins; the cup follows through and is
//    lifted up out of the shot.
#include "gfx/Math.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace yatze::gfx {

struct DiePose { Vec3 pos; Quat rot; };
struct CamPose { Vec3 eye, at; };

// One knock during the take (s009: the sounds are mixed from these).
struct Contact {
    enum class Kind { Felt, Die, Rail };
    double t;        // seconds into the take
    Vec3   pos;      // where (world)
    float  speed;    // impact speed along the contact normal (units / s)
    Kind   kind;
    int    die;      // which die knocked
};

// The cup: an open cylinder, centred on `pos`, its axis (base -> mouth) the
// rotated +Y.
inline constexpr float kCupRadius = 1.5f, kCupHeight = 3.2f;
struct CupPose { Vec3 pos; Quat rot; bool shown; Vec3 axis() const { return rot.rotate({0, 1, 0}); } };

struct Take {
    static constexpr double kFps = 60.0;
    std::vector<std::array<DiePose, 5>> frames;   // at kFps
    std::vector<CamPose>                cams;     // one per frame
    std::vector<Contact>                contacts; // every audible knock, in time order
    std::vector<CupPose>                cups;     // one per frame (s010)
    double                              spill = 0;  // the dice leave the cup (before it, rolled dice are inside)
    double duration() const { return frames.empty() ? 0.0 : double(frames.size() - 1) / kFps; }
    // Interpolated at time t (clamped to the take).
    std::array<DiePose, 5> dice_at(double t) const;
    CamPose                cam_at(double t) const;
    CupPose                cup_at(double t) const;
};

struct Shot {
    std::array<int, 5>   values{};      // the drawn faces (1..6)
    std::array<bool, 5>  held{};        // held dice do not roll
    std::array<Vec3, 5>  slots{};       // where held dice wait (centre, y = 0.5)
    std::uint64_t        seed = 1;
    float                fov = 30.0f * 3.14159265f / 180.0f;   // vertical, radians
    float                aspect = 1.6f;
};

Take shoot(const Shot& shot);

// The table the dice stay on (the rails), for the view and the selftest.
// s009: the tray is DRAWN -- a padded back wall and two side rails, their
// inner faces exactly on these planes; your side is open (the front plane
// only keeps a stray die from rolling into your lap, and makes no sound).
inline constexpr float kRailX = 3.7f, kRailBack = -3.5f, kRailFront = 1.05f;
inline constexpr float kBackWallHeight = 2.4f, kSideRailHeight = 1.4f, kRailThick = 0.5f;   // s019: a touch taller

}  // namespace yatze::gfx
