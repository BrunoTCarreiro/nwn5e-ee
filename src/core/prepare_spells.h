#pragma once

// #36 Part 3a: the Prepare spells window and the lists behind it (prepare_spells.cpp).

// spellcasting faithful: the window, the Wizard's Spellbook item, the lists of creatures that aren't player characters.
void InstallPrepareSpells();

// A creature cast a spell from a class (rules_spell_slots.cpp, UnReadySpell): a Long Rest's change closes.
void PrepareSpellsNoteCast(void* creature);
