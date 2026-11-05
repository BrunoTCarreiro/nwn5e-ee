#pragma once

#include <string>

// #36 Part 3a: the Prepare spells window and the lists behind it (prepare_spells.cpp).

// spellcasting faithful: the window, the Wizard's Spellbook item, the lists of creatures that aren't player characters.
void InstallPrepareSpells();

// A creature cast a spell from a class (rules_spell_slots.cpp, UnReadySpell): a Long Rest's change closes.
void PrepareSpellsNoteCast(void* creature);

// !test (test_commands.cpp): a rebuilt player character's 2024 caster lists start empty, as a new character's -
// the automatic level-ups (LevelUpHenchman) pick spells from the class package - its Spellbook too; the window
// opens. The line for the player ("" for a character with no spellcasting class).
std::string PrepareSpellsReset(void* creature);
