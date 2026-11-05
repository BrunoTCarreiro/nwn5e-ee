#pragma once

// #36 Part 3 (owner, 2026-11-04: "go with your recommendations. But for 3, can you make the window show up for wizard
// characters while they are resting? ..." - 1A 2A 3A; docs/PLAN.md "Phase 4"): what a 2024 caster may change on its
// lists of cantrips and prepared spells, and when (SRD 5.2: each class's Spellcasting feature and the Spell
// Preparation by Class table). core/prepare_spells.cpp (the Prepare spells window), core/rules_spell_slots.cpp.

#include <vector>

namespace rules {

// How much of a list may change: nothing (adding to fill it up is always allowed), one spell, or any.
enum class ListChange { kNone, kOne, kAny };

// After a Long Rest: Cleric, Druid and Wizard any prepared spell, Paladin and Ranger one; Bard and Sorcerer none
// (theirs is one at a level-up).
ListChange LongRestChange(int class_row);

// A cantrip replaced after a Long Rest: the Wizard's one (the other classes replace one at a level-up).
ListChange LongRestCantripChange(int class_row);

// The Wizard prepares from its spellbook; the other casters from their class's list.
bool PreparesFromSpellbook(int class_row);

// The Wizard's spellbook: six level 1 spells, then two more at each Wizard level after 1.
int SpellbookSpells2024(int wizard_level);

// The engine's cap on a class's known list (CNWClass::GetSpellsKnownPerLevel, which the known list - the cantrips and
// the prepared spells - is held to): the cantrips at level 0; at a level the class has slots for, the prepared total
// (the window holds the total); else 0. -1 for a class that isn't a 2024 caster.
int KnownSpellsCap2024(int class_row, int class_level, int spell_level);

// `after` may replace `before` (the list when the change opened): at most `limit` spells, and of `before`'s spells at
// most none (kNone), one (kOne) or any (kAny) gone - adding up to the limit is always fine. A `before` over the limit
// (NWN's list, or a count that fell) may change freely down to it.
bool ListChangeAllowed(ListChange change, const std::vector<int>& before, const std::vector<int>& after, int limit);

// Part 3b-1, NWN's Spells page (owner, 2026-11-05: "go with your recommendations"): a level's spells left to pick
// (`level_left`, the engine's - the prepared total minus what's known at that level) bounded by the total left: the
// prepared total minus what's known at all the class's levels. Never below 0.
int SpellsLeftOnPage(int level_left, int prepared_total, int known_total);

// The same as a cap on the level's known list (the quick level-up asks the cap itself): what's known at the level
// plus what the total still allows, at most `cap`.
int BoundedKnownCap(int cap, int known_here, int prepared_total, int known_total);

}  // namespace rules
