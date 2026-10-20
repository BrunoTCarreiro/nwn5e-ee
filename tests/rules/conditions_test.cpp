#include "check.h"
#include "rules/conditions.h"

using namespace rules;

// CN2/CN3. Breaks caught: stunned auto-fail lost in NWN mode (decided for both
// modes); paralysis / sleep auto-fail leaking into NWN mode; Constitution or
// Wisdom saves auto-failed; the rule still on with conditions=off.
TEST(ConditionsAutoFailStrengthAndDexteritySaves) {
  Conditions stunned;
  stunned.stunned = true;
  CHECK_EQ(ConditionSaveOutcome(stunned, Ability::kDexterity, RuleVariant::kFaithful) == SaveOutcome::kAutoFail, true);
  CHECK_EQ(ConditionSaveOutcome(stunned, Ability::kStrength, RuleVariant::kFaithful) == SaveOutcome::kAutoFail, true);
  CHECK_EQ(ConditionSaveOutcome(stunned, Ability::kDexterity, RuleVariant::kNwn) == SaveOutcome::kAutoFail, true);
  CHECK_EQ(ConditionSaveOutcome(stunned, Ability::kConstitution, RuleVariant::kFaithful) == SaveOutcome::kRoll, true);
  CHECK_EQ(ConditionSaveOutcome(stunned, Ability::kWisdom, RuleVariant::kNwn) == SaveOutcome::kRoll, true);
  CHECK_EQ(ConditionSaveOutcome(stunned, Ability::kDexterity, RuleVariant::kOff) == SaveOutcome::kRoll, true);
  Conditions held;
  held.paralyzed = true;
  CHECK_EQ(ConditionSaveOutcome(held, Ability::kStrength, RuleVariant::kFaithful) == SaveOutcome::kAutoFail, true);
  CHECK_EQ(ConditionSaveOutcome(held, Ability::kDexterity, RuleVariant::kNwn) == SaveOutcome::kRoll, true);
  Conditions asleep;
  asleep.asleep = true;
  CHECK_EQ(ConditionSaveOutcome(asleep, Ability::kDexterity, RuleVariant::kFaithful) == SaveOutcome::kAutoFail, true);
  CHECK_EQ(ConditionSaveOutcome({}, Ability::kDexterity, RuleVariant::kFaithful) == SaveOutcome::kRoll, true);
}

// CN1. Breaks caught: restrained giving disadvantage on Strength saves (2024:
// Dexterity only) or in NWN mode.
TEST(RestrainedHasDisadvantageOnDexteritySaves) {
  Conditions restrained;
  restrained.restrained = true;
  CHECK_EQ(ConditionSaveDisadvantage(restrained, Ability::kDexterity, RuleVariant::kFaithful), 1);
  CHECK_EQ(ConditionSaveDisadvantage(restrained, Ability::kStrength, RuleVariant::kFaithful), 0);
  CHECK_EQ(ConditionSaveDisadvantage(restrained, Ability::kDexterity, RuleVariant::kNwn), 0);
  CHECK_EQ(ConditionSaveDisadvantage({}, Ability::kDexterity, RuleVariant::kFaithful), 0);
}

// CN1/CN2/CN4/CN5/CN6. Breaks caught: a source on the wrong side (attacker vs
// target), concealment giving advantage, faithful-only sources in NWN mode,
// helpless advantage missing in NWN mode (decided for both), anything with
// conditions=off.
TEST(ConditionsAddAttackRollSources) {
  Conditions none;
  Conditions held;
  held.paralyzed = true;
  RollSources s = ConditionAttackSources(none, held, RuleVariant::kFaithful);
  CHECK_EQ(s.advantage, 1);
  CHECK_EQ(s.disadvantage, 0);
  Conditions restrained;
  restrained.restrained = true;
  s = ConditionAttackSources(restrained, none, RuleVariant::kFaithful);
  CHECK_EQ(s.advantage, 0);
  CHECK_EQ(s.disadvantage, 1);
  s = ConditionAttackSources(none, restrained, RuleVariant::kFaithful);
  CHECK_EQ(s.advantage, 1);
  CHECK_EQ(s.disadvantage, 0);
  Conditions poisoned;
  poisoned.poisoned = true;
  s = ConditionAttackSources(poisoned, none, RuleVariant::kFaithful);
  CHECK_EQ(s.disadvantage, 1);
  s = ConditionAttackSources(none, poisoned, RuleVariant::kFaithful);
  CHECK_EQ(s.disadvantage, 0);
  Conditions afraid;
  afraid.frightened_source_in_sight = true;
  s = ConditionAttackSources(afraid, none, RuleVariant::kFaithful);
  CHECK_EQ(s.disadvantage, 1);
  Conditions blurred;
  blurred.concealed = true;
  s = ConditionAttackSources(none, blurred, RuleVariant::kFaithful);
  CHECK_EQ(s.advantage, 0);
  CHECK_EQ(s.disadvantage, 1);
  s = ConditionAttackSources(poisoned, held, RuleVariant::kNwn);
  CHECK_EQ(s.advantage, 1);
  CHECK_EQ(s.disadvantage, 0);
  s = ConditionAttackSources(poisoned, restrained, RuleVariant::kNwn);
  CHECK_EQ(s.advantage, 0);
  CHECK_EQ(s.disadvantage, 0);
  Conditions stunned;
  stunned.stunned = true;
  s = ConditionAttackSources(none, stunned, RuleVariant::kNwn);
  CHECK_EQ(s.advantage, 1);
  s = ConditionAttackSources(poisoned, held, RuleVariant::kOff);
  CHECK_EQ(s.advantage, 0);
  CHECK_EQ(s.disadvantage, 0);
}

// CN2. Breaks caught: hits from beyond 5 ft forced to crits; stunned targets forced
// (2024 Stunned has no auto-crit); NWN mode forcing crits.
TEST(MeleeHitsOnHelplessTargetsAreCriticals) {
  Conditions held;
  held.paralyzed = true;
  Conditions asleep;
  asleep.asleep = true;
  Conditions stunned;
  stunned.stunned = true;
  CHECK_EQ(ConditionForcesCritical(held, true, RuleVariant::kFaithful), true);
  CHECK_EQ(ConditionForcesCritical(asleep, true, RuleVariant::kFaithful), true);
  CHECK_EQ(ConditionForcesCritical(held, false, RuleVariant::kFaithful), false);
  CHECK_EQ(ConditionForcesCritical(stunned, true, RuleVariant::kFaithful), false);
  CHECK_EQ(ConditionForcesCritical(held, true, RuleVariant::kNwn), false);
}
