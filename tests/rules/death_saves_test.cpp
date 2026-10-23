#include "check.h"
#include "rules/death_saves.h"
#include "scripted_dice.h"

using namespace rules;

// Breaks caught: a DC other than 10, a natural 1 counting once, Stable not
// resetting the counts, a death at two failures.
TEST(DeathSavesSucceedOnTenAndCountToThree) {
  DeathSaves saves;
  ScriptedDice dice{10, 9, 15, 1, 12};
  CHECK_EQ(RollDeathSave(saves, RollMode::kNormal, 0, 20, dice).result, DeathSaveResult::kSuccess);  // 10
  CHECK_EQ(RollDeathSave(saves, RollMode::kNormal, 0, 20, dice).result, DeathSaveResult::kFailure);  // 9
  CHECK_EQ(saves.failures, 1);
  CHECK_EQ(RollDeathSave(saves, RollMode::kNormal, 0, 20, dice).result, DeathSaveResult::kSuccess);  // 15
  CHECK_EQ(saves.successes, 2);
  // A natural 1: two more failures - the third, dead.
  CHECK_EQ(RollDeathSave(saves, RollMode::kNormal, 0, 20, dice).result, DeathSaveResult::kDead);
  CHECK_EQ(saves.failures, 3);
  // Three successes: Stable, the counts back to 0.
  DeathSaves other;
  ScriptedDice up{12, 12, 12};
  RollDeathSave(other, RollMode::kNormal, 0, 20, up);
  RollDeathSave(other, RollMode::kNormal, 0, 20, up);
  CHECK_EQ(RollDeathSave(other, RollMode::kNormal, 0, 20, up).result, DeathSaveResult::kStable);
  CHECK_EQ(other.stable, true);
  CHECK_EQ(other.successes, 0);
  CHECK_EQ(other.failures, 0);
}

// Breaks caught: the Exhaustion penalty skipped (a death save is a D20 Test),
// or applied to the natural 20 / natural 1 checks.
TEST(DeathSavesTakeTheExhaustionPenaltyButNotOnTheNaturalRoll) {
  DeathSaves saves;
  ScriptedDice dice{11, 20};
  const DeathSave save = RollDeathSave(saves, RollMode::kNormal, ExhaustionD20Penalty(1), 20, dice);
  CHECK_EQ(save.total, 9);
  CHECK_EQ(save.result, DeathSaveResult::kFailure);
  saves.failures = 2;
  saves.successes = 1;
  CHECK_EQ(RollDeathSave(saves, RollMode::kNormal, ExhaustionD20Penalty(5), 20, dice).result, DeathSaveResult::kWake);
  CHECK_EQ(saves.failures, 0);  // regaining HP resets the counts
  CHECK_EQ(saves.successes, 0);
}

// Breaks caught: Defy Death without advantage, or its 18-19 not waking.
TEST(DefyDeathRollsWithAdvantageAndWakesOnEighteen) {
  CHECK_EQ(HasDefyDeath(17), false);
  CHECK_EQ(HasDefyDeath(18), true);
  DeathSaves saves;
  ScriptedDice dice{4, 18};
  const DeathSave save = RollDeathSave(saves, RollMode::kAdvantage, 0, kDefyDeathTwentyFrom, dice);
  CHECK_EQ(save.roll.kept, 18);
  CHECK_EQ(save.result, DeathSaveResult::kWake);
  CHECK_ROLLED(dice, {20, 20});
  // Without Defy Death an 18 is a plain success.
  DeathSaves plain;
  ScriptedDice one{18};
  CHECK_EQ(RollDeathSave(plain, RollMode::kNormal, 0, 20, one).result, DeathSaveResult::kSuccess);
}

// Breaks caught: a crit counting once, damage not ending Stable, massive
// damage at 0 HP ignored, the off-by-one at exactly the maximum.
TEST(DamageAtZeroHitPointsAddsFailures) {
  DeathSaves saves;
  CHECK_EQ(DamageAtZeroHitPoints(saves, 3, 20, false), false);
  CHECK_EQ(saves.failures, 1);
  CHECK_EQ(DamageAtZeroHitPoints(saves, 3, 20, true), true);  // 1 + 2 = 3
  DeathSaves stable;
  stable.stable = true;
  CHECK_EQ(DamageAtZeroHitPoints(stable, 1, 20, false), false);
  CHECK_EQ(stable.stable, false);
  CHECK_EQ(stable.failures, 1);
  DeathSaves fresh;
  CHECK_EQ(DamageAtZeroHitPoints(fresh, 20, 20, false), true);  // at least the maximum
  CHECK_EQ(DamageAtZeroHitPoints(fresh, 0, 20, false), false);
  CHECK_EQ(fresh.failures, 0);
  CHECK_EQ(IsMassiveDamage(19, 20), false);
  CHECK_EQ(IsMassiveDamage(20, 20), true);
}

// Breaks caught: speed lost per level not 5 ft of 30, or not capped.
TEST(ExhaustionSlowsFiveFeetPerLevel) {
  CHECK_EQ(ExhaustionSpeedDecreasePercent(0), 0);
  CHECK_EQ(ExhaustionSpeedDecreasePercent(1), 17);
  CHECK_EQ(ExhaustionSpeedDecreasePercent(3), 50);
  CHECK_EQ(ExhaustionSpeedDecreasePercent(5), 83);
  CHECK_EQ(ExhaustionSpeedDecreasePercent(9), 83);
}
