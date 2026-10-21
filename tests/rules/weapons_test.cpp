#include "check.h"
#include "rules/weapons.h"

using namespace rules;

// Phase 1d D4 (proposed). NWN base item ids from baseitems.2da. Breaks
// caught: a 2024 weapon with the wrong property; an NWN-only weapon left
// without one; gloves, creature weapons or unknown ids given a property.
TEST(NwnWeaponsGetTheir2024Mastery) {
  CHECK_EQ(MasteryForNwnBaseItem(22), Mastery::kNick);     // dagger
  CHECK_EQ(MasteryForNwnBaseItem(1), Mastery::kSap);       // longsword
  CHECK_EQ(MasteryForNwnBaseItem(13), Mastery::kGraze);    // greatsword
  CHECK_EQ(MasteryForNwnBaseItem(18), Mastery::kCleave);   // greataxe
  CHECK_EQ(MasteryForNwnBaseItem(51), Mastery::kVex);      // rapier
  CHECK_EQ(MasteryForNwnBaseItem(11), Mastery::kVex);      // shortbow
  CHECK_EQ(MasteryForNwnBaseItem(8), Mastery::kSlow);      // longbow
  CHECK_EQ(MasteryForNwnBaseItem(5), Mastery::kPush);      // warhammer
  CHECK_EQ(MasteryForNwnBaseItem(50), Mastery::kTopple);   // quarterstaff
  CHECK_EQ(MasteryForNwnBaseItem(6), Mastery::kPush);      // heavy crossbow
  CHECK_EQ(MasteryForNwnBaseItem(7), Mastery::kSlow);      // light crossbow
  CHECK_EQ(MasteryForNwnBaseItem(41), Mastery::kSap);      // katana (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(42), Mastery::kNick);     // kukri (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(55), Mastery::kGraze);    // scythe (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(32), Mastery::kCleave);   // dire mace (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(108), Mastery::kTopple);  // dwarven waraxe (NWN only)
  CHECK_EQ(MasteryForNwnBaseItem(36), Mastery::kNone);     // gloves (unarmed)
  CHECK_EQ(MasteryForNwnBaseItem(69), Mastery::kNone);     // creature slashing weapon
  CHECK_EQ(MasteryForNwnBaseItem(-1), Mastery::kNone);
  CHECK_EQ(MasteryForNwnBaseItem(999), Mastery::kNone);
}

// Phase 1d D3 (proposed): the 2024 classes with Weapon Mastery. Breaks
// caught: Monk (no mastery in 2024) or a full caster given it; a martial
// class left out.
TEST(WeaponMasteryClassesAre2024) {
  CHECK_EQ(HasWeaponMastery(CharacterClass::kBarbarian), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kFighter), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kPaladin), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kRanger), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kRogue), true);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kMonk), false);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kWizard), false);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kCleric), false);
  CHECK_EQ(HasWeaponMastery(CharacterClass::kBard), false);
}

// Phase 1d D1 (proposed): the extra (off-hand) attack adds no ability
// modifier to damage unless it is negative or the attacker has the
// Two-Weapon Fighting style (NWN's Two-Weapon Fighting feat). Breaks caught:
// NWN's half Strength kept; a negative modifier dropped; the style ignored.
TEST(OffHandDamageModifier) {
  CHECK_EQ(OffHandDamageAbilityModifier(3, false), 0);
  CHECK_EQ(OffHandDamageAbilityModifier(3, true), 3);
  CHECK_EQ(OffHandDamageAbilityModifier(-1, false), -1);
  CHECK_EQ(OffHandDamageAbilityModifier(-1, true), -1);
  CHECK_EQ(OffHandDamageAbilityModifier(0, false), 0);
}

// D1: Light is the 2024 property, by NWN base item. Breaks caught: a
// one-handed martial weapon (longsword, rapier) or light mace/flail counted
// as Light; kukri or kama left out.
TEST(LightWeaponsAre2024) {
  for (int light : {0, 22, 28, 37, 38, 40, 42, 53, 60}) CHECK_EQ(IsLightNwnBaseItem(light), true);
  for (int other : {1, 4, 9, 51, 13, 50, 8, -1}) CHECK_EQ(IsLightNwnBaseItem(other), false);
}

// D1: one extra attack at most, only with two Light weapons, a double weapon
// or Dual Wielder. Breaks caught: Improved Two-Weapon Fighting's second
// attack kept; non-Light pairs allowed; an attack created where NWN saw no
// second weapon.
TEST(OffHandAttacksAre2024) {
  CHECK_EQ(OffHandAttacks(1, false, false, true, true), 1);
  CHECK_EQ(OffHandAttacks(1, false, false, true, false), 0);
  CHECK_EQ(OffHandAttacks(1, false, false, false, true), 0);
  CHECK_EQ(OffHandAttacks(2, false, true, false, false), 1);
  CHECK_EQ(OffHandAttacks(1, true, false, false, false), 1);
  CHECK_EQ(OffHandAttacks(2, true, true, false, false), 1);
  CHECK_EQ(OffHandAttacks(0, false, true, true, true), 0);
}

