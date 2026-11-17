#pragma once

#include <string>
#include <vector>

// #36 Part 3a: the Prepare spells window and the lists behind it (prepare_spells.cpp).

// spellcasting faithful: the window, the Wizard's Spellbook item, the lists of creatures that aren't player characters.
void InstallPrepareSpells();

// A creature cast a spell from a class (rules_spell_slots.cpp, UnReadySpell): a Long Rest's change closes.
void PrepareSpellsNoteCast(void* creature);

// !test (test_commands.cpp): a rebuilt player character's 2024 caster lists start empty, as a new character's -
// the automatic level-ups (LevelUpHenchman) pick spells from the class package - its Spellbook too; the window
// opens. #169: with the profile's `spells` / `spellbook` names, the lists are set to them instead
// (rules::PickProfileSpells) and the window stays shut. The lines for the player (none for a character with no
// spellcasting class).
std::vector<std::string> PrepareSpellsReset(void* creature, const std::vector<std::string>& spells = {},
                                            const std::vector<std::string>& spellbook = {});

// #169 (0.99.71; the owner's 0.99.70 log: "set by the profile", then "window opened ... a Long Rest's change open"
// 0.1 s later): !test's rest after the rebuild (ForceRest) ends in RestDone, which opens the window for a rest that
// had none - so after a profile that set the spells, that window is dropped. The Long Rest's change stays open (the
// rebuild's rest is one): the radial's Prepare Spells still changes them.
void PrepareSpellsKeepShut(void* creature);

// Part 3b-2: the player character's Spellbook (single player: the one player character; its book's spell ids), for
// the client's Spells page. Empty when there is none.
std::vector<int> PrepareSpellsPlayerBook();
