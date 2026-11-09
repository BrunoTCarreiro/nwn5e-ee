#include "check.h"
#include "rules/action_economy.h"
#include "rules/cantrips.h"
#include "rules/concentration.h"
#include "rules/material_components.h"
#include "rules/spells2024.h"

#include <string>
#include <vector>

using namespace rules;

// #39 Part 2a build 1a: SRD 5.2.1's level 1 damage and healing spells (pp. 114-177).
TEST(Spells2024Table) {
  CHECK_EQ(static_cast<int>(Spells2024().size()), 13);
  for (size_t i = 1; i < Spells2024().size(); ++i) CHECK_EQ(Spells2024()[i - 1].row < Spells2024()[i].row, true);
  CHECK_EQ(Spell2024Of(1) == nullptr, true);
  const Spell2024* burning = Spell2024Of(kBurningHands);
  CHECK_EQ(burning != nullptr, true);
  CHECK_EQ(burning->dice, 3);
  CHECK_EQ(burning->die, 6);
  CHECK_EQ(burning->damage_type, kDamageTypeFire);
  CHECK_EQ(burning->save == Ability::kDexterity, true);
  CHECK_EQ(burning->half_on_save, true);
  CHECK_EQ(burning->shape == SpellShape2024::kCone, true);
  CHECK_EQ(burning->feet, 15);
  const Spell2024* missile = Spell2024Of(kMagicMissile);
  CHECK_EQ(missile->effect == SpellEffect2024::kDarts, true);
  CHECK_EQ(missile->die, 4);
  CHECK_EQ(missile->flat, 1);
  CHECK_EQ(missile->damage_type, kDamageTypeForce);
  const Spell2024* bolt = Spell2024Of(kGuidingBolt);
  CHECK_EQ(bolt->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(bolt->melee, false);
  CHECK_EQ(bolt->dice, 4);
  CHECK_EQ(bolt->damage_type, kDamageTypeRadiant);
  const Spell2024* inflict = Spell2024Of(kInflictWounds);
  CHECK_EQ(inflict->melee, true);
  CHECK_EQ(inflict->save == Ability::kConstitution, true);
  CHECK_EQ(inflict->half_on_save, true);
  CHECK_EQ(inflict->die, 10);
  CHECK_EQ(inflict->damage_type, kDamageTypeNecrotic);
  const Spell2024* ray = Spell2024Of(kRayOfSickness);
  CHECK_EQ(ray->dice, 2);
  CHECK_EQ(ray->die, 8);
  CHECK_EQ(ray->damage_type, kDamageTypePoison);
  const Spell2024* cure = Spell2024Of(kCureWounds);
  CHECK_EQ(cure->effect == SpellEffect2024::kHealing, true);
  CHECK_EQ(cure->melee, true);
  CHECK_EQ(cure->upcast_dice, 2);
  const Spell2024* life = Spell2024Of(kFalseLife);
  CHECK_EQ(life->effect == SpellEffect2024::kTemporaryHitPoints, true);
  CHECK_EQ(life->dice, 2);
  CHECK_EQ(life->die, 4);
  const Spell2024* whispers = Spell2024Of(kDissonantWhispers);
  CHECK_EQ(whispers->save == Ability::kWisdom, true);
  CHECK_EQ(whispers->half_on_save, true);
  CHECK_EQ(whispers->damage_type, kDamageTypePsychic);
  const Spell2024* wave = Spell2024Of(kThunderwave);
  CHECK_EQ(wave->save == Ability::kConstitution, true);
  CHECK_EQ(wave->shape == SpellShape2024::kCubeFromCaster, true);
  CHECK_EQ(wave->feet, 15);
  CHECK_EQ(wave->damage_type, kDamageTypeThunder);
  const Spell2024* knife = Spell2024Of(kIceKnife);
  CHECK_EQ(knife->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(knife->dice, 1);
  CHECK_EQ(knife->die, 10);
  CHECK_EQ(knife->damage_type, 2u);
  CHECK_EQ(knife->upcast_dice, 0);
  for (const Spell2024& spell : Spells2024()) {
    CHECK_EQ(spell.level, 1);
    CHECK_EQ(std::string(spell.script).rfind("n5_s_", 0) == 0, true);
  }
}

TEST(Spells2024Upcasting) {
  CHECK_EQ(CastSlotLevel(1, 0), 1);
  CHECK_EQ(CastSlotLevel(1, 1), 1);
  CHECK_EQ(CastSlotLevel(1, 3), 3);
  CHECK_EQ(CastSlotLevel(2, 1), 2);
  CHECK_EQ(CastSlotLevel(1, 12), 9);
  CHECK_EQ(DiceAtSlot(3, 1, 1, 1), 3);
  CHECK_EQ(DiceAtSlot(3, 1, 1, 3), 5);
  CHECK_EQ(DiceAtSlot(2, 2, 1, 2), 4);
  CHECK_EQ(DiceAtSlot(2, 1, 2, 1), 2);
  CHECK_EQ(SpellDiceAt(*Spell2024Of(kBurningHands), 2), 4);
  CHECK_EQ(SpellDiceAt(*Spell2024Of(kCureWounds), 3), 6);
  CHECK_EQ(SpellDiceAt(*Spell2024Of(kGuidingBolt), 9), 12);
  CHECK_EQ(SpellDiceAt(*Spell2024Of(kIceKnife), 5), 1);  // the shard's d10 never grows
  CHECK_EQ(CriticalDice(4, false), 4);
  CHECK_EQ(CriticalDice(4, true), 8);
  CHECK_EQ(MagicMissileDarts(1), 3);
  CHECK_EQ(MagicMissileDarts(2), 4);
  CHECK_EQ(MagicMissileDarts(9), 11);
  CHECK_EQ(FalseLifeTemporaryHitPoints(5, 1), 9);
  CHECK_EQ(FalseLifeTemporaryHitPoints(5, 2), 14);
  CHECK_EQ(FalseLifeTemporaryHitPoints(2, 4), 21);
  CHECK_EQ(IceKnifeExplosionDice(1), 2);
  CHECK_EQ(IceKnifeExplosionDice(3), 4);
}

TEST(Spells2024Saves) {
  CHECK_EQ(DamageAfterSave(11, false, true), 11);
  CHECK_EQ(DamageAfterSave(11, true, true), 5);
  CHECK_EQ(DamageAfterSave(11, true, false), 0);
  CHECK_EQ(DamageAfterSave(1, true, true), 0);
  CHECK_EQ(DamageAfterSave(0, false, true), 0);
  CHECK_EQ(TemporaryHitPointsReplace(0, 7), true);
  CHECK_EQ(TemporaryHitPointsReplace(7, 7), false);
  CHECK_EQ(TemporaryHitPointsReplace(9, 7), false);
}

TEST(Spells2024Areas) {
  const float fifteen = FeetToMetres(15);
  CHECK_EQ(fifteen > 4.57f && fifteen < 4.58f, true);
  // A 15-foot Cone: at 4 m ahead, 2 m to the side is in; 2.5 m isn't (without slack).
  CHECK_EQ(InCone(4.0f, 2.0f, fifteen, 0.0f), true);
  CHECK_EQ(InCone(4.0f, -2.0f, fifteen, 0.0f), true);
  CHECK_EQ(InCone(4.0f, 2.5f, fifteen, 0.0f), false);
  CHECK_EQ(InCone(4.0f, 2.5f, fifteen, 0.5f), true);
  CHECK_EQ(InCone(5.0f, 0.0f, fifteen, 0.0f), false);
  CHECK_EQ(InCone(5.0f, 0.0f, fifteen, 0.5f), true);
  CHECK_EQ(InCone(-1.0f, 0.0f, fifteen, 0.5f), false);  // behind the caster
  CHECK_EQ(InCone(1.0f, 1.0f, fifteen, 0.0f), false);
  // A 15-foot Cube from the caster: 2.28 m to each side, all the way.
  CHECK_EQ(InCubeFromCaster(1.0f, 2.2f, fifteen, 0.0f), true);
  CHECK_EQ(InCubeFromCaster(1.0f, -2.4f, fifteen, 0.0f), false);
  CHECK_EQ(InCubeFromCaster(1.0f, -2.4f, fifteen, 0.5f), true);
  CHECK_EQ(InCubeFromCaster(4.5f, 0.0f, fifteen, 0.0f), true);
  CHECK_EQ(InCubeFromCaster(4.7f, 0.0f, fifteen, 0.0f), false);
  CHECK_EQ(InCubeFromCaster(-0.6f, 0.0f, fifteen, 0.5f), false);
}

// Build 1b: Healing Word, Chromatic Orb, Heroism.
TEST(Spells2024Build1b) {
  const Spell2024* word = Spell2024Of(kHealingWord);
  CHECK_EQ(word != nullptr, true);
  CHECK_EQ(word->effect == SpellEffect2024::kHealing, true);
  CHECK_EQ(word->bonus_action, true);
  CHECK_EQ(word->melee, false);
  CHECK_EQ(word->dice, 2);
  CHECK_EQ(word->die, 4);
  CHECK_EQ(SpellDiceAt(*word, 3), 6);
  CHECK_EQ(Spell2024Of(kCureWounds)->bonus_action, false);
  CHECK_EQ(Spell2024Of(kHealingWordBonus) == nullptr, true);  // its feat's row: cast as Healing Word
  CHECK_EQ(IsBonusActionSpellFeat(kFeatHealingWord), true);
  CHECK_EQ(TakesBonusAction(kFeatHealingWord), true);
  CHECK_EQ(std::string(BonusActionName(kFeatHealingWord)) == "Healing Word", true);
  CHECK_EQ(BonusActionFeatOfSpell(kHealingWord), -1);  // not the cantrips' way: its slot is spent
  const Spell2024* orb = Spell2024Of(kChromaticOrb);
  CHECK_EQ(orb->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(orb->dice, 3);
  CHECK_EQ(orb->die, 8);
  CHECK_EQ(SpellDiceAt(*orb, 2), 4);
  const ChoiceCantrip* menu = ChoiceCantripOf(kChromaticOrbFire);
  CHECK_EQ(menu != nullptr && menu->master == kChromaticOrb, true);
  CHECK_EQ(menu->options.size(), 6u);
  CHECK_EQ(ChoiceOptionOf(kChromaticOrbAcid)->value, kDamageTypeAcid);
  CHECK_EQ(ChoiceOptionOf(kChromaticOrbCold)->value, kDamageTypeCold);
  CHECK_EQ(ChoiceOptionOf(kChromaticOrbLightning)->value, kDamageTypeLightning);
  CHECK_EQ(ChoiceOptionOf(kChromaticOrbPoison)->value, kDamageTypePoison);
  CHECK_EQ(ChoiceOptionOf(kChromaticOrbThunder)->value, kDamageTypeThunder);
  CHECK_EQ(ChoiceOptionOf(kChromaticOrb) == nullptr, true);
  CHECK_EQ(CostlyComponent(kChromaticOrb).needed, 50);
  CHECK_EQ(CostlyComponent(kChromaticOrb).consumed, 0);
  CHECK_EQ(CostlyComponent(kChromaticOrbThunder).needed, 50);
  CHECK_EQ(HasMatchingDice({1, 2, 3}), false);
  CHECK_EQ(HasMatchingDice({4, 2, 4}), true);
  CHECK_EQ(HasMatchingDice({8, 8}), true);
  CHECK_EQ(HasMatchingDice({5}), false);
  CHECK_EQ(HasMatchingDice({}), false);
  CHECK_EQ(HasMatchingDice({1, 2, 3, 4, 5, 6, 7, 8}), false);
  CHECK_EQ(ChromaticOrbLeaps(1), 1);
  CHECK_EQ(ChromaticOrbLeaps(3), 3);
  const Spell2024* heroism = Spell2024Of(kHeroism);
  CHECK_EQ(heroism->effect == SpellEffect2024::kBuff, true);
  CHECK_EQ(heroism->melee, true);
  CHECK_EQ(IsConcentrationSpell(kHeroism), true);
  CHECK_EQ(IsConcentrationSpell(kHealingWord), false);
  CHECK_EQ(HeroismTargets(1), 1);
  CHECK_EQ(HeroismTargets(3), 3);
  CHECK_EQ(HeroismTemporaryHitPoints(3), 3);
  CHECK_EQ(HeroismTemporaryHitPoints(-1), 0);
  // One spell with a slot per turn: the same round, under 6 s.
  CHECK_EQ(SameTurn(5, 5, 3000), true);
  CHECK_EQ(SameTurn(5, 5, 5999), true);
  CHECK_EQ(SameTurn(5, 5, 6000), false);
  CHECK_EQ(SameTurn(5, 6, 1000), false);
  CHECK_EQ(SameTurn(5, 5, -1), false);
}
