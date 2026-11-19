#pragma once

#include <stdint.h>

#include <vector>

namespace rules {

// #171 (owner, 2026-11-18: "I'd rather have a small window that shows the targets assigned to each ray and an ok and
// cancel button. This should also be the case for magic missile ... faithfully they should be able to be directed";
// earlier "remember the order in which enemies are selected"; the window before the cast, "A"): a spell that sends
// several rays or darts at creatures of the caster's choice - Scorching Ray ("You can hurl them at one target within
// range or at several"), Magic Missile (each dart "a creature of your choice") - picks them in a window, one row a
// ray or dart. Row 1 is the creature cast at; a pick fills the next empty row (the same creature may fill several);
// a row is cleared on its own, or the latest pick taken back; OK sends each row's ray at its target, an empty row
// following the last target picked.

// The rows the spell `spell_row` (spells.2da) cast with a level `slot_level` slot has: Magic Missile 3 at level 1 and
// one more a slot level above (rules::MagicMissileDarts), Scorching Ray 3 at level 2 and one more a level above; 0 for
// any other spell (no window).
int TargetRowsFor(int spell_row, int slot_level);

// The rows: an object id each, 0 empty; `picked` the rows filled, oldest first.
struct TargetRows {
  std::vector<uint32_t> rows;
  std::vector<int> picked;
};

// `count` rows (none below 1), row 1 `first` when it isn't 0.
TargetRows MakeTargetRows(int count, uint32_t first);

// Fills the first empty row with `id`. False when every row has a target (or `id` is 0).
bool AddTarget(TargetRows* rows, uint32_t id);

// Empties row `row` (0-based). False when it's out of range or already empty.
bool ClearRow(TargetRows* rows, int row);

// Empties the row filled last; the row emptied, -1 when none has a target.
int TakeBackLatest(TargetRows* rows);

// Puts `id` back in row `row` as the latest pick (a take-back undone). False when the row is out of range or taken.
bool RestoreRow(TargetRows* rows, int row, uint32_t id);

// Each row's target, in row order: an empty row's the last target picked. Empty when no row has a target.
std::vector<uint32_t> ResolveTargets(const TargetRows& rows);

// How many rows have a target.
int FilledRows(const TargetRows& rows);

// 0.99.88 (owner, 2026-11-19: "There isn't a way to see how many darts each of the targets will be getting as they
// get a uniform red glow" - "A now, B later"): each target and how many rays or darts are assigned to it - the filled
// rows only (owner, 2026-11-19: "how many darts/bolts are assigned to them, not how many are left"; an empty row still
// goes to the latest pick at the cast, ResolveTargets) - in the order each first appears. Empty when no row has a
// target.
struct TargetCount {
  uint32_t id = 0;
  int count = 0;
};
std::vector<TargetCount> CountsByTarget(const TargetRows& rows);

}  // namespace rules
