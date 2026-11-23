#pragma once

// The sight-line probe (#166 part 2's research step; owner, 2026-11-23: "How do vision cones interact with objects?
// Placeables, meshes that are part of the tileset, half cover..." - "Start the research and fix the hall map to have
// more obstacles that will allow the testing of the cones with different covers"): `!los` runs the engine's own sight
// line (CNWSArea::ClearLineOfSight, as perception calls it, from the observer's eyes) to points of the speaker's body
// - its feet, waist, chest, head and shoulders - and says which are clear and what blocks the others
// (core/test_commands.cpp). This file only decides the points and the summary.

#include <string>
#include <vector>

namespace rules {

// A point of the target's body: `side` metres to the observer's left of the line from the observer (negative: to its
// right), `height` metres over the target's feet.
struct LosBodyPoint {
  const char* key;  // its name's catalogue key part (data/text/en.txt test_commands.los_<key>)
  float side;
  float height;
};
constexpr int kLosBodyPoints = 6;
extern const LosBodyPoint kLosBody[kLosBodyPoints];  // feet 0.2, waist 1.0, chest 1.3, head 1.7, shoulders 0.3 m, 1.4

// Where a body point is: the target at (tx, ty, tz), seen from (ox, oy).
void LosPointAt(const LosBodyPoint& point, float ox, float oy, float tx, float ty, float tz, float* x, float* y,
                float* z);

// One point's result, its words already in the player's language.
struct LosPointResult {
  std::string name;     // "head"
  bool clear = false;
  std::string blocker;  // what blocked it ("plc_bookshelf", "the tileset"), "" when clear
};

// The chat line's points: those with the same result together, in the body's order of first appearance -
// "feet, waist blocked by plc_table; chest, head, left shoulder, right shoulder clear". `clear_words` and
// `blocked_words` are the catalogue's "{points} clear" and "{points} blocked by {blocker}".
std::string LosSummary(const std::vector<LosPointResult>& points, const std::string& clear_words,
                       const std::string& blocked_words);

}  // namespace rules
