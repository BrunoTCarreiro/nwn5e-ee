#include "rules/hit_points.h"

#include <algorithm>

namespace rules {

int HitDie2024(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian: return 12;
    case CharacterClass::kFighter:
    case CharacterClass::kPaladin:
    case CharacterClass::kRanger: return 10;
    case CharacterClass::kSorcerer:
    case CharacterClass::kWizard: return 6;
    default: return 8;  // Bard, Cleric, Druid, Monk, Rogue, Warlock
  }
}

int MaxHitPoints2024(const std::vector<int>& dice_in_order, int con_modifier) {
  int total = 0;
  for (size_t i = 0; i < dice_in_order.size(); ++i) {
    const int die = dice_in_order[i];
    const int roll = i == 0 ? die : die / 2 + 1;
    total += std::max(1, roll + con_modifier);
  }
  return total;
}

}  // namespace rules
