#pragma once

// #37 Concentration (docs/PLAN.md "Phase 4"; owner, 2026-11-06: "37, all recommendations except 4A ..."). SRD 5.2:
// a spell whose duration is "Concentration, up to ..." lasts while its caster keeps Concentration - one such spell
// at a time (casting another ends the first). Taking damage calls for a Constitution saving throw: "The DC equals
// 10 or half the damage taken (round down), whichever number is higher, up to a maximum DC of 30." Concentration
// also ends when the caster has the Incapacitated condition or dies, and whenever it chooses (no action). Which of
// NWN's spells are SRD 5.2 Concentration spells is the table in concentration.cpp (1A: from the SRD's spell
// descriptions; an NWN-only spell has none until #39 rewrites or retires it).

#include <vector>

namespace rules {

constexpr int kConcentrationMinDc = 10;
constexpr int kConcentrationMaxDc = 30;

// The Constitution save's DC for `damage` taken (0: no damage, no save).
int ConcentrationSaveDc(int damage);

// 3A: a spell from a potion needs no Concentration (the potion's effect, not a cast); from a scroll, a wand or any
// other item, as when cast. `base_item`: the baseitems.2da row (-1: not from an item).
constexpr int kBaseItemPotions = 49;
constexpr int kBaseItemCraftedPotion = 104;
bool ItemCastNeedsConcentration(int base_item);

// Whether spells.2da row `spell` is an SRD 5.2 Concentration spell (a sub-radial variant as its master).
bool IsConcentrationSpell(int spell);
// Every such row, ascending.
const std::vector<int>& ConcentrationSpells();

}  // namespace rules
