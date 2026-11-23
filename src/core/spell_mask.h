#pragma once

// The spells' particle masks (#184; the owner, 2026-11-23: "maskproof seems good!", then "wire it, lets test it out"):
// the walls and obstacles round an area spell's point of origin, measured as it takes effect and sent to the players'
// shaders (rules/particle_mask.h: what the mask hides and how its uniforms are laid out; rules/spell_masks.h: which
// spells, when, and which mask a client shows when several are up).
//
// - Its looks: the masked copies (tools/data/spell_mask.js builds them from the game; data\rows' *.if_spellmask.txt
//   rows name them), served with spell_mask=1 in faithful when every copy those rows name is in data\files. A cone's copy
//   is its spells.2da row's; a burst's or a cloud's is handed to the spell's script in place of the game's effect
//   (EffectVisualEffect, EffectAreaOfEffect: the first argument swapped while that spell's impact script runs).
// - When: as the cast begins for a cone (its look plays with the cast) - from the caster's eyes; as its impact script
//   starts for a burst at a point or a column from above; a lasting area's once its area of effect is there, held
//   while it lasts; Ice Knife's burst by its own code. Its origin is the area code's (object_cover.cpp SpellOriginOf:
//   the cover rule's own), its reach the area's size and 3 m.
// - What: the walls in 32 directions (MeasureMaskWalls) and the four placeables' boxes that hide the most from the
//   origin (rules::PickMaskBoxes, from the cover's list), read once as it takes effect - an instant spell's obstacles
//   are those there as it took effect, a bookcase its own blast broke still masks it. A lasting area's are read again
//   when an obstacle within its reach breaks or is destroyed (re-sent).
// - To whom: every player character in its area (the uniforms are each client's own), each client shown the newest
//   mask up in its area (rules::MaskToShow), the mask off when none - reconciled four times a second while any is up,
//   so a player arriving gets it and one leaving loses it. Each set, clear and re-send logged once ("spell mask: ...").

#include <stdint.h>

#include <string>
#include <vector>

#include "rules/particle_mask.h"

// The masks' hooks and rows (InstallPerceptionRules calls it, after the cover's table): spell_mask=1, the faithful
// rules, the shader and every copy the rows name built. The registry (and !maskproof) runs either way.
void InstallSpellMask();
// The spells' masks wired: their rows served, their hooks in.
bool SpellMasksOn();

// Ice Knife's burst (rules_spells2024.cpp, inside its ScopedAreaOrigin): `spell`'s mask set now, its origin the area
// code's for the burst.
void SetSpellMaskNow(void* caster, int spell);


// !maskproof (mask_proof.cpp): `mask` shown to the players in `creature`'s area for `hold_ms`, as a spell's would be;
// "off": the speaker's proof masks dropped. Logged as "the proof".
void SetProofMask(void* creature, const rules::ParticleMask& mask, int hold_ms);
void DropProofMasks(void* creature);

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
