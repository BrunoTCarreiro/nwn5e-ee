#pragma once

// A spell's particle mask (#184's research spike; the owner, 2026-11-23: "is there a way to make a "mask" effect? an
// effect that makes particles from other effects disappear", then "run the research spike now"). An emitter whose
// texture is nwn5e's material n5_pmask draws with fsn5pmask.shd - the game's own particle fragment shader, read from
// the player's game at build time, with a mask added (tools/data/particle_mask.js) - and that mask drops each fragment
// the line from the effect's point of origin to it passes through an obstacle's box to reach: inside the box or in its
// shadow, as the area's own cover rule draws it (rules/cover_objects.h). The shader reads the mask from EE's per-player
// scriptable uniforms (NWScript SetShaderUniformVec; GLSL scriptableVec1..16): this packs them, and ParticleMasked is the
// shader's own test, step for step, on the CPU.
//
// The uniforms:
// - scriptableVec1: the origin (x, y, z: the area's metres) and how many boxes (w; 0 = off, every uniform's default -
//   then the shader draws exactly as the game's own).
// - two for each box k (scriptableVec<2 + 2k> and <3 + 2k>): its centre and its +X's x, then its half sizes and its
//   +X's y. Its +Y is its +X turned a quarter anticlockwise (the facing: rules::BoxOf).

#include <vector>

#include "rules/cover_objects.h"

namespace rules {

// scriptableVec2 to scriptableVec15: seven boxes (scriptableVec16 is left free).
constexpr int kParticleMaskMaxBoxes = 7;

struct MaskVec {
  float x = 0, y = 0, z = 0, w = 0;
};

// The uniforms for `origin` and `boxes`, scriptableVec1 first: 1 + 2 a box. A box holding the origin is left out (every
// line from inside it would pass through it: the whole effect masked), and so are boxes past kParticleMaskMaxBoxes.
// No box: the one vector, its w 0 (off).
std::vector<MaskVec> PackParticleMask(const float origin[3], const std::vector<ObjectBox>& boxes);

// The shader's test: whether a fragment at `p` (the area's metres) is dropped - the line from the origin to it meets a
// box (rules::SegmentHitsBox's slabs, in the box's frame). False when the mask is off or malformed.
bool ParticleMasked(const std::vector<MaskVec>& uniforms, const float p[3]);

}  // namespace rules
