#include "check.h"
#include "rules/class_features.h"
#include "rules/weapons.h"

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
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kFighter, 9}}).size(), 2u);  // #53: and Action Surge
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kWizard, 5}}).empty(), true);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kWizard, 3}, {CharacterClass::kFighter, 2}})[0], kFeatSecondWind2);
  CHECK_EQ(GrantableFeats2024().size(), 30u);  // #54 Disengage (bit 19); #56 Dodge, Dash, Help; #53 five; #55 two
  CHECK_EQ(GrantableFeats2024()[15], kFeatOpenHandTechnique);  // Monk parts 4 / 5: appended
  CHECK_EQ(GrantableFeats2024()[16], kFeatDeflectArrows);
  CHECK_EQ(GrantableFeats2024()[17], kFeatWholenessOfBody);
  CHECK_EQ(GrantableFeats2024()[18], kFeatQuiveringPalm);
  CHECK_EQ(GrantableFeats2024()[12], kFeatPatientDefense);  // Monk part 3: appended, never inserted
  CHECK_EQ(GrantableFeats2024()[13], kFeatStepOfTheWind);
  CHECK_EQ(GrantableFeats2024()[14], kFeatSuperiorDefense);
  CHECK_EQ(GrantableFeats2024()[9], kFeatImprovedEvasion);
  CHECK_EQ(GrantableFeats2024()[10], kFeatCunningStrike);
  CHECK_EQ(GrantableFeats2024()[11], kFeatDeviousStrikes);
  CHECK_EQ(GrantableFeats2024()[0], kFeatSecondWind2);  // the record's order never changes
  CHECK_EQ(GrantableFeats2024()[4], kFeatBrutalStrike);
  CHECK_EQ(GrantableFeats2024()[5], kFeatKnockdownShove);
  CHECK_EQ(GrantableFeats2024()[6], kFeatShovePush);
  CHECK_EQ(GrantableFeats2024()[7], kFeatIntimidatingPresence);
  CHECK_EQ(GrantableFeats2024()[8], kFeatGrapple);
  // BZ4: Intimidating Presence at Barbarian 14 (the Berserker, the SRD's one subclass).
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 13}}).size(), 2u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 14}}).size(), 3u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 14}})[2], kFeatIntimidatingPresence);
  CHECK_EQ(HasRetaliation(9), false);
  CHECK_EQ(HasRetaliation(10), true);
  CHECK_EQ(HasIntimidatingPresence(13), false);
  CHECK_EQ(HasIntimidatingPresence(14), true);
  CHECK_EQ(IntimidatingPresenceDc(4, 5), 17);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 1}}).empty(), true);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 2}}).size(), 1u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 2}})[0], kFeatRecklessAttack);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 9}}).size(), 2u);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kBarbarian, 9}})[1], kFeatBrutalStrike);
}

// G1: everyone can Shove in 2024 - feat 23 for every player character while the
// Shove is faithful, granted only when missing (one a character took stays
// its own in NWN mode). Breaks caught: granted in NWN mode, a class feat
// marked only-when-missing (NWN mode would then keep a stray nwn5e feat).
TEST(EveryoneCanShove) {
  CHECK_EQ(FeatsEveryoneHas2024(true, false).size(), 3u);
  CHECK_EQ(FeatsEveryoneHas2024(true, false)[0], kFeatKnockdownShove);
  CHECK_EQ(FeatsEveryoneHas2024(true, false)[1], kFeatShovePush);
  CHECK_EQ(FeatsEveryoneHas2024(true, false)[2], kFeatGrapple);
  // #54 / #56: Disengage, Dodge and Dash for everyone while the actions are faithful (whatever the Shove's mode);
  // #39 build 2b: Rouse (1324, the Help action renamed) too - the Help house rule only adds its henchman use.
  const std::vector<int> everyone = FeatsEveryoneHas2024(true, true);
  CHECK_EQ(everyone.size(), 7u);
  CHECK_EQ(everyone.size() == 7u && everyone[3] == kFeatDisengage && everyone[4] == kFeatDodge &&
               everyone[5] == kFeatDash && everyone[6] == kFeatHelp,
           true);
  const std::vector<int> actions_only = FeatsEveryoneHas2024(false, true);
  CHECK_EQ(actions_only.size(), 4u);
  CHECK_EQ(actions_only.size() == 4u && actions_only[0] == kFeatDisengage && actions_only[3] == kFeatHelp, true);
  CHECK_EQ(FeatsEveryoneHas2024(false, false).empty(), true);  // no Rouse in NWN mode
  // Appended: the records' bits keep their places.
  const std::vector<int>& grantable = GrantableFeats2024();
  CHECK_EQ(grantable.size() == 30u && grantable[19] == kFeatDisengage && grantable[20] == kFeatDodge &&
               grantable[21] == kFeatDash && grantable[22] == kFeatHelp,
           true);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatShovePush), false);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatKnockdownShove), true);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatSecondWind2), false);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatBrutalStrike), false);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatOpenHandTechnique), false);  // nwn5e's own: taken back in NWN mode
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatWholenessOfBody), true);     // NWN's table gives it too
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatQuiveringPalm), true);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatDeflectArrows), true);
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
  // R0 (Rogue): Evasion and the bonus picks go; Improved Evasion and the Sneak Attack feats stay.
  CHECK_EQ(has(206), true);   // Evasion
  CHECK_EQ(has(222), true);   // Crippling Strike
  CHECK_EQ(has(225), true);   // Skill Mastery
  CHECK_EQ(has(259), true);   // Slippery Mind
  CHECK_EQ(has(834), true);   // Epic Improved Sneak Attack 1
  CHECK_EQ(has(843), true);   // ... 10
  CHECK_EQ(has(212), false);  // Improved Evasion: the 2024 Evasion (R8)
  CHECK_EQ(has(221), false);  // Sneak Attack
  CHECK_EQ(has(353), false);  // Sneak Attack 10
  CHECK_EQ(std::string(NwnOnlyClassFeatName(252)) == "Uncanny Dodge", true);
  CHECK_EQ(std::string(NwnOnlyClassFeatName(206)) == "Evasion", true);
  CHECK_EQ(std::string(NwnOnlyClassFeatName(840)) == "Epic Improved Sneak Attack", true);
  // M0 / M16 (Monk): the NWN-only Monk features go; their counterparts and the later parts' feats stay.
  for (int feat : {208, 209, 214, 215, 216, 297, 343, 344, 697, 698}) CHECK_EQ(has(feat), true);
  for (int feat : {260, 207, 213, 39, 204, 8, 211, 296, 17, 23, 49}) CHECK_EQ(has(feat), false);
  // The Monk's general feats come off Monks only: never in the list every character is swept with.
  for (int feat : {6, 21, 844, 853}) CHECK_EQ(has(feat), false);
  const std::vector<int>& monk = NwnOnlyMonkFeats();
  auto monk_has = [&monk](int feat) {
    for (int f : monk) {
      if (f == feat) return true;
    }
    return false;
  };
  for (int feat : {6, 21, 844, 849, 853}) CHECK_EQ(monk_has(feat), true);
  CHECK_EQ(monk_has(39), false);  // Stunning Fist: Stunning Strike (part 3)
  CHECK_EQ(monk.size(), 12u);
  CHECK_EQ(std::string(NwnOnlyClassFeatName(215)) == "Diamond Soul", true);
  CHECK_EQ(std::string(NwnOnlyClassFeatName(344)) == "Improved Ki Strike", true);
  CHECK_EQ(std::string(NwnOnlyClassFeatName(21)) == "Improved Unarmed Strike", true);
  CHECK_EQ(std::string(NwnOnlyClassFeatName(850)) == "Epic Improved Stunning Fist", true);
}

