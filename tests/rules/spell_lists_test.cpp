#include "check.h"
#include "rules/spell_lists.h"

using namespace rules;

// #36 Part 3 (owner, 2026-11-04: "go with your recommendations ..."): SRD 5.2's Spell Preparation by Class and the
// Spellcasting features. Breaks caught: a class's Long Rest change (the Paladin any, the Cleric one, the Bard any), the
// Wizard's cantrip not replaceable or another class's replaceable, the spellbook's start or growth, the engine cap off
// the cantrips or the prepared total or at a level without slots, a change over the limit, a removal under kNone, two
// under kOne, a list over its count kept from shrinking.
TEST(SpellLists) {
  CHECK_EQ(LongRestChange(2) == ListChange::kAny, true);   // Cleric
  CHECK_EQ(LongRestChange(3) == ListChange::kAny, true);   // Druid
  CHECK_EQ(LongRestChange(10) == ListChange::kAny, true);  // Wizard
  CHECK_EQ(LongRestChange(6) == ListChange::kOne, true);   // Paladin
  CHECK_EQ(LongRestChange(7) == ListChange::kOne, true);   // Ranger
  CHECK_EQ(LongRestChange(1) == ListChange::kNone, true);  // Bard: at a level-up
  CHECK_EQ(LongRestChange(9) == ListChange::kNone, true);  // Sorcerer: at a level-up
  CHECK_EQ(LongRestCantripChange(10) == ListChange::kOne, true);
  CHECK_EQ(LongRestCantripChange(2) == ListChange::kNone, true);
  CHECK_EQ(PreparesFromSpellbook(10), true);
  CHECK_EQ(PreparesFromSpellbook(2), false);
  CHECK_EQ(SpellbookSpells2024(1), 6);
  CHECK_EQ(SpellbookSpells2024(3), 10);
  CHECK_EQ(SpellbookSpells2024(20), 44);
  CHECK_EQ(SpellbookSpells2024(0), 0);
  // The engine's cap: cantrips at 0, the prepared total where there are slots.
  CHECK_EQ(KnownSpellsCap2024(2, 5, 0), 4);   // Cleric 5: four cantrips
  CHECK_EQ(KnownSpellsCap2024(2, 5, 3), 9);   // nine prepared, 3rd-level slots
  CHECK_EQ(KnownSpellsCap2024(2, 5, 4), 0);   // no 4th-level slots
  CHECK_EQ(KnownSpellsCap2024(6, 2, 1), 3);   // Paladin 2: three prepared
  CHECK_EQ(KnownSpellsCap2024(6, 2, 0), 0);   // no cantrips
  CHECK_EQ(KnownSpellsCap2024(4, 5, 1), -1);  // Fighter: not a caster
  CHECK_EQ(KnownSpellsCap2024(10, 1, 1), 6);  // Wizard 1: its spellbook's six (3b-2: the page picks the book)
  CHECK_EQ(KnownSpellsCap2024(10, 3, 2), 10);
  CHECK_EQ(KnownSpellsCap2024(10, 3, 3), 0);  // no 3rd-level slots at 3
  CHECK_EQ(KnownSpellsCap2024(10, 3, 0), 3);  // cantrips as before
  // A change against the list when it opened.
  const std::vector<int> before{1, 2, 3};
  CHECK_EQ(ListChangeAllowed(ListChange::kNone, before, {1, 2, 3, 4}, 4), true);   // filling up
  CHECK_EQ(ListChangeAllowed(ListChange::kNone, before, {1, 2, 4}, 4), false);     // a spell gone
  CHECK_EQ(ListChangeAllowed(ListChange::kOne, before, {1, 2, 4}, 4), true);
  CHECK_EQ(ListChangeAllowed(ListChange::kOne, before, {1, 5, 4}, 4), false);
  CHECK_EQ(ListChangeAllowed(ListChange::kAny, before, {5, 6, 7}, 4), true);
  CHECK_EQ(ListChangeAllowed(ListChange::kAny, before, {1, 2, 3, 4, 5}, 4), false);  // over the limit
  const std::vector<int> nwn_cantrips{1, 2, 3, 4, 5, 6, 7};  // a Wizard's NWN cantrips, 2024's three
  CHECK_EQ(ListChangeAllowed(ListChange::kNone, nwn_cantrips, {2, 4, 6}, 3), true);
  CHECK_EQ(ListChangeAllowed(ListChange::kNone, nwn_cantrips, {2, 4, 6, 7}, 3), false);
}

