#include "check.h"
#include "rules/combat.h"
#include "scripted_dice.h"

using namespace rules;

// Breaks caught: finesse taking Strength unconditionally, ranged using Strength,
// melee picking the better ability.
TEST(AttackAbilityFollowsWeaponKind) {
  CHECK_EQ(AttackAbilityModifier(AttackAbility::kStrength, -1, 4), -1);
  CHECK_EQ(AttackAbilityModifier(AttackAbility::kDexterity, 3, 1), 1);
  CHECK_EQ(AttackAbilityModifier(AttackAbility::kFinesse, 3, 1), 3);
  CHECK_EQ(AttackAbilityModifier(AttackAbility::kFinesse, 1, 4), 4);
}

// Breaks caught: proficiency added when not proficient, or the other bonus dropped.
TEST(AttackBonusAddsProficiencyOnlyWhenProficient) {
  CHECK_EQ(AttackBonus(3, 2, true, 0), 5);
  CHECK_EQ(AttackBonus(3, 2, false, 0), 3);
  CHECK_EQ(AttackBonus(-1, 4, true, 1), 4);
}

// Breaks caught: a natural 1 hitting when the total beats the AC.
TEST(NaturalOneAlwaysMisses) {
  CHECK_EQ(ResolveAttackRoll(1, 30, 10, 20), AttackOutcome::kMiss);
}

// Breaks caught: a natural 20 missing a high AC, or hitting without being critical.
TEST(NaturalTwentyIsAlwaysACriticalHit) {
  CHECK_EQ(ResolveAttackRoll(20, -5, 30, 20), AttackOutcome::kCritical);
}

// Breaks caught: ">" instead of ">=" (meeting the AC must hit).
TEST(MeetingTheArmorClassHits) {
  CHECK_EQ(ResolveAttackRoll(10, 5, 15, 20), AttackOutcome::kHit);
  CHECK_EQ(ResolveAttackRoll(10, 4, 15, 20), AttackOutcome::kMiss);
}

// Breaks caught: an expanded range ignored (19 only a hit), or turning misses
// into hits (contrary to the ruling in combat.h).
TEST(ExpandedCriticalRangeUpgradesHitsOnly) {
  CHECK_EQ(ResolveAttackRoll(19, 5, 15, 19), AttackOutcome::kCritical);
  CHECK_EQ(ResolveAttackRoll(18, 5, 15, 19), AttackOutcome::kHit);
  CHECK_EQ(ResolveAttackRoll(19, 0, 25, 19), AttackOutcome::kMiss);
}

// Breaks caught: the engine's "cannot crit" value (21) stopping a natural 20 from
// being critical — every 2024 attack roll can crit on a 20.
TEST(CriticalFloorAboveTwentyStillCritsOnTwenty) {
  CHECK_EQ(ResolveAttackRoll(20, 0, 15, 21), AttackOutcome::kCritical);
}

// Breaks caught: wrong die size, modifier left out.
TEST(DamageRollsDiceAndAddsModifier) {
  ScriptedDice dice{5};
  CHECK_EQ(RollDamage({{1, 8}}, 3, false, dice), 8);
  CHECK_ROLLED(dice, {8});
}

// Breaks caught: the 3e x2 multiplier (modifier doubled: 16), or dice not rerolled.
TEST(CriticalRollsDiceTwiceAndAddsModifierOnce) {
  ScriptedDice dice{5, 6};
  CHECK_EQ(RollDamage({{1, 8}}, 3, true, dice), 14);
  CHECK_ROLLED(dice, {8, 8});
}

// Breaks caught: extra dice groups (Sneak Attack) not doubled on a critical.
TEST(CriticalDoublesEveryDiceGroup) {
  ScriptedDice dice{5, 6, 1, 2, 3, 4};
  CHECK_EQ(RollDamage({{1, 8}, {2, 6}}, 3, true, dice), 24);
  CHECK_ROLLED(dice, {8, 8, 6, 6, 6, 6});
}

// Breaks caught: flat damage (e.g. a 2024 Unarmed Strike, 1 + Str) doubled on a critical.
TEST(CriticalDoesNotDoubleFlatDamage) {
  ScriptedDice dice{};
  CHECK_EQ(RollDamage({}, 4, true, dice), 4);
  CHECK_ROLLED(dice, {});
}

// Breaks caught: negative damage healing the target.
TEST(DamageNeverGoesBelowZero) {
  ScriptedDice dice{1};
  CHECK_EQ(RollDamage({{1, 4}}, -3, false, dice), 0);
}

// Breaks caught: Extra Attack at the wrong level, or missing for a class.
TEST(ExtraAttackAtLevelFiveForMartialClasses) {
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 4}}), 1);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 5}}), 2);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kBarbarian, 5}}), 2);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kMonk, 5}}), 2);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kPaladin, 5}}), 2);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kRanger, 5}}), 2);
}

// Breaks caught: Fighter's Two/Three Extra Attacks at the wrong level.
TEST(FighterGetsThreeAttacksAtElevenAndFourAtTwenty) {
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 10}}), 2);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 11}}), 3);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 19}}), 3);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 20}}), 4);
}

// Breaks caught: other classes getting extra attacks (NWN gives everyone iterative attacks).
TEST(NonMartialClassesMakeOneAttack) {
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kRogue, 20}}), 1);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kWizard, 20}}), 1);
  CHECK_EQ(AttacksPerAttackAction({}), 1);
}

// Breaks caught: Extra Attack from two classes adding up, or class levels being
// summed across classes (Fighter 4 + Barbarian 4 is not Extra Attack).
TEST(ExtraAttackFromSeveralClassesDoesNotStack) {
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 5}, {CharacterClass::kPaladin, 5}}), 2);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 11}, {CharacterClass::kBarbarian, 5}}), 3);
  CHECK_EQ(AttacksPerAttackAction({{CharacterClass::kFighter, 4}, {CharacterClass::kBarbarian, 4}}), 1);
}