// M2, M6, M10, M11, M15 (SRD 5.2.1 Monk). Breaks caught: NWN's level / 5 not
// 0 below 5 or wrong at 20; Unarmored Movement before 2 or a step off by a
// level (15 ft at 6, not 5; 30 ft at 18); its factor not a share of 30 ft;
// Empowered Strikes at NWN's 10; Evasion at NWN's 9 or not granted at Monk 7
// (or granted twice to a Rogue 7 / Monk 7); the Monk without its table.
TEST(MonkPart1) {
  CHECK_EQ(NwnMonkLevelArmorClass(0), 0);
  CHECK_EQ(NwnMonkLevelArmorClass(4), 0);
  CHECK_EQ(NwnMonkLevelArmorClass(5), 1);
  CHECK_EQ(NwnMonkLevelArmorClass(20), 4);
  CHECK_EQ(UnarmoredMovementFeet(1), 0);
  CHECK_EQ(UnarmoredMovementFeet(2), 10);
  CHECK_EQ(UnarmoredMovementFeet(5), 10);
  CHECK_EQ(UnarmoredMovementFeet(6), 15);
  CHECK_EQ(UnarmoredMovementFeet(9), 15);
  CHECK_EQ(UnarmoredMovementFeet(10), 20);
  CHECK_EQ(UnarmoredMovementFeet(14), 25);
  CHECK_EQ(UnarmoredMovementFeet(17), 25);
  CHECK_EQ(UnarmoredMovementFeet(18), 30);
  CHECK_EQ(UnarmoredMovementFeet(20), 30);
  CHECK_EQ(UnarmoredMovementFactor(1) == 0.0f, true);
  CHECK_EQ(UnarmoredMovementFactor(6) == 0.5f, true);
  CHECK_EQ(UnarmoredMovementFactor(18) == 1.0f, true);
  CHECK_EQ(UnarmoredMovementFactor(2) > 0.333f && UnarmoredMovementFactor(2) < 0.334f, true);
  CHECK_EQ(HasEmpoweredStrikes(5), false);
  CHECK_EQ(HasEmpoweredStrikes(6), true);
  CHECK_EQ(kDamagePowerPlusTwenty, 21);
  CHECK_EQ(HasMonkEvasion(6), false);
  CHECK_EQ(HasMonkEvasion(7), true);
  CHECK_EQ(HasBodyAndMind(19), false);
  CHECK_EQ(HasBodyAndMind(20), true);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kMonk, 1}}).empty(), true);
  // Part 3: Patient Defense, Step of the Wind; parts 4 / 5: Open Hand Technique, Deflect Attacks, Wholeness of Body.
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kMonk, 6}}).size(), 7u);  // #55: + the free PD / SotW
  const std::vector<int> monk7 = GrantedFeats2024({{CharacterClass::kMonk, 7}});
  CHECK_EQ(monk7.size() == 8 && monk7[0] == kFeatImprovedEvasion, true);
  for (const std::vector<ClassLevels>& both : {std::vector<ClassLevels>{{CharacterClass::kRogue, 7}, {CharacterClass::kMonk, 7}},
                                               std::vector<ClassLevels>{{CharacterClass::kMonk, 7}, {CharacterClass::kRogue, 7}}}) {
    int evasion = 0;
    for (int feat : GrantedFeats2024(both)) evasion += feat == kFeatImprovedEvasion ? 1 : 0;
    CHECK_EQ(evasion, 1);                        // asked by both classes, granted once
    // With the Rogue's Cunning Strike, the Monk's two Focus feats and parts 4 / 5's three; #53: the Rogue's Cunning
    // Action (three) and Steady Aim; #55: the Monk's free Patient Defense / Step of the Wind.
    CHECK_EQ(GrantedFeats2024(both).size(), 13u);
  }
  CHECK_EQ(HasFeatureList(CharacterClass::kMonk), true);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kMonk, 1).size(), 2u);  // Martial Arts, Unarmored Defense
  CHECK_EQ(ClassFeatures2024(CharacterClass::kMonk, 20).size(), 32u);
  int subclass = 0;
  for (const ClassFeature& f : ClassFeatures2024(CharacterClass::kMonk, 20)) subclass += f.subclass ? 1 : 0;
  CHECK_EQ(subclass, 4);  // Open Hand Technique, Wholeness of Body, Fleet Step, Quivering Palm
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kMonk)) == "Warrior of the Open Hand", true);
}

