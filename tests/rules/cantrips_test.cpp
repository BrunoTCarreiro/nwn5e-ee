#include "check.h"
#include "rules/cantrips.h"
#include "rules/skills.h"

#include <string.h>

#include <algorithm>
#include <vector>

using namespace rules;

// #39 Part 1: Cantrip Upgrade - a die more at character levels 5, 11 and 17; doubled on a Critical Hit.
TEST(CantripUpgrade) {
  CHECK_EQ(CantripDice(1), 1);
  CHECK_EQ(CantripDice(4), 1);
  CHECK_EQ(CantripDice(5), 2);
  CHECK_EQ(CantripDice(10), 2);
  CHECK_EQ(CantripDice(11), 3);
  CHECK_EQ(CantripDice(16), 3);
  CHECK_EQ(CantripDice(17), 4);
  CHECK_EQ(CantripDice(20), 4);
  CHECK_EQ(CantripDice(30), 4);  // an epic level: still four
  CHECK_EQ(CantripDiceRolled(5, false), 2);
  CHECK_EQ(CantripDiceRolled(5, true), 4);
  CHECK_EQ(CantripDiceRolled(1, true), 2);
}

// SRD 5.2.1's dice, damage types and rolls.
TEST(CantripsAreSrd52) {
  const Cantrip* fire_bolt = CantripOf(1050);
  CHECK_EQ(fire_bolt != nullptr, true);
  CHECK_EQ(fire_bolt->die, 10);
  CHECK_EQ(fire_bolt->damage_type, kDamageTypeFire);
  CHECK_EQ(fire_bolt->roll == CantripRoll::kRangedSpellAttack, true);
  CHECK_EQ(CantripOf(144)->die, 8);                                       // Ray of Frost
  CHECK_EQ(CantripOf(144)->damage_type, kDamageTypeCold);
  CHECK_EQ(CantripOf(424)->roll == CantripRoll::kDexteritySave, true);    // Acid Splash: a Dexterity save
  CHECK_EQ(CantripOf(424)->die, 6);
  CHECK_EQ(CantripOf(1051)->roll == CantripRoll::kMeleeSpellAttack, true);  // Chill Touch
  CHECK_EQ(CantripOf(1051)->damage_type, kDamageTypeNecrotic);
  CHECK_EQ(CantripOf(1052)->damage_type, kDamageTypeLightning);           // Shocking Grasp
  CHECK_EQ(CantripOf(1053)->die, 12);                                     // Poison Spray
  CHECK_EQ(CantripOf(1053)->damage_type, kDamageTypePoison);
  CHECK_EQ(CantripOf(1054)->damage_type, kDamageTypeRadiant);             // Sacred Flame
  CHECK_EQ(CantripOf(1055)->die, 8);                                      // Starry Wisp
  CHECK_EQ(CantripOf(1056)->roll == CantripRoll::kWisdomSave, true);      // Vicious Mockery
  CHECK_EQ(CantripOf(1056)->damage_type, kDamageTypePsychic);
  CHECK_EQ(CantripOf(100)->die, 0);                                       // Light: no damage
  CHECK_EQ(CantripOf(107) == nullptr, true);                              // Magic Missile
  CHECK_EQ(CantripOf(37) == nullptr, true);                               // Daze: retired
  // 2024's types NWN lacks are custom types of their own (owner: "poison and acid are different things and force
  // and psychic aren't magical"): none is NWN's Acid or Magical.
  CHECK_EQ(kDamageTypePoison != kDamageTypeAcid && kDamageTypePoison == 8192u, true);
  CHECK_EQ(kDamageTypeForce == 16384u && kDamageTypePsychic == 32768u, true);
  const std::vector<Cantrip>& all = Cantrips();
  CHECK_EQ(all.size(), 10u);
  CHECK_EQ(std::is_sorted(all.begin(), all.end(), [](const Cantrip& a, const Cantrip& b) { return a.spell < b.spell; }),
           true);
}

// 4A: NWN's own cantrips retired in faithful.
TEST(CantripsRetired) {
  CHECK_EQ(IsRetiredCantrip(37), true);    // Daze
  CHECK_EQ(IsRetiredCantrip(416), true);   // Flare
  CHECK_EQ(IsRetiredCantrip(439), true);   // Electric Jolt
  CHECK_EQ(IsRetiredCantrip(189), true);   // Virtue
  CHECK_EQ(IsRetiredCantrip(33), true);    // Cure Minor Wounds
  CHECK_EQ(IsRetiredCantrip(431), true);   // Inflict Minor Wounds
  CHECK_EQ(IsRetiredCantrip(144), false);  // Ray of Frost stays
  CHECK_EQ(IsRetiredCantrip(100), false);  // Light stays
  CHECK_EQ(RetiredCantrips().size(), 6u);
}