// NWN's off-hand Strength term, which the D1 hook takes back out. Breaks
// caught: rounding up (Str +3 gives 1, not 2); halving a negative modifier.
TEST(NwnOffHandStrength) {
  CHECK_EQ(NwnOffHandStrengthDamage(3), 1);
  CHECK_EQ(NwnOffHandStrengthDamage(4), 2);
  CHECK_EQ(NwnOffHandStrengthDamage(1), 0);
  CHECK_EQ(NwnOffHandStrengthDamage(0), 0);
  CHECK_EQ(NwnOffHandStrengthDamage(-1), -1);
}

// D3 slot counts. Breaks caught: a Fighter's count not growing at 4/10/16; a
// Barbarian given Fighter counts; a non-mastery class given slots; two
// classes adding up instead of taking the larger.
TEST(WeaponMasterySlotsAre2024) {
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kFighter, 1), 3);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kFighter, 4), 4);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kFighter, 10), 5);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kFighter, 16), 6);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kBarbarian, 3), 2);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kBarbarian, 4), 3);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kBarbarian, 10), 4);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kRogue, 1), 2);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kWizard, 20), 0);
  CHECK_EQ(WeaponMasterySlots(CharacterClass::kFighter, 0), 0);
  CHECK_EQ(WeaponMasterySlots({{CharacterClass::kFighter, 1}, {CharacterClass::kRogue, 3}}), 3);
}

// D3 weapon names and the Weapon Focus / proficiency tables. Breaks caught:
// a name not round-tripping; "short sword" vs "shortsword" treated apart; a
// magic staff not counted as a quarterstaff; wrong feat ids.
TEST(MasteryWeaponTables) {
  for (int id : NwnMasteryWeapons()) {
    CHECK_EQ(NwnWeaponFromName(NwnWeaponName(id)), id);
    CHECK_EQ(MasteryForNwnBaseItem(id) != Mastery::kNone, true);
  }
  CHECK_EQ(NwnWeaponFromName("short sword"), 0);
  CHECK_EQ(NwnWeaponFromName("ShortSword"), 0);
  CHECK_EQ(NwnWeaponFromName("two-bladed sword"), 12);
  CHECK_EQ(NwnWeaponFromName("spear"), 58);
  CHECK_EQ(NwnWeaponFromName("banana"), -1);
  CHECK_EQ(MasteryKindOfNwnBaseItem(45), 50);
  CHECK_EQ(WeaponFocusFeatForNwnBaseItem(1), 106);
  CHECK_EQ(WeaponFocusFeatForNwnBaseItem(45), 96);
  CHECK_EQ(WeaponFocusFeatForNwnBaseItem(95), -1);
  CHECK_EQ(ProficiencyFeatsForNwnBaseItem(1).size(), static_cast<size_t>(2));
  CHECK_EQ(ProficiencyFeatsForNwnBaseItem(45).size(), static_cast<size_t>(0));
  CHECK_EQ(std::string(MasteryName(MasteryForNwnBaseItem(1))) == "Sap", true);
}

// D3 slot helpers. Breaks caught: a slot past the class's count used; an
// empty slot missed; a magic staff not found as the quarterstaff slot.
TEST(MasterySlotHelpers) {
  const std::vector<int> chosen = {1, -1, 22, -1, -1, -1};
  CHECK_EQ(FreeMasterySlot(chosen, 3), 1);
  CHECK_EQ(FreeMasterySlot({1, 22, 11, -1, -1, -1}, 3), -1);
  CHECK_EQ(FreeMasterySlot({1, 22, 11, -1, -1, -1}, 4), 3);
  CHECK_EQ(MasterySlotOf(chosen, 3, 22), 2);
  CHECK_EQ(MasterySlotOf(chosen, 2, 22), -1);
  CHECK_EQ(MasterySlotOf({50, -1}, 2, 45), 0);
}

// D3 swap direction (owner report 2026-10-18: "!mastery dart for sling" meant
// trading the dart for the sling). Breaks caught: one word order only; a swap
// accepted when both or neither weapon is in a slot.
TEST(MasterySwapEitherOrder) {
  const std::vector<int> chosen = {8, 7, 31, -1, -1, -1};  // longbow, light crossbow, dart
  MasterySwap swap = {-1, -1};
  CHECK_EQ(ResolveMasterySwap(chosen, 3, 31, 61, &swap), true);  // dart for sling
  CHECK_EQ(swap.give_up, 31);
  CHECK_EQ(swap.take, 61);
  CHECK_EQ(ResolveMasterySwap(chosen, 3, 61, 31, &swap), true);  // sling for dart
  CHECK_EQ(swap.give_up, 31);
  CHECK_EQ(swap.take, 61);
  CHECK_EQ(ResolveMasterySwap(chosen, 3, 8, 7, &swap), false);   // both in slots
  CHECK_EQ(ResolveMasterySwap(chosen, 3, 61, 22, &swap), false);  // neither
}