// M1 Martial Arts (SRD 5.2.1). Breaks caught: the die a step off (d8 at 4 or
// 6, d10 at 10, d12 at 16), a die below Monk 1; a Martial weapon without Light
// (longsword, morningstar, rapier) or a ranged one (sling, dart, shuriken)
// counted a Monk weapon, or a Simple melee one left out (quarterstaff, short
// spear, light mace); a shield or a non-Monk weapon in the off hand allowed, a
// torch refused; a d6 weapon losing to a d6 die or a d4 one kept against a d6;
// Dexterity used when lower; the bonus strike with an off-hand attack, in
// Flurry mode, or without Martial Arts.
TEST(MonkMartialArts) {
  CHECK_EQ(MartialArtsDie(0), 0);
  CHECK_EQ(MartialArtsDie(1), 6);
  CHECK_EQ(MartialArtsDie(4), 6);
  CHECK_EQ(MartialArtsDie(5), 8);
  CHECK_EQ(MartialArtsDie(10), 8);
  CHECK_EQ(MartialArtsDie(11), 10);
  CHECK_EQ(MartialArtsDie(16), 10);
  CHECK_EQ(MartialArtsDie(17), 12);
  CHECK_EQ(MartialArtsDie(20), 12);
  for (int monk : {0, 9, 22, 28, 37, 38, 40, 42, 45, 50, 53, 58, 60}) CHECK_EQ(IsMonkWeapon2024(monk), true);
  for (int other : {1, 2, 4, 5, 13, 47, 51, 61, 59, 31, 15, 14, 36, -1}) CHECK_EQ(IsMonkWeapon2024(other), false);
  CHECK_EQ(MartialArtsHands(-1, -1), true);   // unarmed
  CHECK_EQ(MartialArtsHands(50, -1), true);   // a quarterstaff
  CHECK_EQ(MartialArtsHands(40, 40), true);   // two kamas
  CHECK_EQ(MartialArtsHands(-1, 15), true);   // a torch in the off hand
  CHECK_EQ(MartialArtsHands(1, -1), false);   // a longsword
  CHECK_EQ(MartialArtsHands(-1, 14), false);  // a small shield
  CHECK_EQ(MartialArtsHands(22, 1), false);   // a dagger and a longsword
  CHECK_EQ(MartialArtsHands(15, -1), false);  // a torch as the main weapon
  CHECK_EQ(MartialArtsDieBetter(1, 6, 6), false);  // a d6 weapon against the d6: kept
  CHECK_EQ(MartialArtsDieBetter(1, 4, 6), true);   // a dagger at Monk 1
  CHECK_EQ(MartialArtsDieBetter(1, 8, 8), false);  // a short spear at 5
  CHECK_EQ(MartialArtsDieBetter(1, 6, 8), true);   // a quarterstaff at 5
  CHECK_EQ(MartialArtsDieBetter(2, 4, 8), false);  // 2d4 (5) against d8 (4.5)
  CHECK_EQ(MartialArtsDieBetter(2, 4, 10), true);  // 2d4 (5) against d10 (5.5)
  CHECK_EQ(MartialArtsDieBetter(1, 6, 0), false);
  CHECK_EQ(DexterousAbilityModifier(1, 3), 3);
  CHECK_EQ(DexterousAbilityModifier(4, 2), 4);
  CHECK_EQ(MartialArtsBonusAttacks(true, 0, false), 1);
  CHECK_EQ(MartialArtsBonusAttacks(true, 1, false), 0);
  CHECK_EQ(MartialArtsBonusAttacks(true, 0, true), 0);
  CHECK_EQ(MartialArtsBonusAttacks(false, 0, false), 0);
}

// Monk part 3 (SRD 5.2.1 Monk): Monk's Focus, Flurry of Blows, Stunning Strike,
// Uncanny Metabolism, Heightened Focus, Disciplined Survivor, Perfect Focus,
// Superior Defense. Breaks caught: points at Monk 1 or not the level; the DC
// without PB or Wis; a feat's cost or level wrong (Superior Defense 1 point or
// at 17, Stunning Strike at 1); Flurry's third strike before 10; Uncanny
// Metabolism with nothing spent, short of half, or not when Bloodied; Perfect
// Focus lowering points above 4 or raising a Monk below 4 points total; temp
// HP before 10; all six saves before 14 or only Str / Dex; the radial's Focus
// feats before 2 / 18.
TEST(MonkFocus) {
  CHECK_EQ(FocusPoints(1), 0);
  CHECK_EQ(FocusPoints(2), 2);
  CHECK_EQ(FocusPoints(20), 20);
  CHECK_EQ(FocusDc(3, 2), 13);
  CHECK_EQ(FocusDc(5, 6), 19);
  CHECK_EQ(FocusCost(kFeatStunningFist), 1);
  CHECK_EQ(FocusCost(kFeatPatientDefense), 1);
  CHECK_EQ(FocusCost(kFeatStepOfTheWind), 1);
  CHECK_EQ(FocusCost(kFeatSuperiorDefense), 3);
  CHECK_EQ(FocusCost(kFeatQuiveringPalm), 4);
  CHECK_EQ(FocusCost(kFeatFlurryOfBlows), 0);  // spent per round by rules_monk, not as uses
  CHECK_EQ(FocusCost(kFeatImprovedEvasion), 0);
  CHECK_EQ(FocusFeatLevel(kFeatStunningFist), 5);
  CHECK_EQ(FocusFeatLevel(kFeatSuperiorDefense), 18);
  CHECK_EQ(FocusFeatLevel(kFeatPatientDefense), 2);
  CHECK_EQ(HasFlurryOfBlows(1), false);
  CHECK_EQ(HasFlurryOfBlows(2), true);
  CHECK_EQ(HasStunningStrike(4), false);
  CHECK_EQ(HasStunningStrike(5), true);
  CHECK_EQ(FlurryStrikes(1), 0);
  CHECK_EQ(FlurryStrikes(2), 2);
  CHECK_EQ(FlurryStrikes(9), 2);
  CHECK_EQ(FlurryStrikes(10), 3);
  CHECK_EQ(UncannyMetabolismWanted(10, 0, true), false);  // nothing to give back
  CHECK_EQ(UncannyMetabolismWanted(10, 4, false), false);
  CHECK_EQ(UncannyMetabolismWanted(10, 5, false), true);   // half spent
  CHECK_EQ(UncannyMetabolismWanted(9, 5, false), true);
  CHECK_EQ(UncannyMetabolismWanted(9, 4, false), false);
  CHECK_EQ(UncannyMetabolismWanted(10, 1, true), true);    // Bloodied
  CHECK_EQ(UncannyMetabolismWanted(0, 0, true), false);
  CHECK_EQ(HasPerfectFocus(14), false);
  CHECK_EQ(HasPerfectFocus(15), true);
  CHECK_EQ(PerfectFocusSpent(15, 13), 11);  // 2 left -> 4
  CHECK_EQ(PerfectFocusSpent(15, 12), 11);  // 3 left -> 4
  CHECK_EQ(PerfectFocusSpent(15, 11), 11);  // 4 left: kept
  CHECK_EQ(PerfectFocusSpent(15, 2), 2);
  CHECK_EQ(PerfectFocusSpent(3, 3), 3);     // fewer than 4 points in all: nothing to rise to
  CHECK_EQ(PatientDefenseTempDice(9), 0);
  CHECK_EQ(PatientDefenseTempDice(10), 2);
  CHECK_EQ(HasDisciplinedSurvivor(13), false);
  CHECK_EQ(HasDisciplinedSurvivor(14), true);
  for (Ability a : {Ability::kStrength, Ability::kDexterity, Ability::kConstitution, Ability::kIntelligence,
                    Ability::kWisdom, Ability::kCharisma}) {
    CHECK_EQ(ClassFeatureSaveProficiency({{CharacterClass::kMonk, 14}}, a), true);
  }
  CHECK_EQ(ClassFeatureSaveProficiency({{CharacterClass::kMonk, 13}}, Ability::kCharisma), false);
  CHECK_EQ(HasSuperiorDefense(17), false);
  CHECK_EQ(HasSuperiorDefense(18), true);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kMonk, 2}}).size(), 4u);  // #55: + the free PD / SotW
  const std::vector<int> monk18 = GrantedFeats2024({{CharacterClass::kMonk, 18}});
  // Evasion, Patient Defense, Step of the Wind, Superior Defense; Open Hand Technique, Deflect Attacks,
  // Wholeness of Body, Quivering Palm (parts 4 / 5).
  CHECK_EQ(monk18.size(), 10u);
  CHECK_EQ(monk18[3], kFeatSuperiorDefense);
  CHECK_EQ(GrantedFeats2024({{CharacterClass::kMonk, 17}}).size(), 9u);
}