// #39 build 2: the choice cantrips' options are SRD 5.2.1's, each a variant row of its own.
TEST(ChoiceCantripsAreSrd52) {
  const ChoiceCantrip* resistance = ChoiceCantripOf(kResistance);
  CHECK_EQ(resistance != nullptr, true);
  CHECK_EQ(resistance->master, 151);
  CHECK_EQ(resistance->kind == ChoiceKind::kDamageType, true);
  CHECK_EQ(resistance->options.size(), 11u);
  CHECK_EQ(resistance->options.front().spell, 1060);
  CHECK_EQ(resistance->options.front().value, kDamageTypeAcid);
  CHECK_EQ(resistance->options[1].value, 1u);    // Bludgeoning
  CHECK_EQ(resistance->options[6].value, 2u);    // Piercing
  CHECK_EQ(resistance->options[7].value, kDamageTypePoison);
  CHECK_EQ(resistance->options[9].value, 4u);    // Slashing
  CHECK_EQ(resistance->options.back().spell, 1070);
  CHECK_EQ(resistance->options.back().value, kDamageTypeThunder);
  const ChoiceCantrip* guidance = ChoiceCantripOf(kGuidance);
  CHECK_EQ(guidance->kind == ChoiceKind::kSkill, true);
  CHECK_EQ(guidance->options.size(), 18u);
  CHECK_EQ(guidance->options.front().spell, 1071);
  CHECK_EQ(guidance->options.front().value, static_cast<uint32_t>(Skill2024::kAcrobatics));
  CHECK_EQ(guidance->options[11].value, static_cast<uint32_t>(Skill2024::kPerception));
  CHECK_EQ(guidance->options.back().spell, 1088);
  CHECK_EQ(guidance->options.back().value, static_cast<uint32_t>(Skill2024::kSurvival));
  CHECK_EQ(strcmp(guidance->options[15].label, "Sleight of Hand"), 0);
  const ChoiceCantrip* burst = ChoiceCantripOf(kSorcerousBurst);
  CHECK_EQ(burst->options.size(), 7u);
  CHECK_EQ(burst->options.front().spell, 1089);
  CHECK_EQ(burst->options[5].value, kDamageTypePsychic);
  CHECK_EQ(burst->options.back().spell, 1095);
  CHECK_EQ(burst->options.back().value, kDamageTypeThunder);
  for (const CantripOption& option : burst->options) {
    CHECK_EQ(option.value != 1u && option.value != 2u && option.value != 4u, true);  // no weapon damage
  }
  // Every option's row comes after its master's (build 4b: Thaumaturgy's Thunder, 1134, after Mage Hand's 1122), and
  // no row is two things.
  std::vector<int> rows;
  for (const ChoiceCantrip& cantrip : ChoiceCantrips()) {
    rows.push_back(cantrip.master);
    for (size_t i = 0; i < cantrip.options.size(); ++i) {
      rows.push_back(cantrip.options[i].spell);
      CHECK_EQ(cantrip.options[i].spell > cantrip.master, true);
    }
  }
  std::sort(rows.begin(), rows.end());
  CHECK_EQ(std::adjacent_find(rows.begin(), rows.end()) == rows.end(), true);
  for (int row : rows) CHECK_EQ(CantripOf(row) == nullptr || row == 151, true);  // not a build 1 cantrip
  CHECK_EQ(ChoiceCantripOf(1065)->master, 151);   // Resistance: Necrotic
  CHECK_EQ(ChoiceCantripOf(1080)->master, 1057);  // Guidance: Medicine
  CHECK_EQ(ChoiceCantripOf(1050) == nullptr, true);
  CHECK_EQ(ChoiceCantripOf(kSpareTheDying) == nullptr, true);
  CHECK_EQ(ChoiceOptionOf(151) == nullptr, true);  // the master: no choice
  CHECK_EQ(ChoiceOptionOf(1094)->value, kDamageTypePsychic);
  CHECK_EQ(ChoiceOptionOf(1080)->value, static_cast<uint32_t>(Skill2024::kMedicine));
}

// #39 build 3b: Shillelagh - a Bonus Action, its die by level, Force or the weapon's type.
TEST(Shillelagh) {
  const ChoiceCantrip* shillelagh = ChoiceCantripOf(kShillelagh);
  CHECK_EQ(shillelagh != nullptr, true);
  CHECK_EQ(shillelagh->kind == ChoiceKind::kDamageType, true);
  CHECK_EQ(shillelagh->options.size(), 2u);
  CHECK_EQ(shillelagh->options[0].spell, 1100);
  CHECK_EQ(shillelagh->options[1].spell, 1101);
  CHECK_EQ(WeaponCantripDamageType(1099), kDamageTypeForce);  // cast with no choice: Force
  CHECK_EQ(WeaponCantripDamageType(1100), kDamageTypeForce);
  CHECK_EQ(WeaponCantripDamageType(1101), 0u);                // the weapon's own
  CHECK_EQ(BonusActionFeatOfSpell(1099), 1549);
  CHECK_EQ(BonusActionFeatOfSpell(1100), 1549);
  CHECK_EQ(BonusActionFeatOfSpell(1101), 1550);
  CHECK_EQ(IsBonusActionSpellFeat(1549), true);
  CHECK_EQ(IsBonusActionSpellFeat(1550), true);
  CHECK_EQ(IsBonusActionSpellFeat(1551), false);
  CHECK_EQ(EffectIconOf(1099), 161);
  CHECK_EQ(EffectIconOf(1100), 162);
  CHECK_EQ(EffectIconOf(1101), 163);
  CHECK_EQ(IsShillelagh(1099) && IsShillelagh(1100) && IsShillelagh(1101), true);
  CHECK_EQ(IsShillelagh(1102) || IsShillelagh(1098), false);
  CHECK_EQ(IsShillelaghWeapon(28), true);  // Club
  CHECK_EQ(IsShillelaghWeapon(50), true);  // Quarterstaff
  CHECK_EQ(IsShillelaghWeapon(45), true);  // NWN's magic staff
  CHECK_EQ(IsShillelaghWeapon(22), false);  // a dagger
  CHECK_EQ(IsShillelaghWeapon(-1), false);  // nothing in hand
  CHECK_EQ(ShillelaghDie(1).count, 1);
  CHECK_EQ(ShillelaghDie(4).sides, 8);
  CHECK_EQ(ShillelaghDie(5).sides, 10);
  CHECK_EQ(ShillelaghDie(10).sides, 10);
  CHECK_EQ(ShillelaghDie(11).sides, 12);
  CHECK_EQ(ShillelaghDie(16).count, 1);
  CHECK_EQ(ShillelaghDie(17).count, 2);
  CHECK_EQ(ShillelaghDie(17).sides, 6);
  // The better die: 2d6 (7) over d12 (6.5), d8 over the staff's d6; a Monk's d10 over Shillelagh's d8; a tie keeps a.
  CHECK_EQ(BetterDice({1, 6}, {1, 8}).sides, 8);
  CHECK_EQ(BetterDice({1, 10}, {1, 8}).sides, 10);
  CHECK_EQ(BetterDice({1, 12}, {2, 6}).count, 2);
  CHECK_EQ(BetterDice({2, 4}, {1, 4}).count, 2);
  CHECK_EQ(BetterDice({1, 8}, {1, 8}).count, 1);
  CHECK_EQ(BetterDice({2, 4}, {1, 9}).count, 2);  // 5 each: the first kept
  // "can use your spellcasting ability instead of Strength": the better.
  CHECK_EQ(WeaponCantripAbility(false, 1, 3), 3);
  CHECK_EQ(WeaponCantripAbility(false, 4, 3), 4);
  CHECK_EQ(WeaponCantripAbility(false, -1, -2), -1);
}

