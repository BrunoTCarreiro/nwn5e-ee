#pragma once

// The spells' particle masks (#184; the owner, 2026-11-23: "maskproof seems good!", then "wire it, lets test it out"):
// the walls and obstacles round an area spell's point of origin, measured as it takes effect and sent to the players'
// shaders (rules/particle_mask.h: what the mask hides and how its uniforms are laid out).

#include <stdint.h>

#include <string>
#include <vector>

#include "rules/particle_mask.h"

// The ring's walls round `origin` in `area`, out to `reach` metres across the ground: each of the 32 directions' wall
// (metres; 0: none within the reach), measured with the engine's own sight line (CNWSArea::ClearLineOfSight: the
// tileset's walls and shut doors - placeables are the boxes' part) across the ground, at the origin's height over the
// ground under it held to 0.5 - 2 m (a falling cylinder's top is measured near the ground it comes down on), the end
// following the ground; the area's edge is a wall. `self`: the object the lines start at (the caster). `lines`: the
// sight lines it took. False when the engine's sight line isn't there (every wall 0).
bool MeasureMaskWalls(void* area, uint32_t self, const float origin[3], float reach,
                      float walls[rules::kMaskRingDirections], int* lines);

// The NWScript that sets every scriptable uniform of `player` (an NWScript object expression) to `uniforms`
// (SetShaderUniformVec, SHADER_UNIFORM_1 first).
std::string MaskUniformScript(const std::string& player, const std::vector<rules::MaskVec>& uniforms);

// A line for the log: the walls (how many of the 32 directions, the nearest) and the boxes.
std::string MaskWords(const rules::ParticleMask& mask);
