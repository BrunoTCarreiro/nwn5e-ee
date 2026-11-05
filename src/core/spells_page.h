#pragma once

// #36 Part 3b-1: NWN's Spells page at creation and level-up with 2024's counts (spells_page.cpp).

#include <stdint.h>

// The client hooks (spellcasting faithful; rules_spell_slots.cpp's install). False, logged, when unmapped.
bool InstallSpellsPage();

// rules_spell_slots.cpp's GetSpellsKnownPerLevel answer, bounded by the 2024 total when the client's quick
// level-up (its return address `caller`) asks; else `cap` as it is.
int SpellsPageCap(uintptr_t caller, int spell_level, int cap);
