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