static bool Grants(const std::vector<int>& feats, int feat) {
  for (int f : feats) {
    if (f == feat) return true;
  }
  return false;
}

// #31 (0.60.11): monsters keep NWN's Stunning Fist / Flurry penalties; a fight ends once the Monk is out of
// combat (not within its first 10 s). Breaks caught: a character's penalty kept, a monster's dropped, an unknown
// creature penalised, a fight over while in combat, over in its first moments, never over.
TEST(MonkFollowUps) {
  CHECK_EQ(MonkAttackPenalty(true, true, -4), 0);    // a character's Stunning Strike
  CHECK_EQ(MonkAttackPenalty(true, false, -4), -4);  // a monster's Stunning Fist
  CHECK_EQ(MonkAttackPenalty(true, false, -2), -2);  // a monster's Flurry
  CHECK_EQ(MonkAttackPenalty(false, false, -2), 0);  // not known: the sheet's figures
  CHECK_EQ(MonkFightOver(true, 60000), false);
  CHECK_EQ(MonkFightOver(false, 9999), false);
  CHECK_EQ(MonkFightOver(false, 10000), true);
  CHECK_EQ(MonkFightOver(false, 600000), true);
}

// Monk part 4 (SRD 5.2.1 Warrior of the Open Hand; PLAN.md OH1, OH2, OH4).
// Breaks caught: a feature at the wrong level (Open Hand Technique 3, Wholeness
// 6, Quivering Palm 17), NWN's levels left for its feats (Wholeness 7, Quivering
// Palm 15, Deflect Arrows 2), Wholeness' uses not Wis or no minimum 1, its
// healing without Wis or no minimum 1, Quivering Palm's half rounded up or
// whole on a success, "never chosen" not Topple, a technique not coming back as
// it went in, a Flurry strike counted among the main hand's attacks or past
// the Flurry's strikes.
TEST(MonkOpenHand) {
  CHECK_EQ(HasOpenHandTechnique(2), false);
  CHECK_EQ(HasOpenHandTechnique(3), true);
  CHECK_EQ(HasWholenessOfBody(5), false);
  CHECK_EQ(HasWholenessOfBody(6), true);
  CHECK_EQ(HasQuiveringPalm(16), false);
  CHECK_EQ(HasQuiveringPalm(17), true);
  CHECK_EQ(MonkFeatLevel2024(kFeatWholenessOfBody), 6);
  CHECK_EQ(MonkFeatLevel2024(kFeatQuiveringPalm), 17);
  CHECK_EQ(MonkFeatLevel2024(kFeatDeflectArrows), 3);
  CHECK_EQ(MonkFeatLevel2024(kFeatStunningFist), 5);
  CHECK_EQ(MonkFeatLevel2024(kFeatFlurryOfBlows), 2);
  CHECK_EQ(MonkFeatLevel2024(kFeatPatientDefense), 0);
  CHECK_EQ(WholenessOfBodyUses(3), 3);
  CHECK_EQ(WholenessOfBodyUses(1), 1);
  CHECK_EQ(WholenessOfBodyUses(0), 1);
  CHECK_EQ(WholenessOfBodyUses(-1), 1);
  CHECK_EQ(WholenessOfBodyHealing(5, 3), 8);
  CHECK_EQ(WholenessOfBodyHealing(1, -1), 1);
  CHECK_EQ(WholenessOfBodyHealing(1, -3), 1);
  CHECK_EQ(QuiveringPalmDamage(65, false), 65);
  CHECK_EQ(QuiveringPalmDamage(65, true), 32);
  CHECK_EQ(DecodeOpenHandTechnique(0) == OpenHandTechnique::kTopple, true);
  for (OpenHandTechnique t : {OpenHandTechnique::kTopple, OpenHandTechnique::kPush, OpenHandTechnique::kAddle,
                              OpenHandTechnique::kOff}) {
    CHECK_EQ(DecodeOpenHandTechnique(EncodeOpenHandTechnique(t)) == t, true);
  }
  CHECK_EQ(std::string(OpenHandTechniqueName(OpenHandTechnique::kPush)) == "Push", true);
  CHECK_EQ(IsFlurryStrike(0, 1, 2), false);  // the main hand's attack
  CHECK_EQ(IsFlurryStrike(1, 1, 2), true);
  CHECK_EQ(IsFlurryStrike(2, 1, 2), true);
  CHECK_EQ(IsFlurryStrike(3, 1, 2), false);  // past the Flurry's two (Haste's)
  CHECK_EQ(IsFlurryStrike(1, 2, 3), false);  // Extra Attack: two main-hand attacks first
  CHECK_EQ(IsFlurryStrike(4, 2, 3), true);
  CHECK_EQ(IsFlurryStrike(2, 2, 0), false);  // no Flurry this round
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 3}}), kFeatOpenHandTechnique), true);
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 2}}), kFeatOpenHandTechnique), false);
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 6}}), kFeatWholenessOfBody), true);
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 5}}), kFeatWholenessOfBody), false);
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 17}}), kFeatQuiveringPalm), true);
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 16}}), kFeatQuiveringPalm), false);
  CHECK_EQ(FocusTooltip(17, 17, 18, true).find("Quivering Palm: 4") != std::string::npos, true);
  CHECK_EQ(FocusTooltip(16, 16, 18, true).find("Quivering Palm") == std::string::npos, true);
}

