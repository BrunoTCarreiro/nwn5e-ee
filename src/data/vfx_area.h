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

// The area draft (.area, Task 2 of #161): the VFX Bench's area mode keeps an area as text, and its twin
// (nwn5e-tools vfx_bench/public/area.js) writes the same bytes. `key = value` lines after kVfxAreaDraftHeader, in order:
//   area = <name>                                      always
//   shape = R, width = <m>, length = <m>               a rectangle; else radius = <m>
//   slot <k> = <model> <copies> <lifetime s> <edge> [<low-detail model>]   k 1-3, each slot with a model
//   duration_effect = <visualeffects row>, sound_impact / sound_duration / sound_cessation = <sound>,
//   sound_oneshot = <sound> <chance 0-1>, orient_ground = 1                when set
// Numbers as %g (at most 4 decimals), clamped as the row's columns are. Names are 1-16 of A-Z a-z 0-9 _.
extern const char* const kVfxAreaDraftHeader;
std::string WriteVfxAreaDraft(const VfxArea& area);
// False with `err` ("line <n>: ...", or "no area name") for a malformed line, an unknown key, a slot past 3 or a bad
// name; out-of-range numbers are clamped, not refused. Blank lines and # comments are skipped; CRLF is read as LF.
bool ReadVfxAreaDraft(const std::string& text, VfxArea* area, std::string* err);

// The VFX Bench's Play in game for an area (Task 5): the area's draft, then each slot's effect draft after a line
// "### slot <k>" and a low-detail effect after "### low <k>" (k 1-3); a "request = ..." line before the area is skipped.
// `parts`: ("slot 1", its text), ... in order. False with `err` for a part past the slots or an area part with nothing in it.
bool SplitVfxAreaRequest(const std::string& text, std::string* area_text, std::vector<std::pair<std::string, std::string>>* parts,
                         std::string* err);

}  // namespace data
