#pragma once

// An area effect (#161): a vfx_persistent.2da row - its shape and size, up to three effect models copied across it, a
// low-detail model for each, an effect for the whole duration, its sounds. The dev layer's !vfxarea points the editor's
// preview row (199) at one while the game runs; the VFX Bench's area mode designs and saves them.
//
// How the game uses the row (EE 89.8193.37-17, the #161 research): the server reads the shape and size for each new
// area; the client copies the models, counts (NUMACT), lifetimes (DURATION, ms), edge weights (EDGEWGHT, 0..1) and the
// one-shot sound for each new area, all copies alive together, each re-placed at a new random spot after its lifetime
// (0: it stays). The client divides by (int)(size x 100) to place a copy, so a tiny area would crash the game: the
// sizes are clamped here.

#include <string>
#include <utility>
#include <vector>

namespace data {

struct VfxAreaSlot {
  std::string model;      // empty: the slot is off
  int count = 0;          // NUMACT: copies alive together, 1..30
  float lifetime = 0.0f;  // seconds a copy stays before it's re-placed (0: never), 0..60
  float edge = 0.0f;      // EDGEWGHT: the share of copies placed on the edge, 0..1
  std::string low_model;  // MODELMIN: shown when high-quality effects are off (empty: the model)
};

struct VfxArea {
  std::string name;
  char shape = 'C';  // 'C' a circle (radius), 'R' a rectangle (width x length, the length along the facing)
  float radius = 1.0f;
  float width = 1.0f;
  float length = 1.0f;
  VfxAreaSlot slots[3];
  std::string duration_effect;  // DurationVFX: a visualeffects.2da row number, or empty
  std::string sound_impact, sound_duration, sound_cessation, sound_oneshot;
  float oneshot_chance = 0.0f;  // SoundOneShotPercentage, 0..1
  bool orient_ground = false;   // OrientWithGround: copies tilted to the ground
};

constexpr float kVfxAreaMinRadius = 0.6f;
constexpr float kVfxAreaMinSide = 0.05f;
constexpr float kVfxAreaMaxSize = 30.0f;
constexpr int kVfxAreaMaxCount = 30;
constexpr float kVfxAreaMaxLifetime = 60.0f;

// The row's columns, in vfx_persistent.2da's names, "****" for a blank. The shape's unused sizes are blank; a slot with
// no model is blank throughout. Out-of-range values are clamped: circle radius 0.6-30 m, width and length 0.05-30 m
// (not a number: the minimum), count 1-30, lifetime 0-60 s (written in ms), edge 0-1, the one-shot chance 0-1.
std::vector<std::pair<std::string, std::string>> VfxAreaRowColumns(const VfxArea& area);

}  // namespace data
