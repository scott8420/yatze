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
#include "gfx/Math.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace yatze::gfx {

struct DiePose { Vec3 pos; Quat rot; };
struct CamPose { Vec3 eye, at; };

struct Take {
    static constexpr double kFps = 60.0;
    std::vector<std::array<DiePose, 5>> frames;   // at kFps
    std::vector<CamPose>                cams;     // one per frame
    double duration() const { return frames.empty() ? 0.0 : double(frames.size() - 1) / kFps; }
    // Interpolated at time t (clamped to the take).
    std::array<DiePose, 5> dice_at(double t) const;
    CamPose                cam_at(double t) const;
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
inline constexpr float kRailX = 3.7f, kRailBack = -3.5f, kRailFront = 1.05f;

}  // namespace yatze::gfx
