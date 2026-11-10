#pragma once

// #155, the Spell & Attack Bench (the animation session's; docs/superpowers/specs/2026-11-10-spell-attack-bench-design.md):
// the rows its looks files may change. The row server (core/data_serving.cpp) merges <dll>\data\looks\*.look.txt (the
// shipped looks), then <dll>\spellbench\looks\*.look.txt (the bench's drafts), after every rows file - each in the rows
// file format ("<table>.look.txt": a header naming columns, then "<row> <values>"). No engine code; unit-tested.

#include <string>

#include "data/twoda.h"

namespace data {

// The visualeffects and progfx rows a look may fill (the orbs it flies).
constexpr int kLookVisualEffectsFirst = 2100;
constexpr int kLookVisualEffectsLast = 2199;
constexpr int kLookProgFxFirst = 910;
constexpr int kLookProgFxLast = 999;

// Whether a looks file for `table` ("spells", "visualeffects", "progfx") stays inside what a look may change:
// - spells: the presentation columns only (the cast's animations, visuals and sounds, the projectile), on rows the
//   table has (no row added) - never the rule's columns (scripts, class lists, levels, timings, targeting);
// - visualeffects 2100-2199 and progfx 910-999: any column, on a row empty in `base` (every value "****") or past its
//   end - a row the game (or nwn5e's rows) already uses is refused.
// Any other table, column or row: false, `error` why (the file is refused whole).
bool CheckLookRows(const std::string& table, const TwoDA& base, const std::string& rows_text, std::string* error);

// Whether `column` is a spells.2da presentation column a look may set.
bool IsLookSpellColumn(const std::string& column);

}  // namespace data
