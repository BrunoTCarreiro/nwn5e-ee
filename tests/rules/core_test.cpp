#include "check.h"
#include "rules/core.h"

using namespace rules;

// Breaks caught: integer division truncating toward zero (9 -> 0 instead of -1),
// off-by-one at the even/odd boundary.
TEST(AbilityModifierRoundsDown) {
  CHECK_EQ(AbilityModifier(10), 0);
  CHECK_EQ(AbilityModifier(11), 0);
  CHECK_EQ(AbilityModifier(12), 1);
  CHECK_EQ(AbilityModifier(9), -1);
  CHECK_EQ(AbilityModifier(8), -1);
  CHECK_EQ(AbilityModifier(7), -2);
  CHECK_EQ(AbilityModifier(1), -5);
  CHECK_EQ(AbilityModifier(30), 10);
}

// Breaks caught: wrong tier boundary (e.g. +3 starting at level 4 or 6).
TEST(ProficiencyBonusChangesAtLevels5_9_13_17) {
  CHECK_EQ(ProficiencyBonusForLevel(1), 2);
  CHECK_EQ(ProficiencyBonusForLevel(4), 2);
  CHECK_EQ(ProficiencyBonusForLevel(5), 3);
  CHECK_EQ(ProficiencyBonusForLevel(8), 3);
  CHECK_EQ(ProficiencyBonusForLevel(9), 4);
  CHECK_EQ(ProficiencyBonusForLevel(12), 4);
  CHECK_EQ(ProficiencyBonusForLevel(13), 5);
  CHECK_EQ(ProficiencyBonusForLevel(16), 5);
  CHECK_EQ(ProficiencyBonusForLevel(17), 6);
  CHECK_EQ(ProficiencyBonusForLevel(20), 6);
}

// Breaks caught: NWN epic levels (21-40) continuing the progression to +7..+11,
// or level 0 producing +1.
TEST(ProficiencyBonusClampsLevelsOutside1To20) {
  CHECK_EQ(ProficiencyBonusForLevel(0), 2);
  CHECK_EQ(ProficiencyBonusForLevel(21), 6);
  CHECK_EQ(ProficiencyBonusForLevel(40), 6);
}

// Breaks caught: wrong tier boundary in the creature table, including the
// tiers above CR 20 that characters never reach.
TEST(ChallengeRatingProficiencyTiers) {
  CHECK_EQ(ProficiencyBonusForChallengeRating(0), 2);
  CHECK_EQ(ProficiencyBonusForChallengeRating(4), 2);
  CHECK_EQ(ProficiencyBonusForChallengeRating(5), 3);
  CHECK_EQ(ProficiencyBonusForChallengeRating(8), 3);
  CHECK_EQ(ProficiencyBonusForChallengeRating(9), 4);
  CHECK_EQ(ProficiencyBonusForChallengeRating(13), 5);
  CHECK_EQ(ProficiencyBonusForChallengeRating(17), 6);
  CHECK_EQ(ProficiencyBonusForChallengeRating(20), 6);
  CHECK_EQ(ProficiencyBonusForChallengeRating(21), 7);
  CHECK_EQ(ProficiencyBonusForChallengeRating(24), 7);
  CHECK_EQ(ProficiencyBonusForChallengeRating(25), 8);
  CHECK_EQ(ProficiencyBonusForChallengeRating(28), 8);
  CHECK_EQ(ProficiencyBonusForChallengeRating(29), 9);
  CHECK_EQ(ProficiencyBonusForChallengeRating(30), 9);
}

// Breaks caught: fractional CRs (1/8, 1/4, 1/2) or NWN's non-integer CRs
// rounding up into the next tier; CRs above 30 running off the table.
TEST(ChallengeRatingProficiencyHandlesFractionsAndOutOfRange) {
  CHECK_EQ(ProficiencyBonusForChallengeRating(0.125), 2);
  CHECK_EQ(ProficiencyBonusForChallengeRating(0.5), 2);
  CHECK_EQ(ProficiencyBonusForChallengeRating(4.9), 2);
  CHECK_EQ(ProficiencyBonusForChallengeRating(-1), 2);
  CHECK_EQ(ProficiencyBonusForChallengeRating(45), 9);
}

// Breaks caught: base 10 instead of 8, or the proficiency bonus left out.
TEST(SaveDcIsEightPlusProficiencyPlusModifier) {
  CHECK_EQ(SaveDifficultyClass(3, 2), 13);
  CHECK_EQ(SaveDifficultyClass(-1, 6), 13);
  CHECK_EQ(SaveDifficultyClass(5, 6), 19);
}

// Breaks caught: the proficiency bonus or the modifier left out.
TEST(SpellAttackIsProficiencyPlusModifier) {
  CHECK_EQ(SpellAttackBonus(3, 2), 5);
  CHECK_EQ(SpellAttackBonus(-1, 4), 3);
}