// Monk part 5 (SRD 5.2.1; PLAN.md M8, M12). Breaks caught: Deflect Attacks at
// the wrong level (3; Deflect Energy 13; Self-Restoration 10), a fire-only hit
// taken before 13, the weapon's base damage not counted as physical, the
// reduction without Dex or the Monk level, a negative reduction, magic damage
// reduced before the physical, damage below 0 or the total wrong, Self-
// Restoration's order (Charmed, Frightened, Poisoned).
// #54 (owner, 2026-11-02, 1 A): Deflect Attacks' redirect. Breaks caught: a redirect with damage left, with the switch
// off, with no Focus Point; a ranged attacker out of 60 ft or unseen reached, a melee one not; the damage without the
// Dexterity modifier or below 0; the type not the largest share; the weapon's own damage not its weapon's type.
// #53 (owner, 2026-11-03): the extra actions. Breaks caught: Action Surge at 1 or not at 2, a second use before 17
// (or none at 17), Cunning Action at 1, Steady Aim at 2, Thief's Reflexes before 17, Steady Aim refused for a jitter
// or allowed after a move, the feats not granted at their levels.
TEST(ExtraActions) {
  CHECK_EQ(HasActionSurge(1), false);
  CHECK_EQ(HasActionSurge(2), true);
  CHECK_EQ(ActionSurgeUses(1), 0);
  CHECK_EQ(ActionSurgeUses(2), 1);
  CHECK_EQ(ActionSurgeUses(16), 1);
  CHECK_EQ(ActionSurgeUses(17), 2);
  CHECK_EQ(HasCunningAction(1), false);
  CHECK_EQ(HasCunningAction(2), true);
  CHECK_EQ(HasSteadyAim(2), false);
  CHECK_EQ(HasSteadyAim(3), true);
  CHECK_EQ(HasThiefsReflexes(16), false);
  CHECK_EQ(HasThiefsReflexes(17), true);
  CHECK_EQ(SteadyAimAllowed(0.0f), true);
  CHECK_EQ(SteadyAimAllowed(0.4f), true);
  CHECK_EQ(SteadyAimAllowed(0.6f), false);
  // 0.66.4: the stealth button - into stealth mode not with an attack target (NWN's), out of it always.
  CHECK_EQ(StealthToggleAllowed(true, false), true);
  CHECK_EQ(StealthToggleAllowed(true, true), false);
  CHECK_EQ(StealthToggleAllowed(false, true), true);
  CHECK_EQ(StealthToggleAllowed(false, false), true);
  // #129: an attack from hiding - the 2024 Hide ends after the roll, NWN's before it.
  CHECK_EQ(WhenAttackEndsHide(false, true) == HideAtAttack::kNotHiding, true);
  CHECK_EQ(WhenAttackEndsHide(true, true) == HideAtAttack::kEndsAfterRoll, true);
  CHECK_EQ(WhenAttackEndsHide(true, false) == HideAtAttack::kEndsBeforeRoll, true);
  // 2026-11-09 (A): a spell without a Verbal component keeps the hide, faithful only.
  CHECK_EQ(SpellHasVerbal("vs"), true);
  CHECK_EQ(SpellHasVerbal("V"), true);
  CHECK_EQ(SpellHasVerbal("s"), false);
  CHECK_EQ(SpellHasVerbal("****"), false);
  CHECK_EQ(CastKeepsHiding(true, false, true), true);
  CHECK_EQ(CastKeepsHiding(true, true, true), false);
  CHECK_EQ(CastKeepsHiding(true, false, false), false);
  CHECK_EQ(CastKeepsHiding(false, false, true), false);
  // 0.66.8: Advantage from hiding - only while hiding, only unseen by the target, faithful only.
  CHECK_EQ(HiddenAttackAdvantage(true, false, true), true);
  CHECK_EQ(HiddenAttackAdvantage(true, true, true), false);
  CHECK_EQ(HiddenAttackAdvantage(false, false, true), false);
  CHECK_EQ(HiddenAttackAdvantage(true, false, false), false);
  // #133 (owner, 2026-11-04: "go with B"): an AI attack on a hider it doesn't see becomes a search. Breaks caught: one
  // that sees the hider searching, a target not hiding searched for, a player's order turned, NWN mode.
  CHECK_EQ(UnseenHiderSearched(true, false, true, false), true);
  CHECK_EQ(UnseenHiderSearched(true, false, true, true), false);   // it sees the hider: it attacks
  CHECK_EQ(UnseenHiderSearched(true, false, false, false), false); // not hiding (an invisible creature): NWN's
  CHECK_EQ(UnseenHiderSearched(true, true, true, false), false);   // a player's order stands
  CHECK_EQ(UnseenHiderSearched(false, false, true, false), false); // NWN mode
  // #136 B: a script's move onto a hider the mover doesn't see (NWN's OnDamaged) becomes the same search; Surprise.
  CHECK_EQ(MoveOntoUnseenHider(true, false, true, false, 0.0f), true);   // the damager's exact location
  CHECK_EQ(MoveOntoUnseenHider(true, false, true, false, 1.0f), true);   // within the reach
  CHECK_EQ(MoveOntoUnseenHider(true, false, true, false, 1.5f), false);  // elsewhere: its own move
  CHECK_EQ(MoveOntoUnseenHider(true, false, true, true, 0.0f), false);   // it sees the hider
  CHECK_EQ(MoveOntoUnseenHider(true, false, false, false, 0.0f), false); // not hiding
  CHECK_EQ(MoveOntoUnseenHider(true, true, true, false, 0.0f), false);   // a player's order stands
  CHECK_EQ(MoveOntoUnseenHider(false, false, true, false, 0.0f), false); // NWN mode
  CHECK_EQ(SurprisedAtInitiative(true, false, true), true);    // hit by a hider it never saw
  CHECK_EQ(SurprisedAtInitiative(true, true, true), false);    // it sees an enemy: not surprised
  CHECK_EQ(SurprisedAtInitiative(true, false, false), false);  // no hidden enemy
  CHECK_EQ(SurprisedAtInitiative(false, false, true), false);  // NWN mode
  auto has = [](const std::vector<int>& feats, int feat) {
    return std::find(feats.begin(), feats.end(), feat) != feats.end();
  };
  CHECK_EQ(has(GrantedFeats2024({{CharacterClass::kFighter, 1}}), kFeatActionSurge), false);
  CHECK_EQ(has(GrantedFeats2024({{CharacterClass::kFighter, 2}}), kFeatActionSurge), true);
  const std::vector<int> rogue2 = GrantedFeats2024({{CharacterClass::kRogue, 2}});
  CHECK_EQ(has(rogue2, kFeatCunningDash) && has(rogue2, kFeatCunningDisengage) && has(rogue2, kFeatCunningHide), true);
  CHECK_EQ(has(rogue2, kFeatSteadyAim), false);
  CHECK_EQ(has(GrantedFeats2024({{CharacterClass::kRogue, 3}}), kFeatSteadyAim), true);
  CHECK_EQ(has(GrantedFeats2024({{CharacterClass::kRogue, 1}}), kFeatCunningDash), false);
  const std::vector<int>& grantable = GrantableFeats2024();
  CHECK_EQ(grantable.size() == 30u && grantable[23] == kFeatActionSurge && grantable[27] == kFeatSteadyAim, true);
  // #55: appended - the Monk's free Patient Defense / Step of the Wind.
  CHECK_EQ(grantable.size() == 30u && grantable[28] == kFeatPatientDefenseFree &&
               grantable[29] == kFeatStepOfTheWindFree,
           true);
  const std::vector<int> monk2 = GrantedFeats2024({{CharacterClass::kMonk, 2}});
  CHECK_EQ(has(monk2, kFeatPatientDefenseFree) && has(monk2, kFeatStepOfTheWindFree), true);
  CHECK_EQ(has(GrantedFeats2024({{CharacterClass::kMonk, 1}}), kFeatPatientDefenseFree), false);
  // #55: Fleet Step (Open Hand 11) - Step of the Wind right after another Bonus Action, once a round.
  CHECK_EQ(HasFleetStep(10), false);
  CHECK_EQ(HasFleetStep(11), true);
  CHECK_EQ(FleetStepAllows(11, kFeatStepOfTheWind, kFeatPatientDefense, true), true);
  CHECK_EQ(FleetStepAllows(11, kFeatStepOfTheWindFree, -1, true), true);  // after the bonus attack / Flurry
  CHECK_EQ(FleetStepAllows(11, kFeatStepOfTheWind, kFeatStepOfTheWindFree, true), false);  // not after itself
  CHECK_EQ(FleetStepAllows(11, kFeatPatientDefense, kFeatPatientDefenseFree, true), false);  // only Step of the Wind
  CHECK_EQ(FleetStepAllows(11, kFeatStepOfTheWind, kFeatPatientDefense, false), false);  // once a round
  CHECK_EQ(FleetStepAllows(10, kFeatStepOfTheWind, kFeatPatientDefense, true), false);
}