// #39 build 3b: True Strike - the spellcasting ability in place of Strength or Dexterity; Radiant dice by level.
TEST(TrueStrike) {
  const ChoiceCantrip* strike = ChoiceCantripOf(kTrueStrike);
  CHECK_EQ(strike != nullptr, true);
  CHECK_EQ(strike->master, 415);
  CHECK_EQ(strike->options.size(), 2u);
  CHECK_EQ(strike->options[0].spell, 1102);
  CHECK_EQ(strike->options[1].spell, 1103);
  CHECK_EQ(WeaponCantripDamageType(415), kDamageTypeRadiant);
  CHECK_EQ(WeaponCantripDamageType(1102), kDamageTypeRadiant);
  CHECK_EQ(WeaponCantripDamageType(1103), 0u);
  CHECK_EQ(WeaponCantripDamageType(1094), 0u);  // Sorcerous Burst: not a weapon cantrip
  CHECK_EQ(BonusActionFeatOfSpell(415), -1);    // an action
  CHECK_EQ(BonusActionFeatOfSpell(1102), -1);
  CHECK_EQ(EffectIconOf(1102), -1);
  CHECK_EQ(IsTrueStrike(415) && IsTrueStrike(1102) && IsTrueStrike(1103), true);
  CHECK_EQ(IsTrueStrike(1101) || IsTrueStrike(414), false);
  CHECK_EQ(WeaponCantripAbility(true, 4, 3), 3);  // instead of: even when lower
  CHECK_EQ(WeaponCantripAbility(true, 1, 3), 3);
  CHECK_EQ(TrueStrikeExtraDice(1, false), 0);
  CHECK_EQ(TrueStrikeExtraDice(4, true), 0);
  CHECK_EQ(TrueStrikeExtraDice(5, false), 1);
  CHECK_EQ(TrueStrikeExtraDice(5, true), 2);
  CHECK_EQ(TrueStrikeExtraDice(10, false), 1);
  CHECK_EQ(TrueStrikeExtraDice(11, false), 2);
  CHECK_EQ(TrueStrikeExtraDice(17, false), 3);
  CHECK_EQ(TrueStrikeExtraDice(20, true), 6);
}

// #39 build 3b: a hit's damage entries - the weapon's damage moved to Force or Radiant, Radiant dice added.
TEST(WeaponCantripDamage) {
  CHECK_EQ(DamageTypeIndex(1), 0);
  CHECK_EQ(DamageTypeIndex(kDamageTypeRadiant), 6);
  CHECK_EQ(DamageTypeIndex(4096), 12);
  CHECK_EQ(DamageTypeIndex(kDamageTypeForce), 14);
  CHECK_EQ(DamageTypeIndex(0), -1);
  CHECK_EQ(DamageTypeIndex(3), -1);
  int16_t entries[32];
  for (int16_t& e : entries) e = -1;
  entries[12] = 9;  // the weapon's
  entries[8] = 3;   // a flaming weapon's fire stays
  CHECK_EQ(MoveWeaponDamage(entries, 32, kDamageTypeForce), 9);
  CHECK_EQ(entries[14], 9);
  CHECK_EQ(entries[12], 0);
  CHECK_EQ(entries[8], 3);
  CHECK_EQ(MoveWeaponDamage(entries, 32, kDamageTypeForce), 0);  // nothing left to move
  CHECK_EQ(MoveWeaponDamage(entries, 32, 0), 0);                 // the weapon's own: stays
  entries[12] = 5;
  entries[6] = 2;
  CHECK_EQ(MoveWeaponDamage(entries, 32, kDamageTypeRadiant), 5);
  CHECK_EQ(entries[6], 7);  // added to what was there
  AddDamage(entries, 32, kDamageTypeRadiant, 4);
  CHECK_EQ(entries[6], 11);
  AddDamage(entries, 32, kDamageTypeCold, 3);  // an unused entry starts from 0
  CHECK_EQ(entries[5], 3);
  AddDamage(entries, 32, kDamageTypeCold, 0);
  CHECK_EQ(entries[5], 3);
  AddDamage(nullptr, 32, kDamageTypeCold, 3);  // no crash
}

