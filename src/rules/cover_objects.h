#pragma once

// Cover from objects (#166 part 2; owner, 2026-11-23: "A, and apply cover to all three"). The session showed the
// engine's own sight line knows only the tileset's walls and shut doors ("from all those, the only one that blocks LoS
// is the closed door"), so nwn5e tests its own lines against the placeables: each one's box - its model's visible
// bounds (data\placeable_bounds.txt, made at build time from the player's game: tools/data/placeable_bounds.js),
// turned by its facing, scaled, at its place. A line is blocked when the engine's line is or a box is in the way. The
// cover degree from the six body points `!los` uses (rules/los_probe.h). This file only decides.

#include <string>

#include "rules/cover.h"
#include "rules/los_probe.h"

namespace rules {

// A model's visible geometry's bounds in model space (metres).
struct ModelBounds {
  float lo[3] = {0, 0, 0};
  float hi[3] = {0, 0, 0};
};

// One line of data\placeable_bounds.txt - "<placeables.2da row> <model> <x0> <x1> <y0> <y1> <z0> <z1> [walk ...]" -
// read; false for a comment ("#"), an empty line or a bad one.
bool ParseBoundsLine(const std::string& line, int* row, ModelBounds* bounds);

// A placeable counts as an obstacle when its visible geometry is this tall at least: smaller ones - a candle, a book,
// an invisible marker's sliver - cover no one.
constexpr float kCoverMinHeight = 0.3f;
bool BoundsCount(const ModelBounds& bounds);

// A placeable's box in the area: its model's bounds times `scale`, turned so the model's +Y is the object's facing
// (fx, fy) - as a creature's model faces its facing - at (x, y, z).
struct ObjectBox {
  float centre[3] = {0, 0, 0};
  float half[3] = {0, 0, 0};
  float ax = 1, ay = 0;  // the model's +X in the area
  float bx = 0, by = 1;  // its +Y (the facing)
  float lo[3] = {0, 0, 0};  // the box's own bounds in the area (the cull)
  float hi[3] = {0, 0, 0};
};
ObjectBox BoxOf(const ModelBounds& bounds, float x, float y, float z, float fx, float fy, float scale);

// Whether the segment from `a` to `b` passes through the box (its surface counts); `t` (optional): where it enters,
// 0 at `a` to 1 at `b`.
bool SegmentHitsBox(const float a[3], const float b[3], const ObjectBox& box, float* t = nullptr);

// Whether a point is inside the box (its surface counts).
bool PointInBox(const float p[3], const ObjectBox& box);

// The cull: whether the segment's own bounds overlap the box's.
bool SegmentNearBox(const float a[3], const float b[3], const ObjectBox& box);

// The cover degree from how many of `points` body points' lines are blocked (SRD 5.2.1: Half Cover "an object that
// covers at least half of the target", Three-Quarters "at least three-quarters", Total "the whole target"): Half from
// half the points (rounded up), Three-Quarters from three-quarters of them (rounded to the nearest, halves up), Total
// all. Six: 3 / 5 / 6 (as before); seven: 4 / 5 / 7.
Cover CoverFromBlocked(int blocked, int points = kLosBodyPoints);

// #166 (owner, 2026-11-23: "Your recommendations are good"): an observer's eyes over its feet by its size, for nwn5e's
// own sight and cover lines (the engine's perception line stays at 1 m): Tiny 0.3, Small 0.9, Medium 1.6, Large 2.5,
// Huge 4 (an unknown size: Medium).
float EyeHeight(int nwn_size);
// The cones' slices (core/stealth_watch.cpp) end at a crouched hider's shoulders over the ground (Medium 0.89 m): the
// rule's own line - its body's three-quarters point - so a slice cut there is about the Three-Quarters Cover that
// hides it.
float SliceTargetHeight(int hider_size);

// The 2024 Hide condition: behind Three-Quarters or Total Cover from a creature, a hider can't be seen by it.
bool CoverHides(Cover cover);

// A Dexterity save's bonus from cover from the effect's point of origin: Half +2, Three-Quarters +5; Total Cover +5
// too (an area that reaches it at all: nwn5e's reading - the owner may rule otherwise).
int CoverSaveBonus(Cover cover);
// A spell whose save ignores cover (SRD 5.2.1 Sacred Flame: "The target gains no benefit from Half Cover or
// Three-Quarters Cover for this save"; nwn5e's served row 1054).
constexpr int kSpellSacredFlame = 1054;
bool SpellIgnoresCover(int spell);

// Total Cover: no direct attack. A ranged attack is refused wherever the attacker stands; a melee one once the
// attacker is within reach (`metres` <= kMeleeCoverReach) - farther out it walks on, and the cover may be gone when it
// gets there.
constexpr float kMeleeCoverReach = 3.0f;
bool CoverRefusesAttack(Cover cover, bool ranged, float metres);

// Where an area's lines start: rules/area_origin.h (#166 decision 2).

}  // namespace rules
