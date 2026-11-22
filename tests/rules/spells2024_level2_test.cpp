#include "check.h"
#include "rules/action_economy.h"
#include "rules/cantrips.h"
#include "rules/concentration.h"
#include "rules/spells2024.h"

#include <string>
#include <vector>

using namespace rules;

// #39 Part 2, level 2, build 1a (owner, 2026-11-18: "A for all, except Q3"): SRD 5.2.1's Acid Arrow, Shatter, Mind
// Spike, Shining Smite and Prayer of Healing - their rows, numbers and upcasting.
TEST(Level2Build1aTable) {
  const Spell2024* acid = Spell2024Of(kAcidArrow);
  CHECK_EQ(acid != nullptr, true);
  CHECK_EQ(acid->level, 2);
  CHECK_EQ(acid->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(acid->dice, 4);
  CHECK_EQ(acid->die, 4);
  CHECK_EQ(acid->damage_type, kDamageTypeAcid);
  CHECK_EQ(SpellDiceAt(*acid, 2), 4);
  CHECK_EQ(SpellDiceAt(*acid, 3), 5);  // 1d4 more a slot level above 2
  const Spell2024* shatter = Spell2024Of(kShatter);
  CHECK_EQ(shatter != nullptr, true);
  CHECK_EQ(shatter->level, 2);
  CHECK_EQ(shatter->effect == SpellEffect2024::kSave, true);
  CHECK_EQ(shatter->save == Ability::kConstitution, true);
  CHECK_EQ(shatter->half_on_save, true);
  CHECK_EQ(shatter->shape == SpellShape2024::kSphere, true);
  CHECK_EQ(shatter->feet, 10);
  CHECK_EQ(shatter->dice, 3);
  CHECK_EQ(shatter->die, 8);
  CHECK_EQ(shatter->damage_type, kDamageTypeThunder);
  CHECK_EQ(SpellDiceAt(*shatter, 4), 5);
  const Spell2024* spike = Spell2024Of(kMindSpike);
  CHECK_EQ(spike != nullptr, true);
  CHECK_EQ(spike->level, 2);
  CHECK_EQ(spike->save == Ability::kWisdom, true);
  CHECK_EQ(spike->half_on_save, true);
  CHECK_EQ(spike->shape == SpellShape2024::kOne, true);
  CHECK_EQ(spike->damage_type, kDamageTypePsychic);
  CHECK_EQ(SpellDiceAt(*spike, 2), 3);
  CHECK_EQ(SpellDiceAt(*spike, 3), 4);
  const Spell2024* shining = Spell2024Of(kShiningSmite);
  CHECK_EQ(shining != nullptr, true);
  CHECK_EQ(shining->level, 2);
  CHECK_EQ(shining->effect == SpellEffect2024::kSmite, true);
  CHECK_EQ(shining->damage_type, kDamageTypeRadiant);
  const Spell2024* prayer = Spell2024Of(kPrayerOfHealing);
  CHECK_EQ(prayer != nullptr, true);
  CHECK_EQ(prayer->level, 2);
  CHECK_EQ(prayer->effect == SpellEffect2024::kHealing, true);
  CHECK_EQ(prayer->dice, 2);
  CHECK_EQ(prayer->die, 8);
  CHECK_EQ(SpellDiceAt(*prayer, 3), 3);
  CHECK_EQ(prayer->ritual, false);
}

TEST(Level2Build1aRules) {
  // Acid Arrow: a miss splashes for half the initial roll; the acid later 2d4, 1d4 more a level above 2.
  CHECK_EQ(AcidArrowMissDamage(9), 4);
  CHECK_EQ(AcidArrowMissDamage(8), 4);
  CHECK_EQ(AcidArrowMissDamage(1), 0);
  CHECK_EQ(AcidArrowMissDamage(0), 0);
  CHECK_EQ(AcidArrowLaterDice(2), 2);
  CHECK_EQ(AcidArrowLaterDice(3), 3);
  CHECK_EQ(AcidArrowLaterDice(5), 5);
  CHECK_EQ(WizardOnlySpell(kAcidArrow), true);  // the Wizard's alone (2024)
  CHECK_EQ(WizardOnlySpell(kShatter), false);    // the Sorcerer's too
  CHECK_EQ(WizardOnlySpell(kMindSpike), false);
  // Shatter: a Construct saves with Disadvantage; no one else does.
  CHECK_EQ(ShatterSaveDisadvantage(10), true);
  CHECK_EQ(ShatterSaveDisadvantage(24), false);  // an Undead
  CHECK_EQ(ShatterSaveDisadvantage(6), false);   // a Human
  // Shining Smite: build 3's smite - a melee weapon or an Unarmed Strike; 2d6, 1d6 more a level; its armed icon.
  CHECK_EQ(IsSmiteSpell(kShiningSmite), true);
  CHECK_EQ(SmiteRidesOn(kShiningSmite, false, false), true);
  CHECK_EQ(SmiteRidesOn(kShiningSmite, false, true), true);
  CHECK_EQ(SmiteRidesOn(kShiningSmite, true, false), false);
  CHECK_EQ(ShiningSmiteDice(2), 2);
  CHECK_EQ(ShiningSmiteDice(3), 3);
  CHECK_EQ(ShiningSmiteDice(5), 5);
  CHECK_EQ(SmiteArmedIconRow(kShiningSmite), 236);
  CHECK_EQ(IsSmiteArmedIcon(225), true);
  CHECK_EQ(IsSmiteArmedIcon(227), true);
  CHECK_EQ(IsSmiteArmedIcon(236), true);
  CHECK_EQ(IsSmiteArmedIcon(228), false);  // Detect Evil and Good's mark
  CHECK_EQ(IsSmiteArmedIcon(224), false);
  CHECK_EQ(IsSmiteArmedIcon(235), false);  // Silent Image's
  // Prayer of Healing: 10 minutes to cast (no Ritual).
  CHECK_EQ(CastingMinutes(kPrayerOfHealing), 10);
  CHECK_EQ(IsRitualSpell(kPrayerOfHealing), false);
  CHECK_EQ(CastingMinutes(kShatter), 0);
  // Concentration: Mind Spike (from the cast) and Shining Smite (from its hit); not the other three.
  CHECK_EQ(IsConcentrationSpell(kMindSpike), true);
  CHECK_EQ(IsConcentrationSpell(kShiningSmite), true);
  CHECK_EQ(IsConcentrationSpell(kAcidArrow) || IsConcentrationSpell(kShatter) ||
               IsConcentrationSpell(kPrayerOfHealing),
           false);
}

TEST(Level2PrayerOfHealingTargets) {
  // The caster first, then the nearest within 30 feet (9.14 m); five at most; none prayed for since its Long Rest.
  std::vector<PrayerCandidate> near = {{7, 8.0f, false}, {1, 0.0f, false}, {2, 3.0f, false}, {3, 1.0f, false},
                                       {4, 9.5f, false}, {5, 2.0f, true},  {6, 5.0f, false}, {8, 6.0f, false}};
  std::vector<uint32_t> picked = PrayerOfHealingTargetsOf(1, near);
  CHECK_EQ(picked.size(), 5u);
  CHECK_EQ(picked[0], 1u);  // the caster
  CHECK_EQ(picked[1], 3u);  // 1 m
  CHECK_EQ(picked[2], 2u);  // 3 m (5 at 2 m prayed already)
  CHECK_EQ(picked[3], 6u);  // 5 m
  CHECK_EQ(picked[4], 8u);  // 6 m; 7 at 8 m is the sixth - left out
  // Past 30 feet: out; the caster prayed for: not itself.
  std::vector<PrayerCandidate> far = {{1, 0.0f, true}, {2, 9.0f, false}, {3, 9.2f, false}};
  picked = PrayerOfHealingTargetsOf(1, far);
  CHECK_EQ(picked.size(), 1u);
  CHECK_EQ(picked[0], 2u);
  CHECK_EQ(PrayerOfHealingTargetsOf(1, {}).empty(), true);
  // The caster first even when another stands as close (a familiar on its shoulder) and is listed before it.
  std::vector<PrayerCandidate> tie = {{2, 0.0f, false}, {1, 0.0f, false}, {3, 9.0f, false}};
  picked = PrayerOfHealingTargetsOf(1, tie);
  CHECK_EQ(picked.size(), 3u);
  CHECK_EQ(picked[0], 1u);
  CHECK_EQ(picked[1], 2u);
  CHECK_EQ(picked[2], 3u);  // 9 m: inside the 30 feet (9.14 m)
  CHECK_EQ(kPrayerOfHealingTargets, 5);
  CHECK_EQ(kPrayerOfHealingFeet, 30);
}

// #39 Part 2, level 2, build 1b (owner, 2026-11-18: level 2's Q1 A; build 1's Q1, Q2, Q4 A): SRD 5.2.1's Spiritual
// Weapon, Flame Blade, Dragon's Breath and Heat Metal - their rows, numbers, upcasting and Bonus Actions.
TEST(Level2Build1bTable) {
  const Spell2024* weapon = Spell2024Of(kSpiritualWeapon);
  CHECK_EQ(weapon != nullptr, true);
  CHECK_EQ(weapon->level, 2);
  CHECK_EQ(weapon->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(weapon->melee, true);
  CHECK_EQ(weapon->bonus_action, true);
  CHECK_EQ(weapon->dice, 1);
  CHECK_EQ(weapon->die, 8);
  CHECK_EQ(weapon->damage_type, kDamageTypeForce);
  CHECK_EQ(SpellDiceAt(*weapon, 2), 1);
  CHECK_EQ(SpellDiceAt(*weapon, 3), 2);  // 1d8 more for every slot level above 2
  CHECK_EQ(SpellDiceAt(*weapon, 5), 4);
  const Spell2024* blade = Spell2024Of(kFlameBlade);
  CHECK_EQ(blade != nullptr, true);
  CHECK_EQ(blade->level, 2);
  CHECK_EQ(blade->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(blade->melee, true);
  CHECK_EQ(blade->bonus_action, true);
  CHECK_EQ(blade->dice, 3);
  CHECK_EQ(blade->die, 6);
  CHECK_EQ(blade->damage_type, kDamageTypeFire);
  CHECK_EQ(SpellDiceAt(*blade, 2), 3);
  CHECK_EQ(SpellDiceAt(*blade, 4), 5);
  const Spell2024* breath = Spell2024Of(kDragonsBreath);
  CHECK_EQ(breath != nullptr, true);
  CHECK_EQ(breath->level, 2);
  CHECK_EQ(breath->effect == SpellEffect2024::kSave, true);
  CHECK_EQ(breath->save == Ability::kDexterity, true);
  CHECK_EQ(breath->half_on_save, true);
  CHECK_EQ(breath->shape == SpellShape2024::kCone, true);
  CHECK_EQ(breath->feet, 15);
  CHECK_EQ(breath->bonus_action, true);
  CHECK_EQ(breath->dice, 3);
  CHECK_EQ(breath->die, 6);
  CHECK_EQ(SpellDiceAt(*breath, 3), 4);
  const Spell2024* heat = Spell2024Of(kHeatMetal);
  CHECK_EQ(heat != nullptr, true);
  CHECK_EQ(heat->level, 2);
  CHECK_EQ(heat->save == Ability::kConstitution, true);
  CHECK_EQ(heat->half_on_save, false);
  CHECK_EQ(heat->bonus_action, false);  // an Action; its later damage the Bonus Action
  CHECK_EQ(heat->dice, 2);
  CHECK_EQ(heat->die, 8);
  CHECK_EQ(heat->damage_type, kDamageTypeFire);
  CHECK_EQ(SpellDiceAt(*heat, 2), 2);
  CHECK_EQ(SpellDiceAt(*heat, 4), 4);
  // Their own actions' rows aren't spells of their own.
  CHECK_EQ(Spell2024Of(kSpiritualWeaponStrike) == nullptr && Spell2024Of(kFlameBladeEvoke) == nullptr &&
               Spell2024Of(kHeatMetalAgain) == nullptr && Spell2024Of(kDragonsBreathExhaleFirst) == nullptr,
           true);
  // The Bonus Action spells: their feats and ranges (Touch: 5 feet), Dragon's Breath's types its own.
  CHECK_EQ(BonusActionSpellOf(kSpiritualWeapon)->feat, kFeatSpiritualWeapon);
  CHECK_EQ(BonusActionSpellOf(kSpiritualWeapon)->feet, 60);
  CHECK_EQ(BonusActionSpellOf(kSpiritualWeaponBonus)->spell, kSpiritualWeapon);
  CHECK_EQ(BonusActionSpellOf(kFlameBlade)->feet, 0);
  CHECK_EQ(BonusActionSpellOf(kFlameBladeBonus)->feat, kFeatFlameBlade);
  CHECK_EQ(BonusActionSpellOf(kDragonsBreath)->feet, 5);
  CHECK_EQ(BonusActionSpellOf(kDragonsBreathBonus)->feat, kFeatDragonsBreath);
  for (int row = kDragonsBreathAcid; row <= kDragonsBreathPoison; ++row) {
    CHECK_EQ(BonusActionSpellOf(row) == BonusActionSpellOf(kDragonsBreath), true);
    CHECK_EQ(IsBonusActionSpellOrder(row), true);
  }
  CHECK_EQ(BonusActionSpellOf(kHeatMetal) == nullptr, true);
  CHECK_EQ(BonusActionSpellOf(kChromaticOrbFire) == nullptr, true);  // an Action spell's menu
  CHECK_EQ(IsBonusActionSpellOrder(kSpiritualWeapon) && IsBonusActionSpellOrder(kHealingWord), true);
  CHECK_EQ(IsBonusActionSpellOrder(kDragonsBreathBonus) || IsBonusActionSpellOrder(kSpiritualWeaponBonus) ||
               IsBonusActionSpellOrder(kHeatMetal) || IsBonusActionSpellOrder(kChromaticOrbAcid),
           false);
  CHECK_EQ(TakesBonusAction(kFeatSpiritualWeaponStrike) && TakesBonusAction(kFeatFlameBladeEvoke) &&
               TakesBonusAction(kFeatHeatMetalAgain) && TakesBonusAction(kFeatDragonsBreath),
           true);
  CHECK_EQ(std::string(BonusActionName(kFeatSpiritualWeaponStrike)) == "Spiritual Weapon: Strike", true);
  CHECK_EQ(std::string(BonusActionName(kFeatFlameBladeEvoke)) == "Flame Blade: Evoke", true);
  CHECK_EQ(std::string(BonusActionName(kFeatHeatMetalAgain)) == "Heat Metal: Again", true);
  for (int feat = kFeatDragonsBreathExhaleFirst; feat <= kFeatDragonsBreathExhaleLast; ++feat) {
    CHECK_EQ(TakesBonusAction(feat), false);  // the exhale is a Magic action
  }
  // Dragon's Breath's menu: its five types, SRD 5.2.1's order.
  const ChoiceCantrip* menu = ChoiceCantripOf(kDragonsBreathLightning);
  CHECK_EQ(menu != nullptr && menu->master == kDragonsBreath, true);
  CHECK_EQ(menu->options.size(), 5u);
  CHECK_EQ(ChoiceOptionOf(kDragonsBreathCold)->value, kDamageTypeCold);
  CHECK_EQ(ChoiceOptionOf(kDragonsBreath) == nullptr, true);
  // Concentration: the casts (and their types and Bonus Action rows), not the actions they give.
  for (int row : {kSpiritualWeapon, kSpiritualWeaponBonus, kFlameBlade, kFlameBladeBonus, kDragonsBreath,
                  kDragonsBreathAcid, kDragonsBreathPoison, kDragonsBreathBonus, kHeatMetal}) {
    CHECK_EQ(IsConcentrationSpell(row), true);
  }
  for (int row : {kSpiritualWeaponStrike, kFlameBladeEvoke, kDragonsBreathExhaleFirst, kDragonsBreathExhaleLast,
                  kHeatMetalAgain}) {
    CHECK_EQ(IsConcentrationSpell(row), false);
  }
}

TEST(Level2Build1bRules) {
  // Spiritual Weapon: the force moves 20 feet and strikes within 5 - 25 feet (7.62 m) from where it was, plus slack.
  CHECK_EQ(SpiritualWeaponReaches(0.0f, 0.0f), true);
  CHECK_EQ(SpiritualWeaponReaches(7.6f, 0.0f), true);
  CHECK_EQ(SpiritualWeaponReaches(7.7f, 0.0f), false);
  CHECK_EQ(SpiritualWeaponReaches(8.0f, 0.5f), true);
  CHECK_EQ(SpiritualWeaponReaches(8.2f, 0.5f), false);
  // Dragon's Breath's type: a type's row, its exhale's; none chosen (the master, its Bonus Action's row) Fire.
  CHECK_EQ(DragonsBreathType(kDragonsBreathAcid), kDamageTypeAcid);
  CHECK_EQ(DragonsBreathType(kDragonsBreathCold), kDamageTypeCold);
  CHECK_EQ(DragonsBreathType(kDragonsBreathFire), kDamageTypeFire);
  CHECK_EQ(DragonsBreathType(kDragonsBreathLightning), kDamageTypeLightning);
  CHECK_EQ(DragonsBreathType(kDragonsBreathPoison), kDamageTypePoison);
  CHECK_EQ(DragonsBreathType(kDragonsBreath), kDamageTypeFire);
  CHECK_EQ(DragonsBreathType(kDragonsBreathBonus), kDamageTypeFire);
  CHECK_EQ(DragonsBreathType(kDragonsBreathExhaleFirst + 1), kDamageTypeCold);
  CHECK_EQ(DragonsBreathType(kDragonsBreathExhaleLast), kDamageTypePoison);
  CHECK_EQ(DragonsBreathType(kHeatMetal), 0u);
  CHECK_EQ(DragonsBreathType(kChromaticOrbAcid), 0u);
  CHECK_EQ(DragonsBreathExhaleRow(kDamageTypeAcid), 1269);
  CHECK_EQ(DragonsBreathExhaleRow(kDamageTypeLightning), 1272);
  CHECK_EQ(DragonsBreathExhaleRow(kDamageTypePoison), 1273);
  CHECK_EQ(DragonsBreathExhaleRow(kDamageTypeThunder), -1);
  CHECK_EQ(DragonsBreathExhaleFeat(kDamageTypeAcid), 1572);
  CHECK_EQ(DragonsBreathExhaleFeat(kDamageTypeFire), 1574);
  CHECK_EQ(DragonsBreathExhaleFeat(kDamageTypePoison), 1576);
  CHECK_EQ(DragonsBreathExhaleFeat(kDamageTypeForce), -1);
  CHECK_EQ(IsDragonsBreathExhaleRow(1268) || IsDragonsBreathExhaleRow(1274), false);
  CHECK_EQ(IsDragonsBreathExhaleRow(1269) && IsDragonsBreathExhaleRow(1273), true);
  CHECK_EQ(IsDragonsBreathExhaleFeat(1571) || IsDragonsBreathExhaleFeat(1577), false);
  CHECK_EQ(IsDragonsBreathExhaleFeat(1572) && IsDragonsBreathExhaleFeat(1576), true);
  CHECK_EQ(IsDragonsBreathRow(kDragonsBreath) && IsDragonsBreathRow(kDragonsBreathAcid) &&
               IsDragonsBreathRow(kDragonsBreathPoison) && IsDragonsBreathRow(kDragonsBreathBonus),
           true);
  CHECK_EQ(IsDragonsBreathRow(kDragonsBreathExhaleFirst) || IsDragonsBreathRow(kFlameBladeEvoke) ||
               IsDragonsBreathRow(kHeatMetal),
           false);
  // Heat Metal: metal weapons (baseitems.2da), not wood, bows, slings, darts or shields.
  for (int metal : {0, 1, 2, 5, 9, 13, 22, 47, 53, 58, 60, 78, 95}) CHECK_EQ(MetalWeaponBaseItem(metal), true);
  for (int other : {-1, 6, 8, 11, 14, 28, 31, 50, 56, 61, 108}) CHECK_EQ(MetalWeaponBaseItem(other), false);
  // Medium or Heavy metal armour: tiers 4-8 (Scale Mail to Plate); not light armour, not none.
  CHECK_EQ(MetalArmourTier(0) || MetalArmourTier(1) || MetalArmourTier(3) || MetalArmourTier(9), false);
  CHECK_EQ(MetalArmourTier(4) && MetalArmourTier(5) && MetalArmourTier(8), true);
  // Its weapon else its armour (Q4 A): the main hand's first.
  CHECK_EQ(HeatMetalObject(1, 22, 8) == HeatedObject::kRightHand, true);
  CHECK_EQ(HeatMetalObject(50, 22, 8) == HeatedObject::kLeftHand, true);
  CHECK_EQ(HeatMetalObject(50, 14, 5) == HeatedObject::kArmour, true);  // a shield isn't a weapon
  CHECK_EQ(HeatMetalObject(-1, -1, 4) == HeatedObject::kArmour, true);
  CHECK_EQ(HeatMetalObject(50, -1, 3) == HeatedObject::kNone, true);
  CHECK_EQ(HeatMetalObject(-1, -1, 0) == HeatedObject::kNone, true);
  // A failed save drops a weapon; keeping hold (a success, or armour) - Disadvantage; no damage, nothing.
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kRightHand, 9, true) == HeatOutcome::kDrops, true);
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kLeftHand, 9, true) == HeatOutcome::kDrops, true);
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kRightHand, 9, false) == HeatOutcome::kDisadvantage, true);
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kArmour, 9, true) == HeatOutcome::kDisadvantage, true);
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kArmour, 9, false) == HeatOutcome::kDisadvantage, true);
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kRightHand, 0, true) == HeatOutcome::kNothing, true);
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kNone, 9, true) == HeatOutcome::kNothing, true);
  // Heat Metal: Again - a later turn, held or worn, within 60 feet (18.29 m) plus slack.
  CHECK_EQ(HeatMetalAgainNow(true, true, 1.0f, 0.0f) == HeatAgain::kCastThisTurn, true);
  CHECK_EQ(HeatMetalAgainNow(false, false, 1.0f, 0.0f) == HeatAgain::kNotHeld, true);
  CHECK_EQ(HeatMetalAgainNow(false, true, 18.2f, 0.0f) == HeatAgain::kOk, true);
  CHECK_EQ(HeatMetalAgainNow(false, true, 18.4f, 0.0f) == HeatAgain::kOutOfRange, true);
  CHECK_EQ(HeatMetalAgainNow(false, true, 18.7f, 0.5f) == HeatAgain::kOk, true);
  CHECK_EQ(HeatMetalAgainNow(false, true, 9999.0f, 0.5f) == HeatAgain::kOutOfRange, true);
  // Flame Blade is the Sorcerer's of NWN's shared Wiz_Sorc column, not the Wizard's; Dragon's Breath both.
  CHECK_EQ(SorcererOnlySpell(kFlameBlade) && SorcererOnlySpell(kSorcerousBurst), true);
  CHECK_EQ(SorcererOnlySpell(kDragonsBreath) || SorcererOnlySpell(kSpiritualWeapon) || SorcererOnlySpell(kShatter),
           false);
  CHECK_EQ(SorcererOnlyRows().size(), 2u);
  CHECK_EQ(WizardOnlySpell(kFlameBlade) || WizardOnlySpell(kDragonsBreath), false);
  // The spells' own actions beside them in the radial: Hunter's Mark's Move and build 1b's three.
  CHECK_EQ(SpellActionLeafOf(kHuntersMark)->feat, kFeatHuntersMarkMove);
  CHECK_EQ(std::string(SpellActionLeafOf(kHuntersMark)->label) == "Hunter's Mark: Move", true);
  CHECK_EQ(SpellActionLeafOf(kSpiritualWeapon)->feat, kFeatSpiritualWeaponStrike);
  CHECK_EQ(std::string(SpellActionLeafOf(kSpiritualWeapon)->spell_name) == "Spiritual Weapon", true);
  CHECK_EQ(SpellActionLeafOf(kFlameBlade)->feat, kFeatFlameBladeEvoke);
  // Heat Metal: Again sits in its menu instead (2026-11-22's menu): a feat row, no slot levels.
  CHECK_EQ(SpellActionLeafOf(kHeatMetal) == nullptr, true);
  CHECK_EQ(ChoiceOptionFeat(kHeatMetalAgain), kFeatHeatMetalAgain);
  CHECK_EQ(IsNoSlotOption(kHeatMetalAgain), true);
  CHECK_EQ(SpellActionLeafOf(kDragonsBreath) == nullptr && SpellActionLeafOf(kBless) == nullptr, true);
}