// D4 Vex / Sap timing and what counts as a hit. Breaks caught: Vex lasting
// past the attacker's next round (or not reaching it); Sap outliving the
// round it was dealt in; a parry or concealment miss counted as a hit.
TEST(VexSapTiming) {
  CHECK_EQ(VexActive(5, 5), true);   // same round
  CHECK_EQ(VexActive(6, 5), true);   // next round
  CHECK_EQ(VexActive(7, 5), false);  // after the next round
  CHECK_EQ(SapActive(5, 5), true);
  CHECK_EQ(SapActive(6, 5), false);
  for (int hit : {1, 3, 7, 10}) CHECK_EQ(MasteryAttackHit(hit), true);
  for (int miss : {0, 2, 4, 8, 9}) CHECK_EQ(MasteryAttackHit(miss), false);
}

// D4 Graze / Topple numbers. Breaks caught: Graze on a hit or not on a parry;
// Graze dealing a negative or zero modifier; Topple DC missing a term; finesse
// used without the feat or when Dex is lower; ranged attacks using Str.
TEST(GrazeToppleNumbers) {
  for (int miss : {2, 4, 8, 9}) CHECK_EQ(MasteryAttackMiss(miss), true);
  for (int hit : {1, 3, 7, 10, 0}) CHECK_EQ(MasteryAttackMiss(hit), false);
  CHECK_EQ(GrazeDamage(3), 3);
  CHECK_EQ(GrazeDamage(0), 0);
  CHECK_EQ(GrazeDamage(-1), 0);
  CHECK_EQ(ToppleDc(3, 2), 13);
  CHECK_EQ(AttackAbilityModifier(true, false, false, 3, 1), 1);
  CHECK_EQ(AttackAbilityModifier(false, false, true, 1, 4), 1);
  CHECK_EQ(AttackAbilityModifier(false, true, true, 1, 4), 4);
  CHECK_EQ(AttackAbilityModifier(false, true, true, 4, 1), 4);
  CHECK_EQ(AttackAbilityModifier(false, true, false, 1, 4), 1);
  CHECK_EQ(IsFinesseNwnBaseItem(51), true);
  CHECK_EQ(IsFinesseNwnBaseItem(13), false);
}

// The main-hand Strength term the D4 Cleave attack takes back out. Breaks
// caught: rounding 1.5 x Str up (Str +3 two-handed is 4, not 5); the
// two-handed bonus applied to a negative modifier or a one-handed weapon.
TEST(NwnMainHandStrength) {
  CHECK_EQ(NwnMainHandStrengthDamage(3, true), 4);
  CHECK_EQ(NwnMainHandStrengthDamage(4, true), 6);
  CHECK_EQ(NwnMainHandStrengthDamage(3, false), 3);
  CHECK_EQ(NwnMainHandStrengthDamage(-1, true), -1);
  CHECK_EQ(NwnMainHandStrengthDamage(0, true), 0);
}

// Breaks caught: a Heavy weapon missed (no Disadvantage below 13), a weapon
// NWN calls Large but 2024 doesn't call Heavy (spear, quarterstaff, trident)
// made Heavy, the ranged ones (longbow, heavy crossbow) left out.
TEST(HeavyWeaponsAre2024) {
  CHECK_EQ(IsHeavyWeapon2024(13), true);   // greatsword
  CHECK_EQ(IsHeavyWeapon2024(18), true);   // greataxe
  CHECK_EQ(IsHeavyWeapon2024(8), true);    // longbow
  CHECK_EQ(IsHeavyWeapon2024(6), true);    // heavy crossbow
  CHECK_EQ(IsHeavyWeapon2024(58), false);  // spear
  CHECK_EQ(IsHeavyWeapon2024(50), false);  // quarterstaff
  CHECK_EQ(IsHeavyWeapon2024(95), false);  // trident
  CHECK_EQ(IsHeavyWeapon2024(1), false);   // longsword
  CHECK_EQ(IsHeavyWeapon2024(11), false);  // shortbow
}

// Breaks caught: a Dexterity rogue's rapier damage left on Strength, the
// adjustment applied when Strength is better, the off hand given Dexterity
// without the style (2024: no ability modifier there unless negative), a
// negative Strength off-hand penalty kept when Dexterity is chosen.
TEST(FinesseDamageUsesTheBetterAbility) {
  CHECK_EQ(FinesseDamageAdjustment(0, 3, false, false), 3);    // main hand: Dex +3 instead of Str +0
  CHECK_EQ(FinesseDamageAdjustment(-1, 3, false, false), 4);   // instead of Str -1
  CHECK_EQ(FinesseDamageAdjustment(3, 1, false, false), 0);    // Strength is better
  CHECK_EQ(FinesseDamageAdjustment(0, 3, true, false), 0);     // off hand, no style: nothing either way
  CHECK_EQ(FinesseDamageAdjustment(-1, 3, true, false), 1);    // off hand: the -1 penalty goes (Dex chosen)
  CHECK_EQ(FinesseDamageAdjustment(1, 3, true, true), 2);      // off hand with the style: Dex +3 instead of +1
  CHECK_EQ(FinesseDamageAdjustment(-2, -1, true, false), 1);   // both negative: the lesser penalty
}
