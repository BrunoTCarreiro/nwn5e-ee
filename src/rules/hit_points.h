#pragma once

// Phase 3 CM3 (PLAN.md): 2024 hit points, SRD 5.2.

#include <vector>

#include "rules/combat.h"

namespace rules {

// Barbarian d12; Fighter, Paladin, Ranger d10; Bard, Cleric, Druid, Monk,
// Rogue, Warlock d8; Sorcerer, Wizard d6.
int HitDie2024(CharacterClass character_class);

// Maximum hit points for levels with these hit dice, in the order taken: the
// first level's die at its maximum, every later level the fixed average
// (die / 2 + 1); each level adds `con_modifier`, at least 1 per level.
int MaxHitPoints2024(const std::vector<int>& dice_in_order, int con_modifier);

}  // namespace rules
