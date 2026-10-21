#include "check.h"
#include "rules/feats2024.h"
#include "rules/weapons.h"

using namespace rules;

// Breaks caught: an NWN feat (Power Attack, Dodge, Weapon Focus) still
// offered in faithful, a background feat offered on the Feats page (the
// Background page gives it), a built 2024 feat (Alert, an ASI row, a style)
// left out, the ranges' ends off by one, a Weapon Mastery feat offered when
// masteries aren't picked as feats.
TEST(ChoosableFeatsAre2024) {
  CHECK_EQ(IsChoosableFeat2024(377, true), true);   // Alert (Improved Initiative)
  CHECK_EQ(IsChoosableFeat2024(1200, true), true);  // Ability Score Improvement 1
  CHECK_EQ(IsChoosableFeat2024(1206, true), true);  // ... 7
  CHECK_EQ(IsChoosableFeat2024(1207, true), false);
  CHECK_EQ(IsChoosableFeat2024(1210, true), true);  // Archery
  CHECK_EQ(IsChoosableFeat2024(1213, true), true);  // Two-Weapon Fighting
  CHECK_EQ(IsChoosableFeat2024(1214, true), false);
  CHECK_EQ(IsChoosableFeat2024(1220, true), false);  // Second Wind (granted)
  CHECK_EQ(IsChoosableFeat2024(1230, true), false);  // a background feat
  CHECK_EQ(IsChoosableFeat2024(28, true), false);    // Power Attack
  CHECK_EQ(IsChoosableFeat2024(10, true), false);    // Dodge
  CHECK_EQ(IsChoosableFeat2024(0, true), false);
  CHECK_EQ(IsChoosableFeat2024(WeaponMasteryFeat(1), true), true);  // Weapon Mastery: Longsword
  CHECK_EQ(IsChoosableFeat2024(WeaponMasteryFeat(1), false), false);
}

// Breaks caught: the ids drifting from the generated rows (feat.mastery.txt:
// Dagger 1260, Whip 1299), a magic staff not counted as a quarterstaff, an id
// past the table treated as a weapon.
TEST(WeaponMasteryFeatIds) {
  CHECK_EQ(WeaponMasteryFeat(22), 1260);   // Dagger, first
  CHECK_EQ(WeaponMasteryFeat(1), 1272);    // Longsword
  CHECK_EQ(WeaponMasteryFeat(111), 1299);  // Whip, last
  CHECK_EQ(WeaponMasteryFeat(45), WeaponMasteryFeat(50));  // magic staff = quarterstaff
  CHECK_EQ(WeaponMasteryFeat(16), -1);     // armor
  CHECK_EQ(WeaponKindOfMasteryFeat(1272), 1);
  CHECK_EQ(WeaponKindOfMasteryFeat(1300), -1);
  CHECK_EQ(WeaponKindOfMasteryFeat(1259), -1);
  for (int kind : NwnMasteryWeapons()) CHECK_EQ(WeaponKindOfMasteryFeat(WeaponMasteryFeat(kind)), kind);
}

// Breaks caught: a level-1 Fighter allowed 4 masteries and no style, or two
// styles; the Fighter's level-4 pick not split ASI + 1 mastery; a style not
// allowed in the ASI pick (2024: a Fighting Style feat is a general feat);
// a class without mastery given mastery picks; masteries counted when off.
TEST(BonusPicksPerClassLevel) {
  const BonusPicks fighter1 = BonusPicks2024(CharacterClass::kFighter, 1, true, true);
  CHECK_EQ(fighter1.general, 0);
  CHECK_EQ(fighter1.style, 1);
  CHECK_EQ(fighter1.mastery, 3);
  CHECK_EQ(fighter1.Total(), 4);
  const BonusPicks fighter4 = BonusPicks2024(CharacterClass::kFighter, 4, true, true);
  CHECK_EQ(fighter4.general, 1);
  CHECK_EQ(fighter4.mastery, 1);
  CHECK_EQ(fighter4.style, 0);
  CHECK_EQ(BonusPicks2024(CharacterClass::kFighter, 7, true, true).style, 1);  // Champion's second style
  CHECK_EQ(BonusPicks2024(CharacterClass::kBarbarian, 1, true, true).mastery, 2);
  CHECK_EQ(BonusPicks2024(CharacterClass::kBarbarian, 10, true, true).mastery, 1);
  CHECK_EQ(BonusPicks2024(CharacterClass::kRogue, 1, true, true).mastery, 2);
  CHECK_EQ(BonusPicks2024(CharacterClass::kWizard, 1, true, true).Total(), 0);
  CHECK_EQ(BonusPicks2024(CharacterClass::kFighter, 1, true, false).Total(), 1);  // masteries not as feats

  BonusPicks taken;
  taken.mastery = 3;
  taken.style = 1;
  CHECK_EQ(FitsBonusPicks(taken, fighter1), true);
  taken.mastery = 4;
  taken.style = 0;
  CHECK_EQ(FitsBonusPicks(taken, fighter1), false);
  taken.mastery = 2;
  taken.style = 2;
  CHECK_EQ(FitsBonusPicks(taken, fighter1), false);
  BonusPicks at4;
  at4.style = 1;
  at4.mastery = 1;
  CHECK_EQ(FitsBonusPicks(at4, fighter4), true);  // a style in the ASI pick
  at4.general = 1;
  CHECK_EQ(FitsBonusPicks(at4, fighter4), false);
  BonusPicks two_asi;
  two_asi.general = 2;
  CHECK_EQ(FitsBonusPicks(two_asi, fighter4), false);
}

// Breaks caught: Savage Attacker choosing set by set on a critical hit (each
// half's better roll kept: 7 + 12 = 19 here, more than either real roll)
// instead of one whole roll; a tie not keeping the first roll.
TEST(SavageAttackerKeepsOneWholeRoll) {
  CHECK_EQ(KeepSecondSavageRoll({2, 12}, {7, 6}), false);  // 14 against 13: the first roll, all of it
  CHECK_EQ(KeepSecondSavageRoll({7, 6}, {2, 12}), true);
  CHECK_EQ(KeepSecondSavageRoll({5}, {9}), true);           // a plain hit: one set
  CHECK_EQ(KeepSecondSavageRoll({8}, {8}), false);          // a tie keeps the first
}
