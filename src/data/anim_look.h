#pragma once

// Spell looks (docs/superpowers/specs/2026-11-10-spell-attack-bench-design.md): how a spell looks, sounds and is
// timed - the cast clip, the caster's effect, the missile, the hit, the leap and the sounds. The Spell & Attack Bench
// (nwn5e-tools/spell_bench) saves one .look file per spell; src/core/anim_look_player.cpp plays it in game. A look
// never touches a rule: the rule asks it when its hit lands and lands the damage then.
//
// The file is line-based, `key = value`, `#` comments:
//   name, rows (the spells.2da rows it dresses), rule_at (where the rule runs, seconds from the cast's start - the
//   bench's assumption, ConjTime), clip / clip_base / clip_seconds (RequestCastClip's swap), release (the missile
//   leaves the hand), leap_for / leap_after (a leap's beam, and how long after the hit before the next one lands),
//   then the parts: cast_vfx / cast_vfx_at / cast_vfx_for, missile, hit, leap_beam (visualeffects rows; 0 = none),
//   `sound = <cast|release|hit> <at> <resref>` (repeatable; `sound = none` for none), `table.<Column> = <value>`
//   (spells.2da presentation columns, served by the game session's row server).
// A `[<row>]` line starts that row's variant: it copies the parts so far, and every later line belongs to it (parts
// keys only). Its first `sound` line clears the copied sounds. The JS twin (spell_bench/public/look.js) reads and
// writes the same text; tests/data/fixtures/chromatic_orb.look is the canonical form both must write back.

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace data {

enum class LookAnchor { kCast, kRelease, kHit };  // a sound's time counts from the cast's start, the release, or each hit

struct LookSound {
  LookAnchor anchor = LookAnchor::kCast;
  float at = 0.0f;
  std::string resref;
};

struct LookParts {  // what a [row] section may change
  int cast_vfx = 0;  // a visualeffects row on the caster (0 = none)
  float cast_vfx_at = 0.0f, cast_vfx_for = 0.0f;
  int missile = 0, hit = 0, leap_beam = 0;  // visualeffects rows (0 = none; missile 0 = "on the target")
  std::vector<LookSound> sounds;
  std::map<std::string, std::string> table;  // spells.2da presentation columns
};

struct SpellLook {
  std::string name, clip, clip_base = "castout";
  std::vector<int> rows;
  float rule_at = 0.0f, clip_seconds = 0.0f, release = 0.0f, leap_for = 1.0f, leap_after = 0.3f;
  LookParts parts;                    // the defaults
  std::map<int, LookParts> variants;  // a [row] section: the defaults with its own keys over them
};

// Reads a .look file. On a bad line: false, and *error (when given) = "line N: <why>"; *look is written only on success.
// Refused: unknown keys, a resref that isn't 1-16 of [a-z0-9_] (it becomes script text), a time outside 0-10 s, a table
// column that isn't presentation, a release before the rule runs, a section for a row the look doesn't list.
bool ReadSpellLook(const std::string& text, SpellLook* look, std::string* error);
// The canonical text (the fixture's form): numbers with at most 3 decimals, trailing zeros trimmed.
std::string WriteSpellLook(const SpellLook& look);
// The row's parts: its section, else the defaults.
const LookParts& LookPartsFor(const SpellLook& look, int row);

}  // namespace data