TEST(DeflectRedirect) {
  CHECK_EQ(DeflectRedirects(0, true, 1), true);
  CHECK_EQ(DeflectRedirects(1, true, 5), false);
  CHECK_EQ(DeflectRedirects(0, false, 5), false);
  CHECK_EQ(DeflectRedirects(0, true, 0), false);
  CHECK_EQ(DeflectRedirectReaches(false, 30.0f, false), true);  // melee: it hit you, so it's in reach
  CHECK_EQ(DeflectRedirectReaches(true, 18.0f, true), true);
  CHECK_EQ(DeflectRedirectReaches(true, 19.0f, true), false);
  CHECK_EQ(DeflectRedirectReaches(true, 5.0f, false), false);
  CHECK_EQ(DeflectRedirectDamage(7, 3), 10);
  CHECK_EQ(DeflectRedirectDamage(2, -4), 0);
  int16_t entries[32];
  for (int16_t& e : entries) e = -1;
  entries[5] = 4;   // cold
  entries[8] = 9;   // fire
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 0), 8);
  entries[12] = 11;  // the weapon's own damage
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 1), 1);  // a piercing weapon
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 2), 0);  // bludgeoning
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 3), 2);  // slashing
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 4), 2);  // slashing and piercing
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 5), 1);  // piercing and bludgeoning
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 0), 0);  // a creature's blow
  for (int16_t& e : entries) e = -1;
  CHECK_EQ(DeflectRedirectDamageIndex(entries, 32, 3), 0);
}

TEST(MonkDeflectAndRestore) {
  CHECK_EQ(HasDeflectAttacks(2), false);
  CHECK_EQ(HasDeflectAttacks(3), true);
  CHECK_EQ(HasDeflectEnergy(12), false);
  CHECK_EQ(HasDeflectEnergy(13), true);
  CHECK_EQ(HasSelfRestoration(9), false);
  CHECK_EQ(HasSelfRestoration(10), true);
  int16_t fire[13] = {-1, -1, -1, -1, -1, -1, -1, -1, 9, -1, -1, -1, -1};
  CHECK_EQ(DeflectAttacksQualifies(fire, 13, false), false);
  CHECK_EQ(DeflectAttacksQualifies(fire, 13, true), true);
  int16_t base[13] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 7};
  CHECK_EQ(DeflectAttacksQualifies(base, 13, false), true);  // the weapon's own damage
  int16_t slash[13] = {-1, -1, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  CHECK_EQ(DeflectAttacksQualifies(slash, 13, false), true);
  int16_t none[13] = {-1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  CHECK_EQ(DeflectAttacksQualifies(none, 13, true), false);
  CHECK_EQ(DeflectAttacksReduction(6, 3, 5), 14);
  CHECK_EQ(DeflectAttacksReduction(1, -5, 0), 0);
  int16_t mixed[13] = {-1, -1, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, 4};  // slashing 10, magical 6, base 4
  CHECK_EQ(ReduceAttackDamage(mixed, 13, 12), 8);
  CHECK_EQ(mixed[2], 0);
  CHECK_EQ(mixed[12], 2);  // physical first
  CHECK_EQ(mixed[3], 6);
  CHECK_EQ(ReduceAttackDamage(mixed, 13, 20), 0);
  CHECK_EQ(mixed[3], 0);
  CHECK_EQ(mixed[0], -1);  // unused entries stay unused
  CHECK_EQ(SelfRestorationPick(true, true, true) == SelfRestoration::kCharmed, true);
  CHECK_EQ(SelfRestorationPick(false, true, true) == SelfRestoration::kFrightened, true);
  CHECK_EQ(SelfRestorationPick(false, false, true) == SelfRestoration::kPoisoned, true);
  CHECK_EQ(SelfRestorationPick(false, false, false) == SelfRestoration::kNone, true);
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 3}}), kFeatDeflectArrows), true);
  CHECK_EQ(Grants(GrantedFeats2024({{CharacterClass::kMonk, 2}}), kFeatDeflectArrows), false);
}

