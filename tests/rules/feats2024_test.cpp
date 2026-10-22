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
  CHECK_EQ(IsChoosableFeat2024(1200, true), false);  // an older ASI row (C7: the ASI's choices instead)
  CHECK_EQ(IsChoosableFeat2024(1206, true), false);
  CHECK_EQ(IsChoosableFeat2024(1207, true), false);
  CHECK_EQ(IsChoosableFeat2024(kAsiSplitFeatFirst, true), true);                              // ASI 1: +2 Str
  CHECK_EQ(IsChoosableFeat2024(kAsiSplitFeatFirst + kAsiSplits * kAsiFeatCount - 1, true), true);  // ASI 7
  CHECK_EQ(IsChoosableFeat2024(kAsiSplitFeatFirst + kAsiSplits * kAsiFeatCount, true), false);
  CHECK_EQ(IsChoosableFeat2024(kFeatSkilledAgainFirst + kSkilledRepeats - 1, true), true);  // Skilled again
  CHECK_EQ(IsChoosableFeat2024(kFeatSkilledAgainFirst + kSkilledRepeats, true), false);
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

// C7. Breaks caught: the 21 choices not the SRD's (a +1 pair missing or
// twice, +2 to two abilities), the ids drifting between slots, the next ASI
// not the only one offered, a choice that changes nothing (its abilities at
// 20) offered, a later Skilled not counted as Skilled.
TEST(AsiChoicesAreFeats) {
  int plus_two = 0, pairs = 0;
  for (int split = 0; split < kAsiSplits; ++split) {
    const AbilityIncrease increase = AsiSplit(split);
    if (increase.second) {
      ++pairs;
      CHECK_EQ(*increase.second != increase.first, true);
    } else {
      ++plus_two;
    }
    CHECK_EQ(AsiSplitIndex(increase), split);
    for (int slot = 1; slot <= kAsiFeatCount; ++slot) {
      const auto choice = AsiFeatFromId(AsiSplitFeat(slot, split));
      CHECK_EQ(choice.has_value(), true);
      CHECK_EQ(choice->slot, slot);
      CHECK_EQ(AsiSplitIndex(choice->increase), split);
      CHECK_EQ(BonusFeatGroupOf(AsiSplitFeat(slot, split)) == BonusFeatGroup::kGeneral, true);
    }
  }
  CHECK_EQ(plus_two, 6);
  CHECK_EQ(pairs, 15);
  CHECK_EQ(AsiSplitIndex({Ability::kCharisma, Ability::kStrength}), AsiSplitIndex({Ability::kStrength, Ability::kCharisma}));
  CHECK_EQ(AsiSplitFeat(0, 0), -1);
  CHECK_EQ(AsiSplitFeat(kAsiFeatCount + 1, 0), -1);
  CHECK_EQ(IsNextAsiChoice(AsiSplitFeat(1, 3), 0), true);
  CHECK_EQ(IsNextAsiChoice(AsiSplitFeat(2, 3), 0), false);
  CHECK_EQ(IsNextAsiChoice(AsiSplitFeat(2, 3), 1), true);
  CHECK_EQ(IsNextAsiChoice(AsiSplitFeat(1, 3), 1), false);
  const int scores[6] = {20, 14, 14, 10, 12, 8};
  CHECK_EQ(AsiChangesSomething({Ability::kStrength, std::nullopt}, scores), false);
  CHECK_EQ(AsiChangesSomething({Ability::kStrength, Ability::kConstitution}, scores), true);
  CHECK_EQ(IsSkilledFeat(kFeatSkilled) && IsSkilledFeat(kFeatSkilledAgainFirst), true);
  CHECK_EQ(BonusFeatGroupOf(kFeatAlert) == BonusFeatGroup::kOrigin, true);
}

// C7. Breaks caught: an Origin feat taking a mastery or style pick at
// level 1; a Human's Versatile pick (an Origin feat) refused; an Origin
// feat refused in an ASI level's general pick; two feats in one general pick.
TEST(OriginFeatsTakeTheGeneralPick) {
  BonusPicks fighter1 = BonusPicks2024(CharacterClass::kFighter, 1, true, true);
  BonusPicks taken;
  taken.origin = 1;
  CHECK_EQ(FitsBonusPicks(taken, fighter1), false);  // not a Human: no Versatile pick
  fighter1.origin = 1;                               // a Human
  CHECK_EQ(FitsBonusPicks(taken, fighter1), true);
  taken.origin = 2;
  CHECK_EQ(FitsBonusPicks(taken, fighter1), false);
  const BonusPicks fighter4 = BonusPicks2024(CharacterClass::kFighter, 4, true, true);
  BonusPicks at4;
  at4.origin = 1;
  CHECK_EQ(FitsBonusPicks(at4, fighter4), true);  // Alert instead of the ASI
  at4.general = 1;
  CHECK_EQ(FitsBonusPicks(at4, fighter4), false);
  at4.general = 0;
  at4.style = 1;
  CHECK_EQ(FitsBonusPicks(at4, fighter4), false);  // a style and an Origin feat: one general pick
}
