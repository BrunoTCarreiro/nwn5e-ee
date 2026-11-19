#include "check.h"
#include "rules/cantrips.h"
#include "rules/spells2024.h"
#include "rules/target_rows.h"

#include <vector>

using namespace rules;

// #171 (owner, 2026-11-18: "I'd rather have a small window that shows the targets assigned to each ray and an ok and
// cancel button. This should also be the case for magic missile"; "remember the order in which enemies are
// selected"): the target window's rows.

TEST(TargetRowsCount) {
  CHECK_EQ(TargetRowsFor(kMagicMissile, 1), 3);
  CHECK_EQ(TargetRowsFor(kMagicMissile, 2), 4);  // one dart more a slot level above 1
  CHECK_EQ(TargetRowsFor(kMagicMissile, 9), 11);
  CHECK_EQ(TargetRowsFor(kMagicMissile, 0), 3);  // no slot below the spell's own
  CHECK_EQ(TargetRowsFor(kScorchingRay, 2), 3);
  CHECK_EQ(TargetRowsFor(kScorchingRay, 3), 4);  // one ray more a slot level above 2
  CHECK_EQ(TargetRowsFor(kScorchingRay, 9), 10);
  CHECK_EQ(TargetRowsFor(kScorchingRay, 1), 3);
  CHECK_EQ(TargetRowsFor(kGuidingBolt, 1), 0);  // no window
  CHECK_EQ(TargetRowsFor(kAcidArrow, 2), 0);
}

TEST(TargetRowsPicks) {
  TargetRows t = MakeTargetRows(3, 11);  // row 1: the creature cast at
  CHECK_EQ(static_cast<int>(t.rows.size()), 3);
  CHECK_EQ(t.rows[0], 11u);
  CHECK_EQ(FilledRows(t), 1);
  CHECK_EQ(AddTarget(&t, 22), true);
  CHECK_EQ(AddTarget(&t, 11), true);  // the same creature again: another ray
  CHECK_EQ(AddTarget(&t, 33), false);  // every row has a target
  CHECK_EQ(t.rows[1], 22u);
  CHECK_EQ(t.rows[2], 11u);
  CHECK_EQ(AddTarget(&t, 0), false);
  // [x] on row 2: it empties; the next pick fills it again (the first empty row).
  CHECK_EQ(ClearRow(&t, 1), true);
  CHECK_EQ(ClearRow(&t, 1), false);   // already empty
  CHECK_EQ(ClearRow(&t, 3), false);   // no such row
  CHECK_EQ(ClearRow(&t, -1), false);
  CHECK_EQ(AddTarget(&t, 44), true);
  CHECK_EQ(t.rows[1], 44u);
  CHECK_EQ(FilledRows(t), 3);
  // A right-click takes back the latest pick (44, row 2), then the one before (11, row 3).
  CHECK_EQ(TakeBackLatest(&t), 1);
  CHECK_EQ(TakeBackLatest(&t), 2);
  CHECK_EQ(t.rows[1], 0u);
  CHECK_EQ(t.rows[2], 0u);
  CHECK_EQ(TakeBackLatest(&t), 0);   // row 1 too
  CHECK_EQ(TakeBackLatest(&t), -1);  // nothing left
  CHECK_EQ(FilledRows(t), 0);
  // An empty start (row 1 unset); no object (0) is never a target.
  TargetRows none = MakeTargetRows(0, 0);
  CHECK_EQ(static_cast<int>(none.rows.size()), 1);
  CHECK_EQ(AddTarget(&none, 0), false);
  CHECK_EQ(none.rows[0], 0u);
  CHECK_EQ(FilledRows(none), 0);
}

TEST(TargetRowsOwnersExample) {
  // The owner's example (a, b, c, d): picks a, b, c; a right-click takes c back; a picked again; OK: a, b, a.
  const uint32_t a = 1, b = 2, c = 3;
  TargetRows t = MakeTargetRows(3, a);
  CHECK_EQ(AddTarget(&t, b), true);
  CHECK_EQ(AddTarget(&t, c), true);
  CHECK_EQ(TakeBackLatest(&t), 2);
  CHECK_EQ(AddTarget(&t, a), true);
  const std::vector<uint32_t> fired = ResolveTargets(t);
  CHECK_EQ(static_cast<int>(fired.size()), 3);
  CHECK_EQ(fired[0], a);
  CHECK_EQ(fired[1], b);
  CHECK_EQ(fired[2], a);
}

