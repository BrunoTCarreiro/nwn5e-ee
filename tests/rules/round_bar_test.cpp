#include "check.h"
#include "rules/round_bar.h"

#include <vector>

using namespace rules;

// #127 (owner, 2026-11-03: "go with A"). Breaks caught: a fill past full or below empty, a fill with no length, the
// wrong way round; the swings out of order, past the bar, more than the bar has room for; an idle round with a
// colour, a cast round as an attack; a pip that says the wrong thing; the bar over the strip.
TEST(RoundBarFillAndSwings) {
  CHECK_EQ(RoundFill(0, 6000) == 0.0f, true);
  CHECK_EQ(RoundFill(3000, 6000) == 0.5f, true);
  CHECK_EQ(RoundFill(9000, 6000) == 1.0f, true);
  CHECK_EQ(RoundFill(-5, 6000) == 0.0f, true);
  CHECK_EQ(RoundFill(3000, 0) == 0.0f, true);
  const std::vector<float> marks = RoundSwingMarks({5000, 1000, 3000, 7000}, 6000);
  CHECK_EQ(marks.size(), 4u);
  CHECK_EQ(marks.size() == 4 && marks[0] < marks[1] && marks[1] < marks[2] && marks[3] == 1.0f, true);
  CHECK_EQ(marks.size() == 4 && marks[1] == 0.5f, true);
  std::vector<long long> many(14, 1000);
  CHECK_EQ(static_cast<int>(RoundSwingMarks(many, 6000).size()), kRoundBarMaxSwings);
}

TEST(RoundBarKindsAndPips) {
  CHECK_EQ(RoundKindOf(false, true) == RoundKind::kNone, true);
  CHECK_EQ(RoundKindOf(true, false) == RoundKind::kAttack, true);
  CHECK_EQ(RoundKindOf(true, true) == RoundKind::kCast, true);
  CHECK_EQ(ActionPip(true) == RoundPip::kSpent, true);
  CHECK_EQ(ActionPip(false) == RoundPip::kFree, true);
  CHECK_EQ(BonusPip(true, false) == RoundPip::kFree, true);
  CHECK_EQ(BonusPip(false, false) == RoundPip::kSpent, true);
  CHECK_EQ(BonusPip(false, true) == RoundPip::kWaiting, true);
  CHECK_EQ(ReactionPip(true) == RoundPip::kFree, true);
  CHECK_EQ(ReactionPip(false) == RoundPip::kSpent, true);
  CHECK_EQ(RoundBarTop(true, 1.0f) == 126.0f, true);  // the strip's tiles: 4 + 122
  CHECK_EQ(RoundBarTop(true, 2.0f) == 248.0f, true);
  CHECK_EQ(RoundBarTop(false, 1.0f) == 0.0f, true);
}
