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
  CHECK_EQ(HasFightingStyleFeature2024(CharacterClass::kFighter, 1), true);
  CHECK_EQ(HasFightingStyleFeature2024(CharacterClass::kPaladin, 1), false);
  CHECK_EQ(HasFightingStyleFeature2024(CharacterClass::kPaladin, 2), true);
  CHECK_EQ(HasFightingStyleFeature2024(CharacterClass::kRanger, 2), true);
  CHECK_EQ(HasFightingStyleFeature2024(CharacterClass::kBarbarian, 20), false);
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
  CHECK_EQ(GrantableFeats2024().size(), 5u);
  CHECK_EQ(GrantableFeats2024()[0], kFeatSecondWind2);  // the record's order never changes
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 1}}).empty(), true);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 2}}).size(), 1u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 2}})[0], kFeatRecklessAttack);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 9}}).size(), 2u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 9}})[1], kFeatBrutalStrike);
}

// Breaks caught: part 3 at the wrong level (Reckless 2, Frenzy 3, Brutal
// Strike 9, Staggering / Sundering 13, 2d10 and two effects 17), Frenzy's dice
// not the Rage Damage bonus, a pick of a locked effect taking, a third effect
// kept at 17 instead of the oldest dropped, two effects below 17, the stored
// choice not coming back as it went in, "never chosen" not Hamstring Blow.
TEST(BarbarianPart3) {
  CHECK_EQ(HasRecklessAttack(1), false);
  CHECK_EQ(HasRecklessAttack(2), true);
  CHECK_EQ(HasFrenzy(2), false);
  CHECK_EQ(HasFrenzy(3), true);
  CHECK_EQ(FrenzyDice(2), 0);
  CHECK_EQ(FrenzyDice(3), 2);
  CHECK_EQ(FrenzyDice(9), 3);
  CHECK_EQ(FrenzyDice(16), 4);
  CHECK_EQ(HasBrutalStrike(8), false);
  CHECK_EQ(BrutalStrikeDice(9), 1);
  CHECK_EQ(BrutalStrikeDice(16), 1);
  CHECK_EQ(BrutalStrikeDice(17), 2);
  CHECK_EQ(BrutalStrikeEffectCount(16), 1);
  CHECK_EQ(BrutalStrikeEffectCount(17), 2);
  CHECK_EQ(BrutalEffectAvailable(BrutalEffect::kHamstring, 9), true);
  CHECK_EQ(BrutalEffectAvailable(BrutalEffect::kStaggering, 12), false);
  CHECK_EQ(BrutalEffectAvailable(BrutalEffect::kSundering, 13), true);
  // Never chosen: Hamstring Blow.
  BrutalChoice choice = DecodeBrutalChoice(0);
  CHECK_EQ(choice.first == BrutalEffect::kHamstring, true);
  // Below 17: one effect; a locked one refused.
  CHECK_EQ(PickBrutalEffect(&choice, BrutalEffect::kStaggering, 9) == BrutalPick::kLocked, true);
  CHECK_EQ(PickBrutalEffect(&choice, BrutalEffect::kForceful, 9) == BrutalPick::kChosen, true);
  CHECK_EQ(choice.first == BrutalEffect::kForceful && choice.second == BrutalEffect::kNone, true);
  CHECK_EQ(PickBrutalEffect(&choice, BrutalEffect::kForceful, 9) == BrutalPick::kRemoved, true);
  CHECK_EQ(choice.first == BrutalEffect::kNone, true);
  CHECK_EQ(ActiveBrutalEffects(choice, 9).empty(), true);  // off, not the default
  // 17: two, the oldest dropped for a third.
  PickBrutalEffect(&choice, BrutalEffect::kForceful, 17);
  PickBrutalEffect(&choice, BrutalEffect::kStaggering, 17);
  CHECK_EQ(ActiveBrutalEffects(choice, 17).size(), 2u);
  PickBrutalEffect(&choice, BrutalEffect::kSundering, 17);
  CHECK_EQ(choice.first == BrutalEffect::kStaggering && choice.second == BrutalEffect::kSundering, true);
  // Stored and read back; off stays off.
  const BrutalChoice back = DecodeBrutalChoice(EncodeBrutalChoice(choice));
  CHECK_EQ(back.first == choice.first && back.second == choice.second, true);
  CHECK_EQ(PickBrutalEffect(&choice, BrutalEffect::kNone, 17) == BrutalPick::kOff, true);
  CHECK_EQ(DecodeBrutalChoice(EncodeBrutalChoice(choice)).first == BrutalEffect::kNone, true);
  // Two chosen at 17, played at 13: only the newer.
  BrutalChoice two;
  two.first = BrutalEffect::kForceful;
  two.second = BrutalEffect::kSundering;
  CHECK_EQ(ActiveBrutalEffects(two, 13).size(), 1u);
  CHECK_EQ(ActiveBrutalEffects(two, 13)[0] == BrutalEffect::kSundering, true);
}

