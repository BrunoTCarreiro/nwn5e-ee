#include "check.h"
#include "rules/class_features.h"

#include <string>

using namespace rules;

// Breaks caught: features past the character's level listed, the subclass
// missing at 3, a class without a brief showing a list, the SRD subclass
// names drifting (they are SRD 5.2's one per class).
TEST(ClassFeaturesByLevel) {
  CHECK_EQ(ClassFeatures2024(CharacterClass::kFighter, 1).size(), 3u);
  bool champion = false, extra_attack = false;
  for (const ClassFeature& f : ClassFeatures2024(CharacterClass::kFighter, 5)) {
    champion = champion || std::string(f.name) == "Champion";
    extra_attack = extra_attack || std::string(f.name) == "Extra Attack";
  }
  CHECK_EQ(champion, true);
  CHECK_EQ(extra_attack, true);
  for (const ClassFeature& f : ClassFeatures2024(CharacterClass::kFighter, 4)) CHECK_EQ(f.level <= 4, true);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kFighter, 20).size(), 29u);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kWizard, 5).empty(), true);
  CHECK_EQ(HasFeatureList(CharacterClass::kFighter), true);
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kFighter)) == "Champion", true);
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kRogue)) == "Thief", true);
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kWizard)) == "Evoker", true);
}

// Breaks caught: Champion crits before 3, 18 before 15, the gates off by one.
TEST(FighterFeatureLevels) {
  CHECK_EQ(ChampionCriticalFloor(0), 20);
  CHECK_EQ(ChampionCriticalFloor(2), 20);
  CHECK_EQ(ChampionCriticalFloor(3), 19);
  CHECK_EQ(ChampionCriticalFloor(14), 19);
  CHECK_EQ(ChampionCriticalFloor(15), 18);
  CHECK_EQ(HasRemarkableAthlete(2), false);
  CHECK_EQ(HasRemarkableAthlete(3), true);
  CHECK_EQ(HasStudiedAttacks(12), false);
  CHECK_EQ(HasStudiedAttacks(13), true);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kFighter, 1), true);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kFighter, 7), true);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kFighter, 4), false);
  CHECK_EQ(IsFightingStyleLevel2024(CharacterClass::kPaladin, 2), false);  // with the Paladin's brief
}

// Breaks caught: Indomitable's uses at the wrong levels, Bloodied off by one
// (exactly half is Bloodied), Heroic Rally before 18.
TEST(IndomitableAndSurvivor) {
  CHECK_EQ(IndomitableUses(8), 0);
  CHECK_EQ(IndomitableUses(9), 1);
  CHECK_EQ(IndomitableUses(13), 2);
  CHECK_EQ(IndomitableUses(17), 3);
  CHECK_EQ(IsBloodied(10, 20), true);
  CHECK_EQ(IsBloodied(11, 20), false);
  CHECK_EQ(IsBloodied(5, 0), false);
  CHECK_EQ(HeroicRallyHitPoints(3), 8);
  CHECK_EQ(HasHeroicRally(17), false);
  CHECK_EQ(HasHeroicRally(18), true);
}

// Breaks caught: Great Weapon Fighting raising a 3 or lowering a high roll,
// a one-handed or double weapon counting, a versatile one missing.
TEST(GreatWeaponFightingWeapons) {
  CHECK_EQ(GreatWeaponFightingDie(1), 3);
  CHECK_EQ(GreatWeaponFightingDie(2), 3);
  CHECK_EQ(GreatWeaponFightingDie(3), 3);
  CHECK_EQ(GreatWeaponFightingDie(6), 6);
  CHECK_EQ(IsTwoHandedWeapon2024(13), true);   // greatsword
  CHECK_EQ(IsTwoHandedWeapon2024(12), false);  // two-bladed sword (a double weapon)
  CHECK_EQ(IsVersatileWeapon2024(1), true);    // longsword
  CHECK_EQ(IsVersatileWeapon2024(4), false);   // light flail: one-handed
  CHECK_EQ(IsTwoHandedWeapon2024(1) || IsVersatileWeapon2024(22), false);  // longsword not two-handed; dagger
}

// Breaks caught: Second Wind's uses not growing at 4 / 10, two tiers at once,
// a non-Fighter getting it.
TEST(GrantedFeatsFollowClassLevels) {
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 1}})[0], kFeatSecondWind2);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 4}})[0], kFeatSecondWind3);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 10}})[0], kFeatSecondWind4);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 9}}).size(), 1u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kWizard, 5}}).empty(), true);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kWizard, 3}, {CharacterClass::kFighter, 2}})[0], kFeatSecondWind2);
  CHECK_EQ(GrantableFeats2024().size(), 3u);
}