// Heat Metal's rework (owner, 2026-11-22: "A with the pickup step plus intelligence flavour, and A for the menu"):
// its menu's object, what drops, and the pickup step.
TEST(Level2HeatMetalMenu) {
  CHECK_EQ(HeatMetalChoiceOf(kHeatMetalWeapon) == HeatChoice::kWeapon, true);
  CHECK_EQ(HeatMetalChoiceOf(kHeatMetalShield) == HeatChoice::kShield, true);
  CHECK_EQ(HeatMetalChoiceOf(kHeatMetalArmour) == HeatChoice::kArmour, true);
  CHECK_EQ(HeatMetalChoiceOf(kHeatMetal) == HeatChoice::kAny, true);
  CHECK_EQ(HeatMetalChoiceOf(kHeatMetalAgain) == HeatChoice::kAny, true);
  // The menu: Weapon, Shield, Armour (their slot levels) and Again (a feat, no levels).
  const ChoiceCantrip* menu = ChoiceCantripOf(kHeatMetalShield);
  CHECK_EQ(menu != nullptr && menu->master == kHeatMetal, true);
  CHECK_EQ(menu->options.size(), 4u);
  CHECK_EQ(ChoiceCantripOf(kHeatMetalAgain) == menu, true);
  CHECK_EQ(IsNoSlotOption(kHeatMetalWeapon) || IsNoSlotOption(kHeatMetalArmour), false);
  CHECK_EQ(IsConcentrationSpell(kHeatMetalWeapon) && IsConcentrationSpell(kHeatMetalShield) &&
               IsConcentrationSpell(kHeatMetalArmour),
           true);
  CHECK_EQ(BonusActionSpellOf(kHeatMetalWeapon) == nullptr, true);
  // Shields: NWN's three.
  CHECK_EQ(MetalShieldBaseItem(14) && MetalShieldBaseItem(56) && MetalShieldBaseItem(57), true);
  CHECK_EQ(MetalShieldBaseItem(1) || MetalShieldBaseItem(-1) || MetalShieldBaseItem(15), false);  // a torch
  // Weapon: the main hand's metal weapon, else the off hand's; never a shield or armour.
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kWeapon, 1, 14, 8) == HeatedObject::kRightHand, true);
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kWeapon, 50, 22, 8) == HeatedObject::kLeftHand, true);
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kWeapon, 50, 14, 8) == HeatedObject::kNone, true);
  // Shield: a shield in the off hand, whatever else it has.
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kShield, 1, 56, 8) == HeatedObject::kShield, true);
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kShield, 1, 22, 8) == HeatedObject::kNone, true);
  // Armour: Medium or Heavy metal armour, whatever it holds.
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kArmour, 1, 56, 5) == HeatedObject::kArmour, true);
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kArmour, 1, 56, 3) == HeatedObject::kNone, true);
  // No choice (an NPC's cast): the weapon, else the armour, else the shield.
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kAny, 1, 56, 8) == HeatedObject::kRightHand, true);
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kAny, 50, 56, 8) == HeatedObject::kArmour, true);
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kAny, 50, 56, 2) == HeatedObject::kShield, true);
  CHECK_EQ(HeatMetalObjectFor(HeatChoice::kAny, 50, -1, 2) == HeatedObject::kNone, true);
  // A shield isn't dropped as it burns (2024: doffing it takes the Utilize action): Disadvantage, as armour.
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kShield, 9, true) == HeatOutcome::kDisadvantage, true);
  CHECK_EQ(HeatMetalOutcome(HeatedObject::kShield, 0, true) == HeatOutcome::kNothing, true);
  // The pickup step: another weapon first; else Intelligence 8+ leaves it while the spell lasts; else picks it up.
  CHECK_EQ(HeatMetalAfterDrop(true, 3, true) == AfterDrop::kDrawOther, true);
  CHECK_EQ(HeatMetalAfterDrop(true, 15, true) == AfterDrop::kDrawOther, true);
  CHECK_EQ(HeatMetalAfterDrop(false, 8, true) == AfterDrop::kLeave, true);
  CHECK_EQ(HeatMetalAfterDrop(false, 7, true) == AfterDrop::kPickUp, true);
  CHECK_EQ(HeatMetalAfterDrop(false, 15, false) == AfterDrop::kPickUp, true);  // the spell over: taken up again
  CHECK_EQ(HeatMetalAfterDrop(false, 6, false) == AfterDrop::kPickUp, true);
  // Weapons a hand holds: metal ones and the others; not shields, armour, ammunition or creature weapons.
  for (int weapon : {1, 22, 53, 6, 8, 28, 31, 50, 61, 108}) CHECK_EQ(WeaponBaseItem(weapon), true);
  for (int other : {-1, 14, 16, 20, 25, 27, 36, 49, 69, 160}) CHECK_EQ(WeaponBaseItem(other), false);
}