TEST(TargetRowsResolve) {
  // Empty rows follow the last target picked - the latest pick, not the lowest row.
  TargetRows t = MakeTargetRows(4, 5);
  CHECK_EQ(AddTarget(&t, 6), true);  // row 2
  CHECK_EQ(ClearRow(&t, 0), true);   // row 1 cleared: 6 is the only pick
  std::vector<uint32_t> fired = ResolveTargets(t);
  CHECK_EQ(static_cast<int>(fired.size()), 4);
  CHECK_EQ(fired[0], 6u);
  CHECK_EQ(fired[1], 6u);
  CHECK_EQ(fired[3], 6u);
  CHECK_EQ(AddTarget(&t, 7), true);  // fills row 1 (the first empty), the latest pick
  fired = ResolveTargets(t);
  CHECK_EQ(fired[0], 7u);
  CHECK_EQ(fired[1], 6u);
  CHECK_EQ(fired[2], 7u);  // empty: the latest pick's
  CHECK_EQ(fired[3], 7u);
  TargetRows empty = MakeTargetRows(3, 0);
  CHECK_EQ(ResolveTargets(empty).empty(), true);
}

TEST(TargetRowsRestore) {
  // A take-back undone (a click on the window that the game reported as a cancel first): the row and its place back.
  TargetRows t = MakeTargetRows(3, 1);
  CHECK_EQ(AddTarget(&t, 2), true);
  const uint32_t was = t.rows[1];
  const int row = TakeBackLatest(&t);
  CHECK_EQ(row, 1);
  CHECK_EQ(RestoreRow(&t, row, was), true);
  CHECK_EQ(t.rows[1], 2u);
  CHECK_EQ(t.picked.back(), 1);
  CHECK_EQ(RestoreRow(&t, 1, 9), false);  // taken
  CHECK_EQ(RestoreRow(&t, 5, 9), false);  // no such row
  CHECK_EQ(RestoreRow(&t, 2, 0), false);
}

TEST(ScorchingRayTable) {
  const Spell2024* ray = Spell2024Of(kScorchingRay);
  CHECK_EQ(ray != nullptr, true);
  CHECK_EQ(ray->level, 2);
  CHECK_EQ(ray->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(ray->dice, 2);
  CHECK_EQ(ray->die, 6);
  CHECK_EQ(ray->damage_type, kDamageTypeFire);
  CHECK_EQ(SpellDiceAt(*ray, 2), 2);
  CHECK_EQ(SpellDiceAt(*ray, 5), 2);  // each ray's dice never grow: the slot adds rays
}

// 0.99.88 (owner, 2026-11-19: "There isn't a way to see how many darts each of the targets will be getting" - "A now,
// B later"): each target's count as the rows resolve, first appearance first.
TEST(TargetRowsCountsByTarget) {
  TargetRows t = MakeTargetRows(5, 5);  // 5 darts: rows a, b, a, then two empty
  CHECK_EQ(AddTarget(&t, 6), true);
  CHECK_EQ(AddTarget(&t, 5), true);
  std::vector<TargetCount> counts = CountsByTarget(t);
  CHECK_EQ(static_cast<int>(counts.size()), 2);
  CHECK_EQ(counts[0].id, 5u);
  CHECK_EQ(counts[0].count, 2);  // rows 1 and 3: what's assigned, not the empty rows the cast gives the latest pick
  CHECK_EQ(counts[1].id, 6u);
  CHECK_EQ(counts[1].count, 1);
  CHECK_EQ(TakeBackLatest(&t), 2);  // row 3 taken back
  counts = CountsByTarget(t);
  CHECK_EQ(static_cast<int>(counts.size()), 2);
  CHECK_EQ(counts[0].id, 5u);
  CHECK_EQ(counts[0].count, 1);
  CHECK_EQ(counts[1].id, 6u);
  CHECK_EQ(counts[1].count, 1);
  ClearRow(&t, 0);  // row 1 emptied: 6 first now, 5 gone
  counts = CountsByTarget(t);
  CHECK_EQ(static_cast<int>(counts.size()), 1);
  CHECK_EQ(counts[0].id, 6u);
  CHECK_EQ(counts[0].count, 1);
  TargetRows empty = MakeTargetRows(3, 0);
  CHECK_EQ(static_cast<int>(CountsByTarget(empty).size()), 0);
}