// #39 build 4a: the utility cantrips' menus - SRD 5.2.1's effects, less those the owner left out.
TEST(UtilityCantrips) {
  // 0.93.1 took Weather Sensor, Bloom and Sensory Effect out; build 4b gave back the animal's sound (Q3 A).
  const ChoiceCantrip* druidcraft = ChoiceCantripOf(kDruidcraft);
  CHECK_EQ(druidcraft != nullptr, true);
  CHECK_EQ(druidcraft->options.size(), 2u);
  CHECK_EQ(UtilityEffectOf(kDruidcraft) == UtilityEffect::kFirePlay, true);  // cast with no choice: Fire Play
  CHECK_EQ(UtilityEffectOf(1107) == UtilityEffect::kFirePlay, true);
  CHECK_EQ(UtilityEffectOf(1108) == UtilityEffect::kAnimalSound, true);
  CHECK_EQ(UtilityEffectOf(1105) == UtilityEffect::kNone, true);  // its old menu's rows: unused
  const ChoiceCantrip* elementalism = ChoiceCantripOf(kElementalism);
  CHECK_EQ(elementalism->options.size(), 4u);  // no Sculpt Element
  CHECK_EQ(strcmp(elementalism->options[2].label, "Beckon Fire"), 0);
  const ChoiceCantrip* prestidigitation = ChoiceCantripOf(kPrestidigitation);
  CHECK_EQ(prestidigitation->options.size(), 8u);  // Fire Play and Magic Mark in seven colours (0.96.1)
  const ChoiceCantrip* thaumaturgy = ChoiceCantripOf(kThaumaturgy);
  CHECK_EQ(thaumaturgy->options.size(), 5u);  // no Altered Eyes or Tremors; Phantom Sound's whispers and thunder
  CHECK_EQ(thaumaturgy->options[3].spell, kPhantomWhispers);
  CHECK_EQ(thaumaturgy->options[4].spell, kPhantomThunder);
  CHECK_EQ(ChoiceCantripOf(kMageHand) == nullptr, true);              // no menu
  CHECK_EQ(UtilityEffectOf(1109) == UtilityEffect::kBeckonAir, true);  // cast with no choice: the first
  CHECK_EQ(UtilityEffectOf(1113) == UtilityEffect::kBeckonWater, true);
  CHECK_EQ(UtilityEffectOf(1115) == UtilityEffect::kFirePlay, true);
  CHECK_EQ(UtilityEffectOf(1116) == UtilityEffect::kMagicMark, true);
  CHECK_EQ(UtilityEffectOf(1119) == UtilityEffect::kThaumaturgyFirePlay, true);
  CHECK_EQ(UtilityEffectOf(1120) == UtilityEffect::kInvisibleHand, true);
  CHECK_EQ(UtilityEffectOf(kMageHand) == UtilityEffect::kMageHand, true);
  CHECK_EQ(UtilityEffectOf(kPhantomWhispers) == UtilityEffect::kPhantomWhispers, true);
  CHECK_EQ(UtilityEffectOf(kPhantomThunder) == UtilityEffect::kPhantomThunder, true);
  CHECK_EQ(UtilityEffectOf(kProduceFlameFlame) == UtilityEffect::kNone, true);  // a mode of its own
  CHECK_EQ(UtilityEffectOf(kProduceFlameHurl) == UtilityEffect::kNone, true);   // its mode 1 isn't Weather Sensor
  CHECK_EQ(UtilityEffectOf(1050) == UtilityEffect::kNone, true);
  CHECK_EQ(EffectIconOf(1118), -1);  // Booming Voice's icon is its own (kBoomingVoiceIcon)
  CHECK_EQ(BonusActionFeatOfSpell(1104), -1);  // all actions
  CHECK_EQ(BonusActionFeatOfSpell(kMageHand), -1);
}

// #39 build 4a: what each effect does to a light and a door; Elementalism's Cube; the ranges.
TEST(UtilityCantripEffects) {
  CHECK_EQ(FirePlayOf(UtilityEffect::kFirePlay) == FirePlay::kToggle, true);
  CHECK_EQ(FirePlayOf(UtilityEffect::kMageHand) == FirePlay::kToggle, true);
  CHECK_EQ(FirePlayOf(UtilityEffect::kBeckonFire) == FirePlay::kLight, true);  // embers light, don't snuff
  CHECK_EQ(FirePlayOf(UtilityEffect::kBeckonWater) == FirePlay::kSnuff, true);
  CHECK_EQ(FirePlayOf(UtilityEffect::kThaumaturgyFirePlay) == FirePlay::kDim, true);
  CHECK_EQ(FirePlayOf(UtilityEffect::kBeckonAir) == FirePlay::kNone, true);
  CHECK_EQ(FirePlayOf(UtilityEffect::kMagicMark) == FirePlay::kNone, true);
  CHECK_EQ(DoorPlayOf(UtilityEffect::kInvisibleHand) == DoorPlay::kToggle, true);
  CHECK_EQ(DoorPlayOf(UtilityEffect::kMageHand) == DoorPlay::kToggle, true);
  CHECK_EQ(DoorPlayOf(UtilityEffect::kBeckonAir) == DoorPlay::kClose, true);  // closes, never opens
  CHECK_EQ(DoorPlayOf(UtilityEffect::kFirePlay) == DoorPlay::kNone, true);
  CHECK_EQ(FillsElementCube(UtilityEffect::kBeckonAir), true);
  CHECK_EQ(FillsElementCube(UtilityEffect::kBeckonWater), true);
  CHECK_EQ(FillsElementCube(UtilityEffect::kFirePlay), false);
  CHECK_EQ(FillsElementCube(UtilityEffect::kMageHand), false);
  // 0.94.1: Magic Mark's five symbols, rows 1840-1844.
  CHECK_EQ(MagicMarkVisual(1), 1860);  // 0.96.0: the owner's 12 symbols
  CHECK_EQ(MagicMarkVisual(12), 1871);
  CHECK_EQ(MagicMarkVisual(6), 1865);
  CHECK_EQ(MagicMarkVisual(0), 1860);  // out of range: the first
  CHECK_EQ(MagicMarkVisual(13), 1860);
  CHECK_EQ(MagicMarkUprightVisual(1), 1880);  // upright on a door or a placeable
  CHECK_EQ(MagicMarkUprightVisual(12), 1891);
  CHECK_EQ(MagicMarkUprightVisual(13), 1880);
  CHECK_EQ(CantripRangeFeet(kDruidcraft), 30);
  CHECK_EQ(CantripRangeFeet(1112), 30);   // Elementalism
  CHECK_EQ(CantripRangeFeet(1115), 10);   // Prestidigitation
  CHECK_EQ(CantripRangeFeet(1116), 10);
  CHECK_EQ(CantripRangeFeet(1120), 30);   // Thaumaturgy
  CHECK_EQ(CantripRangeFeet(kMageHand), 30);
  CHECK_EQ(CantripRangeFeet(1118), 0);    // Booming Voice
  CHECK_EQ(CantripRangeFeet(1050), 0);    // not one
}

