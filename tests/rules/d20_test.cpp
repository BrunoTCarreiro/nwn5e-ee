#include "check.h"
#include "rules/d20.h"
#include "scripted_dice.h"

using namespace rules;

// Breaks caught: sources adding up (2 advantage beating 1 disadvantage, as in
// some house rules), or one side winning when both are present.
TEST(AdvantageAndDisadvantageCancelInAnyAmounts) {
  CHECK_EQ(ResolveRollMode(0, 0), RollMode::kNormal);
  CHECK_EQ(ResolveRollMode(1, 0), RollMode::kAdvantage);
  CHECK_EQ(ResolveRollMode(3, 0), RollMode::kAdvantage);
  CHECK_EQ(ResolveRollMode(0, 1), RollMode::kDisadvantage);
  CHECK_EQ(ResolveRollMode(0, 2), RollMode::kDisadvantage);
  CHECK_EQ(ResolveRollMode(1, 1), RollMode::kNormal);
  CHECK_EQ(ResolveRollMode(3, 1), RollMode::kNormal);
  CHECK_EQ(ResolveRollMode(1, 4), RollMode::kNormal);
}

// Breaks caught: a normal roll rolling two dice, or rolling the wrong die size.
TEST(NormalRollRollsOneD20) {
  ScriptedDice dice{7};
  const D20Roll roll = RollD20(RollMode::kNormal, dice);
  CHECK_EQ(roll.kept, 7);
  CHECK_EQ(roll.second, 0);
  CHECK_ROLLED(dice, {20});
}

// Breaks caught: always keeping the second die, or keeping the lower one.
TEST(AdvantageKeepsTheHigherDieWhicheverComesFirst) {
  ScriptedDice low_first{7, 15};
  CHECK_EQ(RollD20(RollMode::kAdvantage, low_first).kept, 15);
  ScriptedDice high_first{18, 3};
  const D20Roll roll = RollD20(RollMode::kAdvantage, high_first);
  CHECK_EQ(roll.kept, 18);
  CHECK_EQ(roll.first, 18);
  CHECK_EQ(roll.second, 3);
  CHECK_ROLLED(high_first, {20, 20});
}

// Breaks caught: disadvantage keeping the higher die or always the first one.
TEST(DisadvantageKeepsTheLowerDieWhicheverComesFirst) {
  ScriptedDice low_first{7, 15};
  CHECK_EQ(RollD20(RollMode::kDisadvantage, low_first).kept, 7);
  ScriptedDice high_first{18, 3};
  CHECK_EQ(RollD20(RollMode::kDisadvantage, high_first).kept, 3);
}

// Breaks caught: the 2014 exhaustion ladder (no flat penalty), a penalty of 1
// per level, or a positive number.
TEST(ExhaustionReducesD20TestsByTwoPerLevel) {
  CHECK_EQ(ExhaustionD20Penalty(0), 0);
  CHECK_EQ(ExhaustionD20Penalty(1), -2);
  CHECK_EQ(ExhaustionD20Penalty(3), -6);
  CHECK_EQ(ExhaustionD20Penalty(5), -10);
  CHECK_EQ(ExhaustionD20Penalty(-2), 0);
}

// Breaks caught: death at level 5 or never.
TEST(ExhaustionLevelSixIsDeath) {
  CHECK_EQ(IsDeadFromExhaustion(5), false);
  CHECK_EQ(IsDeadFromExhaustion(6), true);
}
