#pragma once

// The look player (docs/superpowers/specs/2026-11-10-spell-attack-bench-design.md): plays a spell look - the Spell &
// Attack Bench's .look file (data/anim_look.h) - in game, timed from the cast. Looks load at start from
// <dll>\data\looks\ (shipped), then <dll>\spellbench\looks\ (the bench's Save; it wins by name). A cast of a row with a
// look swaps the cast clip and plays the caster's parts (its own spell-cast listener); the rule's impact calls
// PlaySpellLook in one line and lands its damage when that says. Logs "look: ...".

#include <stdint.h>

#include <string>

namespace data {
struct SpellLook;
}

// The game rule's one line, at the spell's impact: the look for spells.2da `row`, from `caster` to `target`. Returns the
// seconds until the hit lands (the damage waits that long; at least 0.01 when a look played); 0, and nothing played,
// when no look has that row. Safe inside a spell's impact handler (it runs script chunks); never blocks.
float PlaySpellLook(uint32_t caster, uint32_t target, int row);
// A leap from `from` to `to` that starts `after` seconds from now (the previous hit). Returns the seconds until it lands,
// from now; 0 and nothing played without a look for `row`.
float PlaySpellLookLeap(uint32_t from, uint32_t to, int row, float after);
// A miss (owner, 2026-11-10: "A"): the missile flies as for a hit and fizzles at `target` - no hit effect, no hit
// sounds. Returns the seconds until it arrives; 0 and nothing played without a look for `row` (the rule then shows its
// own). The rule's miss path calls it where a hit calls PlaySpellLook.
float PlaySpellLookMiss(uint32_t caster, uint32_t target, int row);
// A leap that misses: its beam from `from` to `to`, no hit. Returns the seconds until it would land, from now.
float PlaySpellLookLeapMiss(uint32_t from, uint32_t to, int row, float after);

// The same with a look in hand - the bench's staged replay (nwn5e-tools/dev/spell_bench_link.cpp).
void StartLookCast(uint32_t caster, const data::SpellLook& look, int row);  // the clip and the caster's parts, now
float PlayLookHit(uint32_t caster, uint32_t target, const data::SpellLook& look, int row);
float PlayLookMiss(uint32_t caster, uint32_t target, const data::SpellLook& look, int row);
float PlayLookLeap(uint32_t from, uint32_t to, const data::SpellLook& look, int row, float after);
float PlayLookLeapMiss(uint32_t from, uint32_t to, const data::SpellLook& look, int row, float after);

// The visualeffects rows `row`'s look plays that the game doesn't serve (Type_FD empty: the orb rows before the game
// session's looks row server is in the install), as "2100, 2101" - empty when all are there. Each row is asked once
// (a script chunk run as `self`) and logged when missing.
std::string LookRowsMissing(uint32_t self, const data::SpellLook& look, int row);

void InstallLookPlayer();  // from InstallAnimLayers