// #39 build 4b: the sounds - which draw creatures, which scare beasts away (Q7 A), and who flees.
TEST(UtilitySounds) {
  CHECK_EQ(SoundPlayOf(UtilityEffect::kIllusionVoices) == SoundPlay::kDraws, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kIllusionFootsteps) == SoundPlay::kDraws, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kPhantomWhispers) == SoundPlay::kDraws, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kIllusionRoar) == SoundPlay::kScares, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kIllusionThunder) == SoundPlay::kScares, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kPhantomThunder) == SoundPlay::kScares, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kAnimalSound) == SoundPlay::kScares, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kImageObject) == SoundPlay::kNone, true);
  CHECK_EQ(SoundPlayOf(UtilityEffect::kFirePlay) == SoundPlay::kNone, true);
  CHECK_EQ(FleesFromScaringSound(8), true);    // animal
  CHECK_EQ(FleesFromScaringSound(9), true);    // beast
  CHECK_EQ(FleesFromScaringSound(25), true);   // vermin
  CHECK_EQ(FleesFromScaringSound(19), false);  // magical beast
  CHECK_EQ(FleesFromScaringSound(6), false);   // humanoid
  for (UtilityEffect e : {UtilityEffect::kIllusionVoices, UtilityEffect::kIllusionFootsteps,
                          UtilityEffect::kIllusionRoar, UtilityEffect::kIllusionThunder, UtilityEffect::kPhantomWhispers,
                          UtilityEffect::kPhantomThunder, UtilityEffect::kAnimalSound}) {
    CHECK_EQ(SoundResrefOf(e)[0] != 0, true);
  }
  CHECK_EQ(SoundResrefOf(UtilityEffect::kFirePlay)[0], 0);
  CHECK_EQ(strcmp(SoundResrefOf(UtilityEffect::kIllusionThunder), SoundResrefOf(UtilityEffect::kPhantomThunder)), 0);
  CHECK_EQ(kScareSeconds, 12);  // 2 rounds
}

// #39 build 4b: Minor Illusion's menu - four sounds and three images - and seeing through an image.
TEST(MinorIllusion) {
  const ChoiceCantrip* illusion = ChoiceCantripOf(kMinorIllusion);
  CHECK_EQ(illusion != nullptr, true);
  CHECK_EQ(illusion->options.size(), 7u);  // one radial page
  CHECK_EQ(RadialChoicePage(7, 0).more, false);
  CHECK_EQ(UtilityEffectOf(kMinorIllusion) == UtilityEffect::kIllusionVoices, true);  // no choice: the first
  CHECK_EQ(UtilityEffectOf(1127) == UtilityEffect::kIllusionThunder, true);
  CHECK_EQ(UtilityEffectOf(1130) == UtilityEffect::kImageCreature, true);
  CHECK_EQ(ImageKindOf(UtilityEffectOf(1128)) == ImageKind::kObject, true);
  CHECK_EQ(ImageKindOf(UtilityEffectOf(1129)) == ImageKind::kHumanoid, true);
  CHECK_EQ(ImageKindOf(UtilityEffect::kIllusionVoices) == ImageKind::kNone, true);
  CHECK_EQ(strcmp(ImageBlueprintOf(ImageKind::kObject), "plc_barrel"), 0);
  CHECK_EQ(strcmp(ImageBlueprintOf(ImageKind::kHumanoid), "nw_commale"), 0);
  CHECK_EQ(strcmp(ImageBlueprintOf(ImageKind::kCreature), "nw_cat"), 0);
  CHECK_EQ(ImageBlueprintOf(ImageKind::kNone)[0], 0);
  CHECK_EQ(SeesThroughIllusion(10, 2, 12), true);   // meets the DC
  CHECK_EQ(SeesThroughIllusion(10, 1, 12), false);
  CHECK_EQ(CantripRangeFeet(1124), 30);
  CHECK_EQ(CantripRangeFeet(1121), 30);  // Phantom Sound
  CHECK_EQ(CantripRangeFeet(1108), 30);  // Animal Sound
}