// Part 3b-1 (owner, 2026-11-05: "go with your recommendations"): NWN's Spells page with 2024's total. A Cleric 2->3
// knows 5 at level 1 of a total of 6: the page's level 1 (6 - 5 = 1 left) and level 2 (6 - 0 = 6) each come down to
// the 1 the total leaves. Breaks caught: no bound, the bound off by one, a negative count, the cap not bounded or
// above the engine's.
TEST(SpellsPageTotal) {
  CHECK_EQ(SpellsLeftOnPage(1, 6, 5), 1);
  CHECK_EQ(SpellsLeftOnPage(6, 6, 5), 1);   // level 2: 6 per level, 1 by the total
  CHECK_EQ(SpellsLeftOnPage(6, 6, 6), 0);   // the total reached
  CHECK_EQ(SpellsLeftOnPage(0, 6, 7), 0);   // over it (an old list): none, not negative
  CHECK_EQ(SpellsLeftOnPage(2, 9, 4), 2);   // the level's own count is the lower
  CHECK_EQ(BoundedKnownCap(6, 0, 6, 5), 1);  // level 2 for the quick level-up: 0 known + 1 the total allows
  CHECK_EQ(BoundedKnownCap(6, 5, 6, 5), 6);  // level 1: 5 known + 1
  CHECK_EQ(BoundedKnownCap(6, 5, 6, 6), 5);  // the total reached: no room
  CHECK_EQ(BoundedKnownCap(4, 0, 9, 0), 4);  // never above the engine's cap
}

// Part 3b-3 (2A): the level-up swaps on NWN's Spells page. Breaks caught: a second swap allowed, the Bard's or
// Sorcerer's spell swap refused, a Cleric's spell swap allowed, a Cleric's cantrip swap refused, a Paladin's or
// Wizard's allowed.
TEST(LevelUpSwaps) {
  CHECK_EQ(LevelUpSwapAllowed(1, false, 0), true);   // Bard: a spell
  CHECK_EQ(LevelUpSwapAllowed(9, false, 0), true);   // Sorcerer: a spell
  CHECK_EQ(LevelUpSwapAllowed(9, true, 0), true);    // Sorcerer: a cantrip
  CHECK_EQ(LevelUpSwapAllowed(9, false, 1), false);  // one only
  CHECK_EQ(LevelUpSwapAllowed(2, true, 0), true);    // Cleric: a cantrip
  CHECK_EQ(LevelUpSwapAllowed(3, true, 0), true);    // Druid: a cantrip
  CHECK_EQ(LevelUpSwapAllowed(2, false, 0), false);  // Cleric: no spell (after a Long Rest)
  CHECK_EQ(LevelUpSwapAllowed(2, true, 1), false);
  CHECK_EQ(LevelUpSwapAllowed(6, false, 0), false);  // Paladin: after a Long Rest
  CHECK_EQ(LevelUpSwapAllowed(10, true, 0), false);  // Wizard: its cantrip after a Long Rest
  CHECK_EQ(LevelUpSwapAllowed(10, false, 0), false); // a book spell never leaves
  // 0.83.0: the remove icon - a pick always; a spell known before only while its list's swap is left.
  CHECK_EQ(RemoveIconShown(true, false), true);
  CHECK_EQ(RemoveIconShown(true, true), true);
  CHECK_EQ(RemoveIconShown(false, true), true);
  CHECK_EQ(RemoveIconShown(false, false), false);  // its swap spent (or none): hidden
  // 0.83.1: the add icon - while a pick is left on the tab.
  CHECK_EQ(AddIconShown(2), true);
  CHECK_EQ(AddIconShown(1), true);
  CHECK_EQ(AddIconShown(0), false);
  // 0.83.3: a Wizard's level-up page - its whole book at the tab's level, then the picks.
  CHECK_EQ(WizardPageRows({10, 11, 12}, {11, 20}) == std::vector<int>({10, 11, 12, 20}), true);
  CHECK_EQ(WizardPageRows({}, {5, 6}) == std::vector<int>({5, 6}), true);       // no book yet: the picks
  CHECK_EQ(WizardPageRows({7, 8}, {}) == std::vector<int>({7, 8}), true);       // the book alone
  CHECK_EQ(WizardPageRows({7}, {20, 20, 7}) == std::vector<int>({7, 20}), true);  // each once
}

// Part 3b-2 (owner, 2026-11-05: "Not a fan of the spellbook appearing AFTER the class configuration can it be
// integrated into the character creation somehow?"): the Spells page's picks into a Wizard's book and prepared
// list. Breaks caught: a pick left out of the book, the prepared over its count, an old prepared spell dropped for a
// pick, the picks' order lost.
TEST(WizardPicks) {
  // Creation: six picks, four prepared.
  WizardLists made = TakeWizardPicks({}, {11, 12, 13, 14, 15, 16}, 4);
  CHECK_EQ(made.book == std::vector<int>({11, 12, 13, 14, 15, 16}), true);
  CHECK_EQ(made.prepared == std::vector<int>({11, 12, 13, 14}), true);
  // A level-up: four prepared from a book of six, two picks, one more prepared place.
  WizardLists up = TakeWizardPicks({11, 12, 13, 14, 15, 16}, {13, 14, 15, 16, 21, 22}, 5);
  CHECK_EQ(up.book == std::vector<int>({11, 12, 13, 14, 15, 16, 21, 22}), true);
  CHECK_EQ(up.prepared == std::vector<int>({13, 14, 15, 16, 21}), true);
  // Nothing picked: the lists stay.
  WizardLists same = TakeWizardPicks({11, 12}, {11}, 4);
  CHECK_EQ(same.book == std::vector<int>({11, 12}), true);
  CHECK_EQ(same.prepared == std::vector<int>({11}), true);
}