// Breaks caught: a Barbarian feature at the wrong level (Danger Sense 2, Fast
// Movement 5, Feral Instinct 7, Primal Champion 20), the Barbarian's table
// missing, Unarmored Defense lowering AC on a negative Con or stacking with a
// Monk's Wisdom (X5: the higher one).
TEST(BarbarianFeaturesByLevel) {
  CHECK_EQ(HasDangerSense(1), false);
  CHECK_EQ(HasDangerSense(2), true);
  CHECK_EQ(HasBarbarianFastMovement(4), false);
  CHECK_EQ(HasBarbarianFastMovement(5), true);
  CHECK_EQ(HasFeralInstinct(6), false);
  CHECK_EQ(HasFeralInstinct(7), true);
  CHECK_EQ(HasPrimalChampion(19), false);
  CHECK_EQ(HasPrimalChampion(20), true);
  CHECK_EQ(HasFeatureList(CharacterClass::kBarbarian), true);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kBarbarian, 1).size(), 3u);  // Rage, Unarmored Defense, Weapon Mastery
  CHECK_EQ(UnarmoredDefenseConBonus(3, 0), 3);
  CHECK_EQ(UnarmoredDefenseConBonus(-1, 0), 0);
  CHECK_EQ(UnarmoredDefenseConBonus(3, 2), 1);   // a Barbarian/Monk: Wis +2 already given, Con +3 the higher
  CHECK_EQ(UnarmoredDefenseConBonus(2, 4), 0);   // Wis the higher: nothing more
}

// Breaks caught: X1 removing a feat with a 2024 counterpart (Rage, Fast
// Movement) or missing an NWN-only one.
TEST(NwnOnlyClassFeatsAreTheOnesWithoutCounterparts) {
  const std::vector<int>& feats = NwnOnlyClassFeats();
  auto has = [&feats](int feat) {
    for (int f : feats) {
      if (f == feat) return true;
    }
    return false;
  };
  CHECK_EQ(has(195), true);   // Uncanny Dodge 1
  CHECK_EQ(has(255), true);   // Uncanny Dodge 6
  CHECK_EQ(has(196), true);   // Damage Reduction
  CHECK_EQ(has(1067), true);  // epic Damage Reduction
  CHECK_EQ(has(293), false);  // Rage: kept as the 2024 Rage
  CHECK_EQ(has(194), false);  // Fast Movement: kept, from Barbarian 5
}

// Breaks caught: Rage's uses or damage at the wrong level (2 / 3 / 4 / 5 / 6
// at 1 / 3 / 6 / 12 / 17; +2 / +3 / +4 at 1 / 9 / 16), a feat of the Rage
// chain missed or another feat taken for one, a part 2 feature at the wrong
// level, Relentless Rage's DC climbing by other than 5 or its hit points other
// than twice the level, Indomitable Might lowering a total.
TEST(BarbarianRage) {
  CHECK_EQ(RageUses2024(0), 0);
  CHECK_EQ(RageUses2024(1), 2);
  CHECK_EQ(RageUses2024(2), 2);
  CHECK_EQ(RageUses2024(3), 3);
  CHECK_EQ(RageUses2024(5), 3);
  CHECK_EQ(RageUses2024(6), 4);
  CHECK_EQ(RageUses2024(11), 4);
  CHECK_EQ(RageUses2024(12), 5);
  CHECK_EQ(RageUses2024(16), 5);
  CHECK_EQ(RageUses2024(17), 6);
  CHECK_EQ(RageUses2024(20), 6);
  CHECK_EQ(RageDamage2024(1), 2);
  CHECK_EQ(RageDamage2024(8), 2);
  CHECK_EQ(RageDamage2024(9), 3);
  CHECK_EQ(RageDamage2024(15), 3);
  CHECK_EQ(RageDamage2024(16), 4);
  CHECK_EQ(IsRageFeat(293), true);
  CHECK_EQ(IsRageFeat(326), true);
  CHECK_EQ(IsRageFeat(331), true);
  CHECK_EQ(IsRageFeat(325), false);
  CHECK_EQ(IsRageFeat(332), false);  // Damage Reduction 2
  CHECK_EQ(IsRageFeat(194), false);  // Fast Movement
  CHECK_EQ(HasPrimalKnowledge(2), false);
  CHECK_EQ(HasPrimalKnowledge(3), true);
  CHECK_EQ(HasMindlessRage(5), false);
  CHECK_EQ(HasMindlessRage(6), true);
  CHECK_EQ(HasInstinctivePounce(6), false);
  CHECK_EQ(HasInstinctivePounce(7), true);
  CHECK_EQ(HasRelentlessRage(10), false);
  CHECK_EQ(HasRelentlessRage(11), true);
  CHECK_EQ(HasPersistentRage(14), false);
  CHECK_EQ(HasPersistentRage(15), true);
  CHECK_EQ(HasIndomitableMight(17), false);
  CHECK_EQ(HasIndomitableMight(18), true);
  CHECK_EQ(RelentlessRageDc(0), 10);
  CHECK_EQ(RelentlessRageDc(1), 15);
  CHECK_EQ(RelentlessRageDc(2), 20);
  CHECK_EQ(RelentlessRageHitPoints(11), 22);
  CHECK_EQ(IndomitableMightTotal(12, 18), 18);
  CHECK_EQ(IndomitableMightTotal(21, 18), 21);
}
