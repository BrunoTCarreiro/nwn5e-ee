#include "check.h"
#include "rules/swing_spacing.h"

using namespace rules;

// #53 (owner, 2026-11-03: "go with A"). Breaks caught: the engine's swing length or total wrong (its 750 / 500 /
// 3000, the divisor's 3 + (n - 3) / 2, two attacks'); the round's 6 s not kept (the engine's total not given back,
// the swings' not taken); dues not from the first swing, the last gap not at the round's end, a lone attack split,
// no room not refused; added swings not from now, closer than the least gap, the round's end not moved out,
// nothing added accepted.
TEST(SwingSpacingEngineLengths) {
  CHECK_EQ(EngineSwingMs(1), 1250);
  CHECK_EQ(EngineSwingMs(2), 1000);
  CHECK_EQ(EngineSwingMs(3), 916);
  CHECK_EQ(EngineSwingMs(4), 857);
  CHECK_EQ(EngineSwingMs(6), 666);
  CHECK_EQ(EngineSwingMs(8), 545);
  CHECK_EQ(EngineRoundSwingTotalMs(1), 1250);
  CHECK_EQ(EngineRoundSwingTotalMs(3), 2750);
  CHECK_EQ(EngineRoundSwingTotalMs(4), 3000);
  CHECK_EQ(EngineRoundSwingTotalMs(9), 3000);
}

TEST(SwingSpacingOneSwingPerAttack) {
  // Six attacks (Action Surge before a fight): the engine's timer 6000 - 3000, six swings of 666 ms.
  const SwingLayout six = OneSwingPerAttack(6, 0, 3000, 666);
  CHECK_EQ(six.round_length, 2004);
  CHECK_EQ(six.round_length + 6 * 666, 6000);  // the round's 6 s kept
  CHECK_EQ(static_cast<int>(six.dues.size()), 6);
  bool ordered = six.dues.size() == 6;
  for (size_t i = 1; ordered && i < six.dues.size(); ++i) ordered = six.dues[i] > six.dues[i - 1];
  CHECK_EQ(ordered, true);
  CHECK_EQ(six.dues.size() == 6 && six.dues[0] == 0 && six.dues[1] == 334 && six.dues[5] == 1670, true);
  // A start delay: from the first swing's due, the last gap at the round's end.
  const SwingLayout late = OneSwingPerAttack(4, 200, 3000, 857);
  CHECK_EQ(late.round_length, 3000 + 3000 - 4 * 857);
  CHECK_EQ(late.dues.size() == 4 && late.dues[0] == 200 && late.dues[3] == 1979, true);
  CHECK_EQ(late.dues.size() == 4 && late.dues[1] == 793, true);
  // Nothing to split, or no room.
  CHECK_EQ(OneSwingPerAttack(1, 0, 4750, 1250).dues.empty(), true);
  CHECK_EQ(OneSwingPerAttack(6, 0, 3000, 5000).dues.empty(), true);
}

TEST(SwingSpacingAddedSwings) {
  // Action Surge mid-round: two swings to come, three added, room enough.
  const SwingLayout room = AddedSwings(2, 3, 1500, 3250);
  CHECK_EQ(room.round_length, 3250);
  CHECK_EQ(room.dues.size() == 5 && room.dues[0] == 1500 && room.dues[1] == 1850 && room.dues[4] == 2900, true);
  // Late in the round: the least gap kept, the round's end moved out.
  const SwingLayout late = AddedSwings(0, 3, 3000, 3250);
  CHECK_EQ(late.round_length, 3000 + 3 * kMinSwingGapMs);
  CHECK_EQ(late.dues.size() == 3 && late.dues[1] - late.dues[0] == kMinSwingGapMs, true);
  // Past its end already (a swing holding it open): moved out, never in.
  const SwingLayout over = AddedSwings(1, 1, 4000, 3250);
  CHECK_EQ(over.round_length, 4000 + 2 * kMinSwingGapMs);
  CHECK_EQ(AddedSwings(2, 0, 1500, 3250).dues.empty(), true);
}
