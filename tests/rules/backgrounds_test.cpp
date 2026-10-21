#include "check.h"
#include "rules/backgrounds.h"

using namespace rules;

// Breaks caught: an ability outside the background accepted, +2 / +1 vs
// +1 / +1 / +1 mixed up, a repeated ability, the saved forms not round-tripping,
// a background's data drifting from SRD 5.2.
TEST(BackgroundsAre2024) {
  int amounts[6];
  CHECK_EQ(BackgroundIncreases(Background::kSoldier, {Ability::kStrength, Ability::kConstitution}, amounts).empty(),
           true);
  CHECK_EQ(amounts[0], 2);
  CHECK_EQ(amounts[2], 1);
  CHECK_EQ(amounts[1], 0);
  CHECK_EQ(BackgroundIncreases(Background::kSoldier,
                               {Ability::kStrength, Ability::kDexterity, Ability::kConstitution}, amounts)
               .empty(),
           true);
  CHECK_EQ(amounts[0], 1);
  CHECK_EQ(amounts[1], 1);
  CHECK_EQ(BackgroundIncreases(Background::kSoldier, {Ability::kWisdom, Ability::kStrength}, amounts).empty(), false);
  CHECK_EQ(BackgroundIncreases(Background::kSage, {Ability::kIntelligence, Ability::kIntelligence}, amounts).empty(),
           false);
  CHECK_EQ(BackgroundIncreases(Background::kCriminal, {Ability::kDexterity}, amounts).empty(), false);
  CHECK_EQ(ParseBackground("criminal") == Background::kCriminal, true);
  CHECK_EQ(ParseBackground("noble").has_value(), false);
  CHECK_EQ(Has(BackgroundData(Background::kCriminal)->skills, Skill2024::kStealth), true);
  CHECK_EQ(Has(BackgroundData(Background::kCriminal)->skills, Skill2024::kThievesTools), true);
  CHECK_EQ(Has(BackgroundData(Background::kSage)->skills, Skill2024::kArcana), true);
  const int split[6] = {0, 2, 1, 0, 0, 0};
  int back[6];
  DecodeAmounts(EncodeAmounts(split), back);
  CHECK_EQ(back[1], 2);
  CHECK_EQ(back[2], 1);
  const auto asi = DecodeAsi(EncodeAsi({Ability::kCharisma, Ability::kWisdom}));
  CHECK_EQ(asi.has_value(), true);
  CHECK_EQ(asi->first == Ability::kCharisma, true);
  CHECK_EQ(asi->second == Ability::kWisdom, true);
  CHECK_EQ(DecodeAsi(EncodeAsi({Ability::kStrength, std::nullopt}))->second.has_value(), false);
  CHECK_EQ(DecodeAsi(0).has_value(), false);
}

// Breaks caught (background feats, chosen at creation): a row mapped to the
// wrong background or split, two rows for one choice, a split outside the
// background's abilities, rows outside the 28 read as a background.
TEST(BackgroundFeatsCoverEverySplit) {
  int seen[kBackgroundFeatCount] = {};
  for (const BackgroundInfo& info : Backgrounds2024()) {
    for (int split = 0; split < kBackgroundSplits; ++split) {
      const int feat = BackgroundFeat(info.background, split);
      CHECK_EQ(feat >= kBackgroundFeatFirst && feat < kBackgroundFeatFirst + kBackgroundFeatCount, true);
      ++seen[feat - kBackgroundFeatFirst];
      const auto choice = BackgroundFromFeat(feat);
      CHECK_EQ(choice.has_value(), true);
      CHECK_EQ(choice->background == info.background, true);
      int amounts[6];
      CHECK_EQ(BackgroundIncreases(info.background, choice->chosen, amounts).empty(), true);
      int total = 0;
      for (int a : amounts) total += a;
      CHECK_EQ(total, 3);
    }
  }
  for (int count : seen) CHECK_EQ(count, 1);
  // Soldier (Str, Dex, Con): split 1 = Str +2, Con +1; split 6 = +1 each.
  int amounts[6];
  const auto soldier = BackgroundFromFeat(BackgroundFeat(Background::kSoldier, 1));
  CHECK_EQ(BackgroundIncreases(soldier->background, soldier->chosen, amounts).empty(), true);
  CHECK_EQ(amounts[0], 2);
  CHECK_EQ(amounts[2], 1);
  const auto even = BackgroundFromFeat(BackgroundFeat(Background::kSoldier, 6));
  CHECK_EQ(even->chosen.size(), 3u);
  CHECK_EQ(BackgroundFromFeat(kBackgroundFeatFirst - 1).has_value(), false);
  CHECK_EQ(BackgroundFromFeat(kBackgroundFeatFirst + kBackgroundFeatCount).has_value(), false);
  CHECK_EQ(BackgroundFeat(Background::kNone, 0), -1);
  CHECK_EQ(BackgroundFeat(Background::kSage, kBackgroundSplits), -1);
}