// R1 (SRD 5.2.1 p. 62): 1d6 at 1, one more at every odd level, 10d6 at 19; a
// Finesse or ranged weapon, and Advantage or (without Disadvantage) an ally next
// to the target. Breaks caught: NWN's count off by one (2d6 at 2), past 10d6,
// a club counted, an ally's help kept under Disadvantage, Advantage alone
// refused without an ally.
TEST(RogueSneakAttack2024) {
  CHECK_EQ(SneakAttackDice2024(0), 0);
  CHECK_EQ(SneakAttackDice2024(1), 1);
  CHECK_EQ(SneakAttackDice2024(2), 1);
  CHECK_EQ(SneakAttackDice2024(3), 2);
  CHECK_EQ(SneakAttackDice2024(19), 10);
  CHECK_EQ(SneakAttackDice2024(20), 10);
  CHECK_EQ(SneakAttackApplies(false, RollMode::kAdvantage, true), false);
  CHECK_EQ(SneakAttackApplies(true, RollMode::kAdvantage, false), true);
  CHECK_EQ(SneakAttackApplies(true, RollMode::kNormal, true), true);
  CHECK_EQ(SneakAttackApplies(true, RollMode::kNormal, false), false);
  CHECK_EQ(SneakAttackApplies(true, RollMode::kDisadvantage, true), false);
  CHECK_EQ(HasFeatureList(CharacterClass::kRogue), true);
  // Part 2 (R8, R10, R11): Evasion from 7 (granted as Improved Evasion), Slippery
  // Mind's Wisdom and Charisma saves from 15, Elusive from 18.
  CHECK_EQ(HasRogueEvasion(6), false);
  CHECK_EQ(HasRogueEvasion(7), true);
  CHECK_EQ(HasElusive(17), false);
  CHECK_EQ(HasElusive(18), true);
  const std::vector<ClassLevels> rogue14 = {{CharacterClass::kRogue, 14}};
  const std::vector<ClassLevels> fighter2_rogue15 = {{CharacterClass::kFighter, 2}, {CharacterClass::kRogue, 15}};
  CHECK_EQ(ClassFeatureSaveProficiency(rogue14, Ability::kWisdom), false);
  CHECK_EQ(ClassFeatureSaveProficiency(fighter2_rogue15, Ability::kWisdom), true);
  CHECK_EQ(ClassFeatureSaveProficiency(fighter2_rogue15, Ability::kCharisma), true);
  CHECK_EQ(ClassFeatureSaveProficiency(fighter2_rogue15, Ability::kStrength), false);
  const std::vector<int> granted4 = GrantedFeats2024({{CharacterClass::kRogue, 4}});
  const std::vector<int> granted6 = GrantedFeats2024({{CharacterClass::kRogue, 6}});
  const std::vector<int> granted7 = GrantedFeats2024({{CharacterClass::kRogue, 7}});
  const std::vector<int> granted14 = GrantedFeats2024({{CharacterClass::kRogue, 14}});
  // #53: Cunning Action's three (2) and Steady Aim (3) first.
  CHECK_EQ(static_cast<int>(granted4.size()), 4);
  CHECK_EQ(static_cast<int>(granted6.size()), 5);  // Cunning Strike's radial (R6)
  CHECK_EQ(granted6.size() == 5 ? granted6[4] : 0, kFeatCunningStrike);
  CHECK_EQ(static_cast<int>(granted7.size()), 6);
  CHECK_EQ(granted7.size() == 6 ? granted7[4] : 0, kFeatImprovedEvasion);
  CHECK_EQ(static_cast<int>(granted14.size()), 7);
  CHECK_EQ(granted14.size() == 7 ? granted14[6] : 0, kFeatDeviousStrikes);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatImprovedEvasion), true);
  // Parts 3-4 (R7, R12): Uncanny Dodge from 5 halves each damage entry (rounded
  // down; -1 unused, untouched); Stroke of Luck at 20.
  CHECK_EQ(HasUncannyDodge(4), false);
  CHECK_EQ(HasUncannyDodge(5), true);
  CHECK_EQ(HasStrokeOfLuck(19), false);
  CHECK_EQ(HasStrokeOfLuck(20), true);
  int16_t damage[5] = {9, -1, 0, 4, 1};
  CHECK_EQ(HalveAttackDamage(damage, 5), 6);
  CHECK_EQ(damage[0], 4);
  CHECK_EQ(damage[1], -1);
  CHECK_EQ(damage[3], 2);
  CHECK_EQ(damage[4], 0);
  CHECK_EQ(static_cast<int>(ClassFeatures2024(CharacterClass::kRogue, 1).size()), 4);
  CHECK_EQ(static_cast<int>(ClassFeatures2024(CharacterClass::kRogue, 20).size()), 28);
}

