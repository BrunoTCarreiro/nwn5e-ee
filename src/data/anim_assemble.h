#pragma once

// Assembling an assembled recipe (#87, docs/BENCH-REDESIGN.md, "Shipping: the assembled recipe"):
// the game builds one at startup for each body from the player's own animations, and nwn5e-tools'
// helper builds the bench's preview with this same code (the core stamp says they match). Phase 1
// does steps 1, 6 and 9 of the design's order: the capture composed onto the named stance, the
// root, and the blends into and out of the stance and the loop.

#include <string>
#include <vector>

#include "data/anim_recipe.h"

namespace data {

// One library's clip from an assembled recipe, sampled evenly every ~1/30 s, frames 0 .. n with
// n = round(length * 30) and frame n at the length. Each keyed bone's turn is L(t) = S * delta(t),
// S its turn in the stance's first frame; a position track (the root's) is the stance's first-frame
// position + the offset. A loop crossfades the last loop_blend seconds toward frame 0's values,
// then the blends ease from and to the stance's first frame over blend_in / blend_out (smoothstep;
// a zero length is no blend). The delta tracks hold their first and last keys outside them. Bones
// without a track stay in the tree unkeyed, as KeyedAnimClip leaves them (the game plays rest).
// False, with `err` (the reason only - the caller names the recipe and library), when the recipe
// has no keys for `library`, the stance clip is empty, or none of the tracks' bones is in `bones`.
bool AssembleAnimClip(const AnimRecipe& recipe, const std::string& library, const std::vector<AnimNode>& bones,
                      const AnimClip& stance, AnimClip* out, std::string* err);

}  // namespace data
