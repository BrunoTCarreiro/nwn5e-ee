#pragma once

// A refused attacker repositions (#166 decision 3; owner, 2026-11-23: "Your recommendations are good"): an AI attacker
// whose attack Total Cover refuses moves instead of trying again in place - a ranged one steps sideways (up to
// kSidestepMetres) to a spot its own sight test finds a clear line from, else closes in; a melee one walks round the
// obstacle to its target. Queued once (core/object_cover.cpp); player characters are only told. This file only
// decides.

#include <vector>

namespace rules {

constexpr float kSidestepMetres = 5.0f;

// A spot to try, metres from the attacker.
struct Sidestep {
  float x = 0, y = 0;
};
// The spots a ranged attacker at (ax, ay) tries for a line to its target at (tx, ty): square to the line between them,
// 1.5, 3 and 4.5 m to either side, nearest first, the left one first at each distance.
std::vector<Sidestep> SidestepSpots(float ax, float ay, float tx, float ty);

enum class Reposition { kSidestep, kCloseIn, kWalkRound };
// What it does: melee - walk round; ranged - the first spot with a line (`clear`, as SidestepSpots' order; `*pick` its
// index), else close in.
Reposition ChooseReposition(bool ranged, const std::vector<bool>& clear, int* pick);

}  // namespace rules
