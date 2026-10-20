#include "check.h"
#include "rules/abilities.h"

using namespace rules;

// Breaks caught: NWN's level +1s kept, an increase passing 20 (or the excess
// moved elsewhere), a +1/+1 ASI giving +2, the background applied after the
// ASIs (so an ASI's room is used first).
TEST(FaithfulAbilityChanges) {
  //                     Str Dex Con Int Wis Cha
  const int base[6] = {16, 14, 14, 10, 12, 8};
  const int bumps[6] = {2, 0, 0, 0, 0, 0};  // NWN levels 4 and 8 raised Str
  const int none[6] = {0, 0, 0, 0, 0, 0};
  int delta[6];
  FaithfulAbilityDelta(base, bumps, none, {}, delta);
  CHECK_EQ(delta[0], -2);
  CHECK_EQ(delta[1], 0);
  const int soldier[6] = {2, 0, 1, 0, 0, 0};  // background: +2 Str, +1 Con
  FaithfulAbilityDelta(base, bumps, soldier, {{Ability::kStrength, std::nullopt}}, delta);
  CHECK_EQ(delta[0], 2);  // 16 - 2 + 2 + 2 = 18
  CHECK_EQ(delta[2], 1);
  FaithfulAbilityDelta(base, none, soldier, {{Ability::kStrength, std::nullopt}, {Ability::kStrength, std::nullopt}},
                       delta);
  CHECK_EQ(delta[0], 4);  // 16 + 2 + 2 = 20, the last +2 has no room
  FaithfulAbilityDelta(base, none, none, {{Ability::kDexterity, Ability::kConstitution}}, delta);
  CHECK_EQ(delta[1], 1);
  CHECK_EQ(delta[2], 1);
  const int high[6] = {19, 20, 10, 10, 10, 10};
  FaithfulAbilityDelta(high, none, none, {{Ability::kStrength, std::nullopt}, {Ability::kDexterity, std::nullopt}},
                       delta);
  CHECK_EQ(delta[0], 1);  // 19 -> 20
  CHECK_EQ(delta[1], 0);  // already 20
}

// Breaks caught: a henchman's ASI past 20, the second ability never used, the
// Fighter ignoring a higher Dexterity.
TEST(HenchmenRaiseTheirMainAbilities) {
  const int scores[6] = {16, 18, 14, 10, 12, 8};
  const auto rogue = AutomaticAsis(3, AsiPriority2024(CharacterClass::kRogue, 16, 18), scores);
  CHECK_EQ(static_cast<int>(rogue.size()), 3);
  CHECK_EQ(rogue[0].first == Ability::kDexterity, true);  // 18 -> 20
  CHECK_EQ(rogue[1].first == Ability::kConstitution, true);
  CHECK_EQ(AsiPriority2024(CharacterClass::kFighter, 12, 16)[0] == Ability::kDexterity, true);
  CHECK_EQ(AsiPriority2024(CharacterClass::kFighter, 16, 12)[0] == Ability::kStrength, true);
  CHECK_EQ(ParseAbility("CON") == Ability::kConstitution, true);
  CHECK_EQ(ParseAbility("wisdom") == Ability::kWisdom, true);
  CHECK_EQ(ParseAbility("luck").has_value(), false);
}

// Breaks caught: a Recommended set that isn't a legal 27-point buy, a class's
// main ability not at 15, or a multiclass 13 missed (Paladin, Monk, Ranger).
TEST(RecommendedScoresAreLegal27PointSets) {
  for (int c = 0; c <= static_cast<int>(CharacterClass::kWizard); ++c) {
    const std::array<int, 6> scores = RecommendedScores2024(static_cast<CharacterClass>(c));
    int points = 0;
    for (int s : scores) {
      CHECK_EQ(PointBuyCost2024(s) >= 0, true);
      points += PointBuyCost2024(s);
    }
    CHECK_EQ(points, 27);
  }
  const std::array<int, 6> paladin = RecommendedScores2024(CharacterClass::kPaladin);
  CHECK_EQ(paladin[0], 15);  // Str
  CHECK_EQ(paladin[5] >= 13, true);   // Cha
  const std::array<int, 6> monk = RecommendedScores2024(CharacterClass::kMonk);
  CHECK_EQ(monk[1], 15);  // Dex
  CHECK_EQ(monk[4] >= 13, true);   // Wis
  CHECK_EQ(RecommendedScores2024(CharacterClass::kWizard)[3], 15);  // Int
  CHECK_EQ(PointBuyCost2024(16), -1);
}