// #39 build 4b: Dancing Lights - 120 ft, its Move a Bonus Action up to 60 ft; the patches.
TEST(DancingLightsAndPatches) {
  const ChoiceCantrip* lights = ChoiceCantripOf(kDancingLights);
  CHECK_EQ(lights != nullptr, true);
  CHECK_EQ(lights->options.size(), 2u);
  CHECK_EQ(UtilityEffectOf(kDancingLights) == UtilityEffect::kDancingLights, true);  // no choice: the lights
  CHECK_EQ(UtilityEffectOf(kDancingLightsMove) == UtilityEffect::kDancingLightsMove, true);
  CHECK_EQ(CantripRangeFeet(kDancingLightsLights), 120);
  CHECK_EQ(CantripRangeFeet(kDancingLightsMove), 120);
  CHECK_EQ(kDancingLightsCount, 4);
  CHECK_EQ(DancingLightsMoveAllowed(60.0f, 100.0f), true);
  CHECK_EQ(DancingLightsMoveAllowed(61.0f, 100.0f), false);   // farther than the move
  CHECK_EQ(DancingLightsMoveAllowed(10.0f, 121.0f), false);   // out of the spell's range
  CHECK_EQ(DancingLightsMoveAllowed(0.0f, 120.0f), true);
  CHECK_EQ(PatchAreaEffectOf(UtilityEffect::kBeckonWater), kPuddleAreaEffect);
  CHECK_EQ(PatchAreaEffectOf(UtilityEffect::kBeckonEarth), kDustAreaEffect);
  CHECK_EQ(PatchAreaEffectOf(UtilityEffect::kBeckonFire), -1);
  CHECK_EQ(PatchSecondsOf(UtilityEffect::kBeckonWater), 60);   // its water evaporates in 1 minute
  CHECK_EQ(PatchSecondsOf(UtilityEffect::kBeckonEarth), 600);  // the owner's 10 minutes
  CHECK_EQ(PatchSecondsOf(UtilityEffect::kBeckonAir), 0);
}

// 0.90.1: the effect icon each shows while it lasts - effecticons.2da rows 130-160, past the game's 129.
TEST(ChoiceCantripIcons) {
  CHECK_EQ(EffectIconOf(151), 130);   // Resistance, cast with no choice
  CHECK_EQ(EffectIconOf(1060), 131);  // Resistance: Acid
  CHECK_EQ(EffectIconOf(1066), 137);  // Resistance: Piercing
  CHECK_EQ(EffectIconOf(1070), 141);  // Resistance: Thunder
  CHECK_EQ(EffectIconOf(1057), 142);  // Guidance
  CHECK_EQ(EffectIconOf(1071), 143);  // Guidance: Acrobatics
  CHECK_EQ(EffectIconOf(1087), 159);  // Guidance: Stealth
  CHECK_EQ(EffectIconOf(1088), 160);  // Guidance: Survival, the last
  CHECK_EQ(EffectIconOf(1059), -1);   // Sorcerous Burst: no lasting effect
  CHECK_EQ(EffectIconOf(1091), -1);
  CHECK_EQ(EffectIconOf(1050), -1);   // Fire Bolt
}

// Build 3a: Produce Flame's Flame and Hurl; the Flame (or the cantrip cast with no choice) a Bonus Action.
TEST(ProduceFlame) {
  const ChoiceCantrip* flame = ChoiceCantripOf(kProduceFlame);
  CHECK_EQ(flame != nullptr, true);
  CHECK_EQ(flame->kind == ChoiceKind::kMode, true);
  CHECK_EQ(flame->options.size(), 2u);
  CHECK_EQ(flame->options[0].spell, kProduceFlameFlame);
  CHECK_EQ(flame->options[1].spell, kProduceFlameHurl);
  CHECK_EQ(ChoiceCantripOf(1098)->master, 1096);
  CHECK_EQ(RadialChoicePage(2, 0).shown, 2);  // one page
  CHECK_EQ(BonusActionFeatOfSpell(1096), 1548);  // the cantrip cast: its Flame
  CHECK_EQ(BonusActionFeatOfSpell(1097), 1548);
  CHECK_EQ(BonusActionFeatOfSpell(1098), -1);    // Hurl: an action
  CHECK_EQ(BonusActionFeatOfSpell(1050), -1);
  CHECK_EQ(IsBonusActionSpellFeat(1548), true);
  CHECK_EQ(IsBonusActionSpellFeat(1547), false);
  CHECK_EQ(EffectIconOf(1097), -1);
}

// Decision 5: eight options to a page, or seven and "More".
TEST(RadialChoicePage) {
  CHECK_EQ(RadialChoicePage(7, 0).shown, 7);   // Sorcerous Burst: one page
  CHECK_EQ(RadialChoicePage(7, 0).more, false);
  CHECK_EQ(RadialChoicePage(8, 0).shown, 8);
  CHECK_EQ(RadialChoicePage(8, 0).more, false);
  CHECK_EQ(RadialChoicePage(9, 0).shown, 7);
  CHECK_EQ(RadialChoicePage(9, 0).more, true);
  CHECK_EQ(RadialChoicePage(9, 7).shown, 2);
  CHECK_EQ(RadialChoicePage(11, 0).shown, 7);  // Resistance: seven and More, then four
  CHECK_EQ(RadialChoicePage(11, 0).more, true);
  CHECK_EQ(RadialChoicePage(11, 7).shown, 4);
  CHECK_EQ(RadialChoicePage(11, 7).more, false);
  CHECK_EQ(RadialChoicePage(18, 7).shown, 7);  // Guidance: 7 + More, 7 + More, 4
  CHECK_EQ(RadialChoicePage(18, 7).more, true);
  CHECK_EQ(RadialChoicePage(18, 14).shown, 4);
  CHECK_EQ(RadialChoicePage(18, 14).more, false);
  CHECK_EQ(RadialChoicePage(0, 0).shown, 0);
  CHECK_EQ(RadialChoicePage(5, 5).shown, 0);
  CHECK_EQ(RadialChoicePage(5, -1).shown, 0);
}