// Part 5 (R6): Cunning Strike from 5 (Poison, Trip, Withdraw: 1d6 each), two
// effects from 11, Devious Strikes from 14 (Daze 2d6, Obscure 3d6, Knock Out
// 6d6); DC 8 + PB + Dex; never chosen = off. Breaks caught: an effect at the
// wrong level or cost, a third effect kept at 11 or a second below it, the
// older of two not the one dropped, a pick not removing a chosen effect, the
// stored number not round-tripping, an effect applied when its dice don't fit
// (or charged when skipped: Trip on a Huge target), dice left miscounted.
TEST(RogueCunningStrike) {
  CHECK_EQ(HasCunningStrike(4), false);
  CHECK_EQ(HasCunningStrike(5), true);
  CHECK_EQ(HasDeviousStrikes(13), false);
  CHECK_EQ(HasDeviousStrikes(14), true);
  CHECK_EQ(CunningStrikeEffectCount(4), 0);
  CHECK_EQ(CunningStrikeEffectCount(10), 1);
  CHECK_EQ(CunningStrikeEffectCount(11), 2);
  CHECK_EQ(CunningEffectCost(CunningEffect::kPoison), 1);
  CHECK_EQ(CunningEffectCost(CunningEffect::kTrip), 1);
  CHECK_EQ(CunningEffectCost(CunningEffect::kWithdraw), 1);
  CHECK_EQ(CunningEffectCost(CunningEffect::kDaze), 2);
  CHECK_EQ(CunningEffectCost(CunningEffect::kObscure), 3);
  CHECK_EQ(CunningEffectCost(CunningEffect::kKnockOut), 6);
  CHECK_EQ(CunningEffectAvailable(CunningEffect::kPoison, 4), false);
  CHECK_EQ(CunningEffectAvailable(CunningEffect::kPoison, 5), true);
  CHECK_EQ(CunningEffectAvailable(CunningEffect::kDaze, 13), false);
  CHECK_EQ(CunningEffectAvailable(CunningEffect::kKnockOut, 14), true);
  CHECK_EQ(CunningEffectAvailable(CunningEffect::kNone, 20), false);
  CHECK_EQ(CunningStrikeDc(4, 3), 15);
  CHECK_EQ(ActiveCunningEffects(DecodeCunningChoice(0), 20).empty(), true);  // never chosen: off

  CunningChoice c;
  CHECK_EQ(PickCunningEffect(&c, CunningEffect::kTrip, 5) == CunningPick::kChosen, true);
  CHECK_EQ(PickCunningEffect(&c, CunningEffect::kPoison, 5) == CunningPick::kChosen, true);
  std::vector<CunningEffect> active = ActiveCunningEffects(c, 5);
  CHECK_EQ(static_cast<int>(active.size()), 1);  // one at a time below 11
  CHECK_EQ(active.size() == 1 && active[0] == CunningEffect::kPoison, true);
  CHECK_EQ(PickCunningEffect(&c, CunningEffect::kDaze, 5) == CunningPick::kLocked, true);
  CHECK_EQ(PickCunningEffect(&c, CunningEffect::kPoison, 5) == CunningPick::kRemoved, true);
  CHECK_EQ(ActiveCunningEffects(c, 5).empty(), true);

  c = CunningChoice();
  PickCunningEffect(&c, CunningEffect::kTrip, 11);
  PickCunningEffect(&c, CunningEffect::kPoison, 11);
  CHECK_EQ(static_cast<int>(ActiveCunningEffects(c, 11).size()), 2);
  PickCunningEffect(&c, CunningEffect::kWithdraw, 11);  // a third: the older (Trip) goes
  CHECK_EQ(c.first == CunningEffect::kPoison && c.second == CunningEffect::kWithdraw, true);
  const CunningChoice back = DecodeCunningChoice(EncodeCunningChoice(c));
  CHECK_EQ(back.first == CunningEffect::kPoison && back.second == CunningEffect::kWithdraw, true);
  CHECK_EQ(static_cast<int>(ActiveCunningEffects(c, 10).size()), 1);  // a drained level: the first only
  CHECK_EQ(PickCunningEffect(&c, CunningEffect::kNone, 11) == CunningPick::kOff, true);
  CHECK_EQ(ActiveCunningEffects(DecodeCunningChoice(EncodeCunningChoice(c)), 11).empty(), true);

  int left = -1;
  c = CunningChoice{CunningEffect::kTrip, CunningEffect::kPoison};
  CHECK_EQ(static_cast<int>(CunningEffectsForSneakAttack(c, 11, 6, true, &left).size()), 2);
  CHECK_EQ(left, 4);
  std::vector<CunningEffect> huge = CunningEffectsForSneakAttack(c, 11, 6, false, &left);
  CHECK_EQ(huge.size() == 1 && huge[0] == CunningEffect::kPoison, true);  // no Trip on a Huge target
  CHECK_EQ(left, 5);
  c = CunningChoice{CunningEffect::kKnockOut, CunningEffect::kTrip};
  CHECK_EQ(static_cast<int>(CunningEffectsForSneakAttack(c, 14, 7, true, &left).size()), 2);
  CHECK_EQ(left, 0);
  std::vector<CunningEffect> short_dice = CunningEffectsForSneakAttack(c, 14, 5, true, &left);
  CHECK_EQ(short_dice.size() == 1 && short_dice[0] == CunningEffect::kTrip, true);  // Knock Out doesn't fit
  CHECK_EQ(left, 4);
}

// Part 6 (T1, T4): Fast Hands at Thief 3 takes the better value; Use Magic
// Device at 13 reads any scroll, levels 2+ with an Arcana check against 10 +
// the level. Breaks caught: either at the wrong level, the lower value taken,
// a level 1 scroll checked, the DC off by the level.
TEST(RogueThief) {
  CHECK_EQ(HasFastHands(2), false);
  CHECK_EQ(HasFastHands(3), true);
  CHECK_EQ(FastHandsValue(5, 8), 8);
  CHECK_EQ(FastHandsValue(9, 8), 9);
  CHECK_EQ(HasUseMagicDevice(12), false);
  CHECK_EQ(HasUseMagicDevice(13), true);
  CHECK_EQ(ScrollNeedsArcanaCheck(0), false);
  CHECK_EQ(ScrollNeedsArcanaCheck(1), false);
  CHECK_EQ(ScrollNeedsArcanaCheck(2), true);
  CHECK_EQ(ScrollArcanaDc(3), 13);
  CHECK_EQ(ScrollArcanaDc(9), 19);
}

// Breaks caught: Rage's uses or damage at the wrong level (2 / 3 / 4 / 5 / 6
// at 1 / 3 / 6 / 12 / 17; +2 / +3 / +4 at 1 / 9 / 16), a feat of the Rage
// chain missed or another feat taken for one, a part 2 feature at the wrong
// level, Relentless Rage's DC climbing by other than 5 or its hit points other
// than twice the level, Indomitable Might lowering a total.
// 0.50.1 (B3's chase): an enemy ahead keeps a moving Rage - within 45 degrees
// of the facing and 40 m. Breaks caught: behind or beside counted, the cone
// wider or narrower than 45 degrees, no range limit, a zero facing counted.
TEST(RageChaseHeading) {
  CHECK_EQ(HeadingToward(1.0f, 0.0f, 10.0f, 0.0f), true);    // straight ahead
  CHECK_EQ(HeadingToward(1.0f, 0.0f, 10.0f, 9.0f), true);    // 42 degrees
  CHECK_EQ(HeadingToward(1.0f, 0.0f, 10.0f, 11.0f), false);  // 48 degrees
  CHECK_EQ(HeadingToward(1.0f, 0.0f, 0.0f, 10.0f), false);   // beside
  CHECK_EQ(HeadingToward(1.0f, 0.0f, -10.0f, 0.0f), false);  // behind
  CHECK_EQ(HeadingToward(0.0f, 1.0f, 0.5f, 39.0f), true);
  CHECK_EQ(HeadingToward(0.0f, 1.0f, 0.0f, 41.0f), false);   // past 40 m
  CHECK_EQ(HeadingToward(0.0f, 0.0f, 5.0f, 0.0f), false);
}

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
