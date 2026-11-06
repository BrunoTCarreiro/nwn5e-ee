#include "check.h"
#include "rules/concentration.h"

#include <algorithm>

using namespace rules;

// #37: SRD 5.2's Concentration save - 10 or half the damage (rounded down), whichever is higher, at most 30.
TEST(ConcentrationSaveDc) {
  CHECK_EQ(ConcentrationSaveDc(0), 0);   // no damage: no save
  CHECK_EQ(ConcentrationSaveDc(-3), 0);
  CHECK_EQ(ConcentrationSaveDc(1), 10);
  CHECK_EQ(ConcentrationSaveDc(20), 10);
  CHECK_EQ(ConcentrationSaveDc(21), 10);  // half of 21, rounded down: 10
  CHECK_EQ(ConcentrationSaveDc(22), 11);
  CHECK_EQ(ConcentrationSaveDc(45), 22);
  CHECK_EQ(ConcentrationSaveDc(60), 30);
  CHECK_EQ(ConcentrationSaveDc(61), 30);  // half is 30
  CHECK_EQ(ConcentrationSaveDc(62), 30);  // at most 30
  CHECK_EQ(ConcentrationSaveDc(500), 30);
}

// 3A: a potion's spell needs no Concentration; a scroll's, a wand's or a cast one does.
TEST(ConcentrationFromItems) {
  CHECK_EQ(ItemCastNeedsConcentration(-1), true);                     // cast
  CHECK_EQ(ItemCastNeedsConcentration(kBaseItemPotions), false);      // 49
  CHECK_EQ(ItemCastNeedsConcentration(kBaseItemCraftedPotion), false);  // 104
  CHECK_EQ(ItemCastNeedsConcentration(75), true);                     // a scroll
  CHECK_EQ(ItemCastNeedsConcentration(46), true);                     // a wand
}

// 1A: the table - SRD 5.2's Concentration spells among NWN's rows.
TEST(ConcentrationSpellsAreSrd52) {
  CHECK_EQ(IsConcentrationSpell(6), true);     // Bless
  CHECK_EQ(IsConcentrationSpell(78), true);    // Haste
  CHECK_EQ(IsConcentrationSpell(83), true);    // Hold Person
  CHECK_EQ(IsConcentrationSpell(192), true);   // Web
  CHECK_EQ(IsConcentrationSpell(163), true);   // Silence
  CHECK_EQ(IsConcentrationSpell(172), true);   // Stoneskin
  CHECK_EQ(IsConcentrationSpell(138), true);   // Protection from Evil (a variant of row 321)
  CHECK_EQ(IsConcentrationSpell(321), true);
  CHECK_EQ(IsConcentrationSpell(485), true);   // Flesh to Stone (the table's last)
  CHECK_EQ(IsConcentrationSpell(4), true);     // Bestow Curse (the first)
  CHECK_EQ(IsConcentrationSpell(107), false);  // Magic Missile
  CHECK_EQ(IsConcentrationSpell(58), false);   // Fireball
  CHECK_EQ(IsConcentrationSpell(142), false);  // Raise Dead
  CHECK_EQ(IsConcentrationSpell(86), false);   // Identify
  CHECK_EQ(IsConcentrationSpell(325), false);  // a deleted row
  CHECK_EQ(IsConcentrationSpell(-1), false);
  const std::vector<int>& all = ConcentrationSpells();
  CHECK_EQ(all.size(), 84u);
  CHECK_EQ(std::is_sorted(all.begin(), all.end()), true);
  CHECK_EQ(std::adjacent_find(all.begin(), all.end()) == all.end(), true);  // no row twice
}