// The page rides in the radial param; an option's cast keeps the class bits and drops the page.
TEST(RadialChoiceParams) {
  const long long menu = (2LL << 28) | 151LL | (3LL << 40);  // class slot 2, Resistance, #36's from-level 3
  CHECK_EQ(RadialChoiceFirst(menu), 0);
  const long long page = RadialChoiceWithFirst(menu, 14);
  CHECK_EQ(RadialChoiceFirst(page), 14);
  CHECK_EQ(page & 0xFFFF, 151LL);
  CHECK_EQ((page >> 28) & 3, 2LL);
  const long long cast = RadialChoiceCast(page, 1063);  // Resistance: Fire
  CHECK_EQ(cast & 0xFFFF, 1063LL);
  CHECK_EQ((cast >> 28) & 3, 2LL);
  CHECK_EQ(RadialChoiceFirst(cast), 0);
  CHECK_EQ((cast >> 40) & 0xF, 0LL);
  CHECK_EQ(RadialChoiceFirst(RadialChoiceWithFirst(page, 7)), 7);
}

namespace {
struct Rolls {
  std::vector<int> values;
  size_t next = 0;
};
int NextRoll(void* context) {
  Rolls* rolls = static_cast<Rolls*>(context);
  return rolls->next < rolls->values.size() ? rolls->values[rolls->next++] : 1;
}
}  // namespace

// Sorcerous Burst: each 8 adds a d8, at most the spellcasting modifier more.
TEST(SorcerousBurstDice) {
  Rolls a{{8, 3, 8, 8, 2}};
  int extra = -1;
  CHECK_EQ(SorcerousBurstDamage(2, 3, &NextRoll, &a, &extra), 29);  // 8 3, then 8 8 2
  CHECK_EQ(extra, 3);
  Rolls b{{8, 8, 5}};
  CHECK_EQ(SorcerousBurstDamage(1, 1, &NextRoll, &b, &extra), 16);  // one more only
  CHECK_EQ(extra, 1);
  CHECK_EQ(b.next, 2u);
  Rolls c{{8, 8}};
  CHECK_EQ(SorcerousBurstDamage(2, 0, &NextRoll, &c, &extra), 16);  // modifier +0: none
  CHECK_EQ(extra, 0);
  Rolls d{{8, 4}};
  CHECK_EQ(SorcerousBurstDamage(1, -1, &NextRoll, &d, &extra), 8);  // a negative modifier: none
  Rolls e{{5, 6, 7, 2}};
  CHECK_EQ(SorcerousBurstDamage(4, 5, &NextRoll, &e, &extra), 20);  // no 8: the dice alone
  CHECK_EQ(extra, 0);
  Rolls f{{}};
  CHECK_EQ(SorcerousBurstDamage(0, 5, &NextRoll, &f, nullptr), 0);
}

// Resistance's 1d4 off (at most the damage); Spare the Dying's range doubles at 5, 11, 17.
TEST(ResistanceAndSpareTheDying) {
  CHECK_EQ(ResistanceReduction(10, 3), 3);
  CHECK_EQ(ResistanceReduction(2, 4), 2);
  CHECK_EQ(ResistanceReduction(0, 3), 0);
  CHECK_EQ(ResistanceReduction(5, 0), 0);
  CHECK_EQ(SpareTheDyingRangeFeet(1), 15);
  CHECK_EQ(SpareTheDyingRangeFeet(4), 15);
  CHECK_EQ(SpareTheDyingRangeFeet(5), 30);
  CHECK_EQ(SpareTheDyingRangeFeet(10), 30);
  CHECK_EQ(SpareTheDyingRangeFeet(11), 60);
  CHECK_EQ(SpareTheDyingRangeFeet(16), 60);
  CHECK_EQ(SpareTheDyingRangeFeet(17), 120);
  CHECK_EQ(SpareTheDyingRangeFeet(20), 120);
}

namespace {
bool Near(float a, float b) { return a - b < 0.005f && b - a < 0.005f; }
bool At(const MarkSpot& s, float x, float y, float z, float facing) {
  return Near(s.x, x) && Near(s.y, y) && Near(s.z, z) && Near(s.facing, facing);
}
}  // namespace

// 0.96.0 (owner, 2026-11-08: "Go with A"): where Magic Mark stands on a door or a placeable. Breaks caught: the door's
// frame turned the wrong way, the face behind the door taken, an oblique look not landing on the front face, the
// side face missed edge-on, the gap or the height dropped, the mark not facing out of its face or toward the caster,
// a placeable's surface or its no-walkmesh reach wrong, the halving search off.
TEST(MagicMarkOnObjects) {
  // A door facing north (its frame the world's) at (10, 10, 0).
  CHECK_EQ(At(MarkOnDoor(10, 10, 0, 90, 10, 15), 10, 10.18f, 1.4f, 90), true);     // the caster north: the front
  CHECK_EQ(At(MarkOnDoor(10, 10, 0, 90, 10, 5), 10, 9.82f, 1.4f, 270), true);      // south: the back
  CHECK_EQ(At(MarkOnDoor(10, 10, 0, 90, 15, 15), 10.15f, 10.18f, 1.4f, 90), true);  // north-east: still the front
  CHECK_EQ(At(MarkOnDoor(10, 10, 0, 90, 20, 10), 11.08f, 10, 1.4f, 0), true);      // edge-on from the east: its side
  // A door facing east: its front looks east.
  CHECK_EQ(At(MarkOnDoor(10, 10, 2, 0, 20, 10), 10.18f, 10, 3.4f, 0), true);
  CHECK_EQ(At(MarkOnDoor(10, 10, 2, 0, 0, 10), 9.82f, 10, 3.4f, 180), true);
  // A placeable at the origin, the caster 5 m south; its walkmesh starts 0.9 of the way (0.5 m short of its centre).
  CHECK_EQ(At(MarkOnPlaceable(0, 0, 0, -5, 0.9f, 1.0f), 0, -0.53f, 1.7f, 270), true);
  CHECK_EQ(At(MarkOnPlaceable(0, 0, 0, -5, -1.0f, 0.0f), 0, -0.35f, 0.7f, 270), true);  // no walkmesh
  CHECK_EQ(At(MarkOnPlaceable(0, 0, 5, 0, 0.9f, 0.0f), 0.53f, 0, 0.7f, 0), true);       // from the east: faces east
  // The search for the walkmesh's start.
  const float share = ClearShare([](float t) { return t > 0.6f; }, 14);
  CHECK_EQ(Near(share, 0.6f), true);
  CHECK_EQ(share <= 0.6f, true);  // the clear side of the edge
  CHECK_EQ(Near(ClearShare([](float) { return false; }, 14), -1.0f), true);  // nothing in the way
}

