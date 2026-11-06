#pragma once

// #36 Part 3b-1: NWN's Spells page at creation and level-up with 2024's counts (spells_page.cpp).

#include <stdint.h>

// The client hooks (spellcasting faithful; rules_spell_slots.cpp's install). False, logged, when unmapped.
bool InstallSpellsPage();

// rules_spell_slots.cpp's GetSpellsKnownPerLevel answer, bounded by the 2024 total when the client's quick
// level-up (its return address `caller`) asks; else `cap` as it is.
int SpellsPageCap(uintptr_t caller, int spell_level, int cap);
// 0.84.1: whether a client class slot's known list holds `spell` at `spell_level` (a 2024 caster's known list is its
// cantrips and prepared spells); true when the list can't be read.
bool ClientKnowsSpell(void* client_stats, int slot, int spell_level, uint32_t spell);
