#include "rules/combat.h"

#include <algorithm>

namespace rules {

int AttackAbilityModifier(AttackAbility ability, int strength_modifier, int dexterity_modifier) {
  switch (ability) {
    case AttackAbility::kStrength: return strength_modifier;
    case AttackAbility::kDexterity: return dexterity_modifier;
    case AttackAbility::kFinesse: return std::max(strength_modifier, dexterity_modifier);
  }
  return strength_modifier;
}

int AttackBonus(int ability_modifier, int proficiency_bonus, bool proficient, int other_bonus) {
  return ability_modifier + (proficient ? proficiency_bonus : 0) + other_bonus;
}

AttackOutcome ResolveAttackRoll(int natural_roll, int attack_bonus, int armor_class, int critical_floor) {
  if (natural_roll <= 1) return AttackOutcome::kMiss;
  if (natural_roll >= 20) return AttackOutcome::kCritical;
  if (natural_roll + attack_bonus < armor_class) return AttackOutcome::kMiss;
  // A floor above 20 never matches here; the natural 20 already returned above.
  return natural_roll >= critical_floor ? AttackOutcome::kCritical : AttackOutcome::kHit;
}

int RollDamage(const std::vector<DamageDice>& dice, int modifier, bool critical, Dice& roller) {
  int total = modifier;
  for (const DamageDice& group : dice) {
    const int count = critical ? group.count * 2 : group.count;
    for (int i = 0; i < count; ++i) total += roller.Roll(group.sides);
  }
  return std::max(total, 0);
}

int AttacksPerAttackAction(const std::vector<ClassLevels>& classes) {
  int attacks = 1;
  for (const ClassLevels& entry : classes) {
    int from_class = 1;
    switch (entry.character_class) {
      case CharacterClass::kFighter:
        from_class = entry.levels >= 20 ? 4 : entry.levels >= 11 ? 3 : entry.levels >= 5 ? 2 : 1;
        break;
      case CharacterClass::kBarbarian:
      case CharacterClass::kMonk:
      case CharacterClass::kPaladin:
      case CharacterClass::kRanger:
        from_class = entry.levels >= 5 ? 2 : 1;
        break;
      default:
        break;
    }
    attacks = std::max(attacks, from_class);
  }
  return attacks;
}

}  // namespace rules