// 0.96.1 (owner, 2026-11-09: "Option b + random"): Magic Mark's colour. Breaks caught: a colour's row read as another,
// Random or another row taken for a colour, a colour's rows overlapping another's or the upright ones, White not
// keeping its rows, the menu not fitting one radial page.
TEST(MagicMarkColours) {
  CHECK_EQ(MagicMarkTintOf(kMagicMarkWhite), 0);
  CHECK_EQ(MagicMarkTintOf(kMagicMarkGold), 1);
  CHECK_EQ(MagicMarkTintOf(1139), 5);  // Violet
  CHECK_EQ(MagicMarkTintOf(kMagicMarkRandom), -1);
  CHECK_EQ(MagicMarkTintOf(1115), -1);  // Fire Play
  CHECK_EQ(MagicMarkRow(1, 0, false), 1860);  // White: 0.96.0's rows
  CHECK_EQ(MagicMarkRow(12, 0, true), 1891);
  CHECK_EQ(MagicMarkRow(1, 1, false), 1900);  // Gold
  CHECK_EQ(MagicMarkRow(12, 1, false), 1911);
  CHECK_EQ(MagicMarkRow(1, 1, true), 1920);
  CHECK_EQ(MagicMarkRow(12, 1, true), 1931);
  CHECK_EQ(MagicMarkRow(1, 2, false), 1940);  // Red
  CHECK_EQ(MagicMarkRow(12, 5, true), 2091);  // Violet, upright, the last
  CHECK_EQ(MagicMarkRow(13, 3, false), 1980);  // out of range: the first symbol
  CHECK_EQ(MagicMarkRow(2, 6, false), 1861);  // out of range: White
  CHECK_EQ(MagicMarkRow(2, -1, true), 1881);
  // Prestidigitation's menu: Fire Play and the seven Magic Marks, one radial page; every Magic Mark row a Magic Mark.
  const ChoiceCantrip* presti = ChoiceCantripOf(kPrestidigitation);
  CHECK_EQ(presti != nullptr, true);
  CHECK_EQ(static_cast<int>(presti->options.size()), 8);
  CHECK_EQ(RadialChoicePage(8, 0).shown, 8);
  CHECK_EQ(RadialChoicePage(8, 0).more, false);
  for (int row : {kMagicMarkWhite, 1135, 1136, 1137, 1138, 1139, kMagicMarkRandom}) {
    CHECK_EQ(UtilityEffectOf(row) == UtilityEffect::kMagicMark, true);
    CHECK_EQ(ChoiceCantripOf(row) == presti, true);
  }
}

// 0.96.2 (owner, 2026-11-09: "go with 1"): a mark's model, and a symbol not on the object already. Breaks caught: a
// colour's letter or the upright u wrong or missing, out-of-range symbols or colours not falling back, a taken
// symbol picked, the roll counted over the wrong symbols, all taken leaving none.
TEST(MagicMarkModels) {
  CHECK_EQ(MagicMarkModel(3, 0, false) == "n5v_mark03", true);
  CHECK_EQ(MagicMarkModel(3, 0, true) == "n5v_mark03u", true);
  CHECK_EQ(MagicMarkModel(3, 1, true) == "n5v_mark03gu", true);
  CHECK_EQ(MagicMarkModel(12, 2, false) == "n5v_mark12r", true);
  CHECK_EQ(MagicMarkModel(10, 3, true) == "n5v_mark10nu", true);
  CHECK_EQ(MagicMarkModel(7, 4, true) == "n5v_mark07bu", true);
  CHECK_EQ(MagicMarkModel(12, 5, true) == "n5v_mark12vu", true);
  CHECK_EQ(MagicMarkModel(13, 6, true) == "n5v_mark01u", true);
  CHECK_EQ(MagicMarkModel(0, -1, false) == "n5v_mark01", true);
  const std::vector<bool> none;
  CHECK_EQ(FreeMarkSymbols(none), 12);
  CHECK_EQ(FreeMarkSymbol(5, none), 5);
  const std::vector<bool> some = {true, false, true};
  CHECK_EQ(FreeMarkSymbols(some), 10);
  CHECK_EQ(FreeMarkSymbol(1, some), 2);
  CHECK_EQ(FreeMarkSymbol(2, some), 4);
  CHECK_EQ(FreeMarkSymbol(10, some), 12);
  CHECK_EQ(FreeMarkSymbol(0, some), 2);
  CHECK_EQ(FreeMarkSymbol(99, some), 12);
  const std::vector<bool> all(12, true);
  CHECK_EQ(FreeMarkSymbols(all), 12);
  CHECK_EQ(FreeMarkSymbol(5, all), 5);
}
