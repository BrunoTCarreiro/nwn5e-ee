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
  CHECK_EQ(GrantableFeats2024().size(), 12u);
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
  CHECK_EQ(FeatsEveryoneHas2024(true).size(), 3u);
  CHECK_EQ(FeatsEveryoneHas2024(true)[0], kFeatKnockdownShove);
  CHECK_EQ(FeatsEveryoneHas2024(true)[1], kFeatShovePush);
  CHECK_EQ(FeatsEveryoneHas2024(true)[2], kFeatGrapple);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatShovePush), false);
  CHECK_EQ(FeatsEveryoneHas2024(false).empty(), true);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatKnockdownShove), true);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatSecondWind2), false);
  CHECK_EQ(GrantedOnlyWhenMissing(kFeatBrutalStrike), false);
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
  CHECK_EQ(static_cast<int>(granted4.size()), 0);
  CHECK_EQ(static_cast<int>(granted6.size()), 1);  // Cunning Strike's radial (R6)
  CHECK_EQ(granted6.empty() ? 0 : granted6[0], kFeatCunningStrike);
  CHECK_EQ(static_cast<int>(granted7.size()), 2);
  CHECK_EQ(granted7.empty() ? 0 : granted7[0], kFeatImprovedEvasion);
  CHECK_EQ(static_cast<int>(granted14.size()), 3);
  CHECK_EQ(granted14.size() == 3 ? granted14[2] : 0, kFeatDeviousStrikes);
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