// Option C (owner, 2026-11-22: "if you can build it faithfuly, then yes, C"): Spiritual Weapon's force - a Strike's
// reach and move from where it is, where it stops to strike, its move's time, the cast's first strike.
TEST(Level2SpiritualWeaponForce) {
  // At a creature: 25 feet (7.62 m) from the force, plus the creature's slack.
  CHECK_EQ(SpiritualWeaponOrder(true, 7.6f, 0.0f) == ForceOrder::kStrike, true);
  CHECK_EQ(SpiritualWeaponOrder(true, 7.7f, 0.0f) == ForceOrder::kTooFar, true);
  CHECK_EQ(SpiritualWeaponOrder(true, 8.0f, 0.5f) == ForceOrder::kStrike, true);
  // At a spot: it only moves, 20 feet (6.10 m) at most.
  CHECK_EQ(SpiritualWeaponOrder(false, 0.0f, 0.0f) == ForceOrder::kMove, true);
  CHECK_EQ(SpiritualWeaponOrder(false, 6.0f, 0.0f) == ForceOrder::kMove, true);
  CHECK_EQ(SpiritualWeaponOrder(false, 6.2f, 0.0f) == ForceOrder::kTooFar, true);
  CHECK_EQ(SpiritualWeaponOrder(false, 6.5f, 0.5f) == ForceOrder::kMove, true);
  CHECK_EQ(SpiritualWeaponOrder(false, 6.7f, 0.5f) == ForceOrder::kTooFar, true);
  CHECK_EQ(SpiritualWeaponOrder(false, 7.0f, 0.5f) == ForceOrder::kTooFar, true);  // a creature's reach, not a spot's
  // Where it stops: 4 feet short of the creature on the line from it; no move when it's that close.
  float x = -1.0f, y = -1.0f;
  CHECK_EQ(SpiritualWeaponStrikePoint(0.0f, 0.0f, 10.0f, 0.0f, 1.2f, &x, &y) > 8.79f, true);
  CHECK_EQ(x > 8.79f && x < 8.81f && y == 0.0f, true);
  CHECK_EQ(SpiritualWeaponStrikePoint(0.0f, 0.0f, 3.0f, 4.0f, 1.0f, &x, &y), 4.0f);
  CHECK_EQ(x > 2.39f && x < 2.41f && y > 3.19f && y < 3.21f, true);
  CHECK_EQ(SpiritualWeaponStrikePoint(2.0f, 2.0f, 3.0f, 2.0f, 1.2f, &x, &y), 0.0f);
  CHECK_EQ(x == 2.0f && y == 2.0f, true);
  CHECK_EQ(kSpiritualWeaponStandOffFeet < kSpiritualWeaponReachFeet, true);
  // Its move's time: 4 m a second.
  CHECK_EQ(SpiritualWeaponMoveSeconds(8.0f), 2.0f);
  CHECK_EQ(SpiritualWeaponMoveSeconds(0.0f), 0.0f);
  CHECK_EQ(SpiritualWeaponMoveSeconds(-1.0f), 0.0f);
  // The cast: at a creature, its strike at once; at a spot or at yourself, none.
  CHECK_EQ(SpiritualWeaponStrikesAtOnce(true, false), true);
  CHECK_EQ(SpiritualWeaponStrikesAtOnce(true, true), false);
  CHECK_EQ(SpiritualWeaponStrikesAtOnce(false, false), false);
}

