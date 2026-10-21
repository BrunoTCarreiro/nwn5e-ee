#pragma once

// Weapon size and the Heavy property (rules_weapon_size.cpp, `weapon_size`).

#include <string>

// The 2024 Heavy property, faithful: 1 (a Disadvantage source) when the
// attacker's current weapon is Heavy and its Strength (melee) or Dexterity
// (ranged) is below 13, else 0; `note` gets the reason for the log.
int HeavyWeaponDisadvantage(void* attacker, bool ranged, std::string* note);
