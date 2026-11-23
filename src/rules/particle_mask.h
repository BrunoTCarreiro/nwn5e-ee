#pragma once

// A spell's particle mask (#184; the owner, 2026-11-23: "is there a way to make a "mask" effect? an effect that makes
// particles from other effects disappear", then "run the research spike now", "maskproof seems good!" and "wire it,
// lets test it out"). An emitter whose texture is one of nwn5e's masked materials (data\files\n5_pmask.mtr, the
// spells' n5x_*.mtr: tools/data/spell_mask.js) draws with fsn5pmask.shd - the game's own particle fragment shader, read
// from the player's game at build time, with a mask added (tools/data/particle_mask.js). The mask drops a fragment
// - past the wall in its direction from the effect's point of origin: the walls measured in a ring of directions round
//   it (the engine's sight line, core/spell_mask.cpp), as far as each reaches across the ground;
// - or when the line from the origin to it passes through an obstacle's box: inside the box or in its shadow, as the
//   area's own cover rule draws it (rules/cover_objects.h);
// and only within its reach of the origin across the ground - another masked effect farther off draws whole.
// The shader reads the mask from EE's per-player scriptable uniforms (NWScript SetShaderUniformVec; GLSL
// scriptableVec1..16): this packs them, and ParticleMasked is the shader's own test, step for step, on the CPU.
//
// The uniforms (16 vectors, every one sent - a slot left from an older mask would read as part of this one):
// - scriptableVec1: the origin (x, y, z: the area's metres) and the reach (w, metres across the ground; 0 = off, every
//   uniform's default - then the shader draws exactly as the game's own).
// - scriptableVec2..9: the ring, 4 directions a vector (direction k in slot 2 + k / 4, component k % 4; direction k at
//   k x 360 / 32 degrees, anticlockwise from the area's +X): each its wall's nearness, 1 / metres (the wall measured,
//   plus kMaskWallMargin) - 0 when no wall is within the reach. Between two directions the nearness is taken in
//   proportion (a straight wall's nearness is near enough linear in the angle; a doorway between two blocked directions
//   opens gradually instead of reaching straight past the wall's end).
// - scriptableVec10..16: up to four boxes, 7 numbers each back to back (box j: numbers 7j..7j+6 of the 28, number n in
//   slot 10 + n / 4, component n % 4): its centre (x y z), its half sizes (x y z), its facing - the angle of its +X
//   (radians, anticlockwise from the area's +X; its +Y a quarter turn on). A box with no half size across is none.
//   (A box takes 7 numbers, not the 8 of two whole vectors: 1 + 8 + 7 = 16 holds four boxes, not three.)

#include <vector>

#include "rules/cover_objects.h"

namespace rules {

constexpr int kParticleMaskSlots = 16;
constexpr int kMaskRingDirections = 32;
constexpr int kMaskRingFirstSlot = 1;   // scriptableVec2 (0-based in the packed list)
constexpr int kMaskBoxFirstSlot = 9;    // scriptableVec10
constexpr int kParticleMaskMaxBoxes = 4;
constexpr int kMaskBoxNumbers = 7;
constexpr float kMaskWallMargin = 0.25f;  // the particles drawn this far past where the sight line met the wall: its face

struct MaskVec {
  float x = 0, y = 0, z = 0, w = 0;
};

struct ParticleMask {
  float origin[3] = {0, 0, 0};
  float reach = 0.0f;                            // metres across the ground from the origin; 0: off
  float walls[kMaskRingDirections] = {};         // each direction's wall, metres across the ground; 0: none in reach
  std::vector<ObjectBox> boxes;                  // the first kParticleMaskMaxBoxes are packed
};

// Direction k's angle: radians, anticlockwise from the area's +X.
float MaskRingAngle(int k);

// The 16 uniforms, scriptableVec1 first. A reach of 0 packs all zeros (off). A wall at 0 or less, or past the reach, is
// none.
std::vector<MaskVec> PackParticleMask(const ParticleMask& mask);

// The shader's test: whether a fragment at `p` (the area's metres) is dropped - within the reach across the ground,
// past its direction's wall, or the line from the origin to it meeting a box (rules::SegmentHitsBox's slabs, in the
// box's frame). False when the mask is off, the list isn't 16 long, or the fragment is past the reach.
bool ParticleMasked(const std::vector<MaskVec>& uniforms, const float p[3]);

// The boxes a mask takes (#184: four fit): each within the reach of the origin across the ground (its nearest point),
// not holding the origin (every line from inside it would pass through it: the whole effect masked), the ones that
// hide the most from the origin first - the widest angle across them as seen from it (a near crate before a far
// statue of its size); equal ones, the nearer first. At most kParticleMaskMaxBoxes.
std::vector<ObjectBox> PickMaskBoxes(const float origin[3], float reach, const std::vector<ObjectBox>& boxes);

// The angle (radians) a box spans across the ground as seen from `origin`: its corners' widest pair. Pi when the origin
// is inside its footprint.
float BoxAngleFrom(const float origin[3], const ObjectBox& box);

// The distance across the ground from `origin` to the box's footprint (0 inside it).
float BoxGroundDistance(const float origin[3], const ObjectBox& box);

}  // namespace rules
