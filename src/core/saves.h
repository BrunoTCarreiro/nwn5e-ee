#pragma once

// 2024 saving throws (rules_saves.cpp: the save values; rules_save_roll.cpp:
// the roll, the DCs) — port of 1.69's C1–C8.

#include <stdint.h>

#include <optional>

#include "rules/saves.h"

// rules_save_roll.cpp
// C1: the ability the save being rolled for `stats` uses, when its spell asks
// for another one than the NWN save's default (faithful).
std::optional<rules::Ability> SaveAbilityOverride(void* stats);
// Whether `stats`' creature is inside its own save roll (the roll asks the
// save getters for base values; Aura of Protection still counts there).
bool SaveRollInProgressFor(void* stats);
// A DC already on the 2024 scale (D4 Topple, SK6 Shove), so the save roll
// does not convert it: remembered like a spell DC for `caster`.
void RememberSaveDc2024(uint32_t caster, int dc);
// SK6: the next save rolls on `ability` (nullopt: back to the save's own).
void SetForcedSaveAbility(std::optional<rules::Ability> ability);

// R8 (Rogue Evasion): the ability of the save `creature` rolled in the last
// 200 ms (after C1's spell substitution and any forced ability), or none - the
// engine's Reflex-damage command asks for Evasion right after its save.
std::optional<rules::Ability> RecentSaveAbility(void* creature);

// rules_saves.cpp
// A creature's 2024 save bonus for `ability`, as !saves shows it.
int SaveBonusFor(void* stats, rules::Ability ability);

// SK10: the casting part of a creature's 2024 spell attack bonus for the spell
// it is casting (its last spell): PB + the casting ability of the class slot
// or item it casts from — the C5 DC minus 8. False when not known.
bool SpellAttackCasting(void* creature, int* bonus, const char** source);
