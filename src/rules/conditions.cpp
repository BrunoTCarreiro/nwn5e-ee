#include "rules/conditions.h"

namespace rules {

namespace {

bool StrengthOrDexterity(Ability ability) {
  return ability == Ability::kStrength || ability == Ability::kDexterity;
}

}  // namespace

SaveOutcome ConditionSaveOutcome(const Conditions& creature, Ability ability, RuleVariant variant) {
  if (variant == RuleVariant::kOff || !StrengthOrDexterity(ability)) return SaveOutcome::kRoll;
  if (creature.stunned) return SaveOutcome::kAutoFail;
  if (variant == RuleVariant::kFaithful && (creature.paralyzed || creature.asleep)) return SaveOutcome::kAutoFail;
  return SaveOutcome::kRoll;
}

int ConditionSaveDisadvantage(const Conditions& creature, Ability ability, RuleVariant variant) {
  return variant == RuleVariant::kFaithful && creature.restrained && ability == Ability::kDexterity ? 1 : 0;
}

RollSources ConditionAttackSources(const Conditions& attacker, const Conditions& target, RuleVariant variant) {
  RollSources sources;
  if (variant == RuleVariant::kOff) return sources;
  if (target.stunned || target.paralyzed || target.asleep) ++sources.advantage;
  if (variant != RuleVariant::kFaithful) return sources;
  if (target.restrained) ++sources.advantage;
  if (target.concealed) ++sources.disadvantage;
  if (attacker.restrained) ++sources.disadvantage;
  if (attacker.poisoned) ++sources.disadvantage;
  if (attacker.frightened_source_in_sight) ++sources.disadvantage;
  return sources;
}

bool ConditionForcesCritical(const Conditions& target, bool within_five_feet, RuleVariant variant) {
  return variant == RuleVariant::kFaithful && within_five_feet && (target.paralyzed || target.asleep);
}

}  // namespace rules