// 0.99.117's fix (owner, 2026-11-22, S2-24: "the quarterstaff doesn't come back, the PC keeps attacking unarmed"):
// Flame Blade's blade in the hand, on its way, or let go; the weapon it replaced back at the end.
TEST(Level2Build1bBlade) {
  CHECK_EQ(FlameBladeHand(true, false, 0, 6000) == BladeHand::kHeld, true);
  CHECK_EQ(FlameBladeHand(true, true, 90000, 6000) == BladeHand::kHeld, true);
  CHECK_EQ(FlameBladeHand(false, false, 5999, 6000) == BladeHand::kOnItsWay, true);  // its equip on its way
  CHECK_EQ(FlameBladeHand(false, false, 6000, 6000) == BladeHand::kLetGo, true);     // it never came
  CHECK_EQ(FlameBladeHand(false, true, 100, 6000) == BladeHand::kLetGo, true);       // held once, now not
  // Back: an old weapon, and the hand holds the blade or nothing; another weapon there stays.
  CHECK_EQ(FlameBladePutsBack(true, true, false), true);
  CHECK_EQ(FlameBladePutsBack(true, false, true), true);   // let go of: the hand empty
  CHECK_EQ(FlameBladePutsBack(true, false, false), false);  // the creature's own weapon now
  CHECK_EQ(FlameBladePutsBack(false, true, false), false);  // none to put back
  CHECK_EQ(FlameBladePutsBack(false, false, true), false);
}
