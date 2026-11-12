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
  CHECK_EQ(static_cast<int>(Spells2024().size()), 29);
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

// Build 2a: the buffs' numbers and the Bonus Action spells' table.
TEST(Spells2024Build2a) {
  CHECK_EQ(BonusActionSpells().size(), 6u);
  for (const BonusActionSpell& spell : BonusActionSpells()) {
    CHECK_EQ(BonusActionSpellOf(spell.spell) == &spell, true);
    CHECK_EQ(BonusActionSpellOf(spell.bonus_row) == &spell, true);
    CHECK_EQ(IsBonusActionSpellFeat(spell.feat), true);
    CHECK_EQ(TakesBonusAction(spell.feat), true);
    CHECK_EQ(std::string(BonusActionName(spell.feat)) == spell.name, true);
    CHECK_EQ(BonusActionFeatOfSpell(spell.spell), -1);  // not the cantrips' way: the slot is spent
  }
  CHECK_EQ(BonusActionSpellOf(kHealingWordBonus)->feat, kFeatHealingWord);
  CHECK_EQ(BonusActionSpellOf(kShieldOfFaith)->feet, 60);
  CHECK_EQ(BonusActionSpellOf(kSanctuary)->feet, 30);
  CHECK_EQ(BonusActionSpellOf(kHuntersMark)->feet, 90);
  CHECK_EQ(BonusActionSpellOf(kDivineFavor)->feet, 0);
  CHECK_EQ(BonusActionSpellOf(kExpeditiousRetreat)->bonus_row, kExpeditiousRetreatBonus);
  CHECK_EQ(BonusActionSpellOf(kBless) == nullptr, true);
  CHECK_EQ(BonusActionSpellOf(kMageArmor) == nullptr, true);
  CHECK_EQ(TakesBonusAction(kFeatExpeditiousDash), true);
  CHECK_EQ(TakesBonusAction(kFeatHuntersMarkMove), true);
  CHECK_EQ(std::string(BonusActionName(kFeatHuntersMarkMove)) == "Hunter's Mark: Move", true);
  CHECK_EQ(IsSlotSpellBonusFeat(1554), false);
  CHECK_EQ(BlessTargets(1), 3);
  CHECK_EQ(BlessTargets(3), 5);
  CHECK_EQ(MageArmorBaseAc(2), 15);
  CHECK_EQ(MageArmorBaseAc(-1), 12);
  CHECK_EQ(HuntersMarkHours(1), 1);
  CHECK_EQ(HuntersMarkHours(2), 1);
  CHECK_EQ(HuntersMarkHours(3), 8);
  CHECK_EQ(HuntersMarkHours(4), 8);
  CHECK_EQ(HuntersMarkHours(5), 24);
  CHECK_EQ(LongstriderTargets(1), 1);
  CHECK_EQ(LongstriderTargets(2), 2);
  CHECK_EQ(WardedAgainstRacialType(24), true);   // Undead
  CHECK_EQ(WardedAgainstRacialType(20), true);   // Outsider
  CHECK_EQ(WardedAgainstRacialType(7), true);    // Aberration
  CHECK_EQ(WardedAgainstRacialType(16), true);   // Elemental
  CHECK_EQ(WardedAgainstRacialType(17), true);   // Fey
  CHECK_EQ(WardedAgainstRacialType(6), false);   // Human
  CHECK_EQ(WardedAgainstRacialType(8), false);   // Animal
  CHECK_EQ(SanctuaryStopsOwnAttacks(true, true), true);    // a player on itself (0.99.15)
  CHECK_EQ(SanctuaryStopsOwnAttacks(false, true), false);  // a player on a henchman: its AI decides
  CHECK_EQ(SanctuaryStopsOwnAttacks(true, false), false);  // a henchman on itself
  CHECK_EQ(SanctuaryStopsOwnAttacks(false, false), false);
  // 0.99.22: Hunter's Mark: Move - moves once the quarry is down; armed early only with the option and a creature.
  CHECK_EQ(HuntersMarkMoveNow(true, false, false) == MarkMove::kMove, true);
  CHECK_EQ(HuntersMarkMoveNow(true, true, true) == MarkMove::kMove, true);
  CHECK_EQ(HuntersMarkMoveNow(false, true, true) == MarkMove::kArm, true);
  CHECK_EQ(HuntersMarkMoveNow(false, false, true) == MarkMove::kRefuse, true);
  CHECK_EQ(HuntersMarkMoveNow(false, true, false) == MarkMove::kRefuse, true);
  CHECK_EQ(IsConcentrationSpell(kHuntersMark), true);
  CHECK_EQ(IsConcentrationSpell(kHuntersMarkBonus), true);
  CHECK_EQ(IsConcentrationSpell(kShieldOfFaithBonus), true);
  CHECK_EQ(IsConcentrationSpell(kExpeditiousRetreatBonus), true);
  CHECK_EQ(IsConcentrationSpell(kDivineFavorBonus), false);  // 1 minute, no Concentration (2024)
  CHECK_EQ(IsConcentrationSpell(kSanctuaryBonus), false);
  CHECK_EQ(IsConcentrationSpell(kBless), true);
  CHECK_EQ(IsConcentrationSpell(kLongstrider), false);
  // The table: each buff once, its script; the Bonus Action ones marked.
  for (int row : {kBless, kBane, kShieldOfFaith, kMageArmor, kDivineFavor, kProtectionFromEvilAndGood, kSanctuary,
                  kExpeditiousRetreat, kLongstrider, kHuntersMark}) {
    const Spell2024* spell = Spell2024Of(row);
    CHECK_EQ(spell != nullptr && spell->effect == SpellEffect2024::kBuff, true);
    CHECK_EQ(spell->bonus_action, BonusActionSpellOf(row) != nullptr);
  }
  CHECK_EQ(Spell2024Of(kBane)->save == Ability::kCharisma, true);
  CHECK_EQ(Spell2024Of(kMageArmor)->melee, true);
  CHECK_EQ(Spell2024Of(kShieldOfFaith)->bonus_action, true);
  CHECK_EQ(Spell2024Of(kBless)->bonus_action, false);
  CHECK_EQ(std::string(Spell2024Of(kHuntersMark)->script) == "n5_s_huntmark", true);
  CHECK_EQ(std::string(Spell2024Of(kSanctuary)->script) == "n5_s_sanctuary", true);
}

// Build 2b: conditions on creatures - Command's words, the charms' creatures, the saves' Advantage, Sleep's stages.
TEST(Spells2024Build2b) {
  for (int row : {kCommand, kCharmPerson, kAnimalFriendship, kHideousLaughter, kColorSpray, kSleep}) {
    const Spell2024* spell = Spell2024Of(row);
    CHECK_EQ(spell != nullptr && spell->effect == SpellEffect2024::kCondition, true);
    CHECK_EQ(spell->bonus_action, false);
    CHECK_EQ(spell->half_on_save, false);
    CHECK_EQ(spell->dice, 0);
    CHECK_EQ(ConditionSpellRounds(row) > 0 || ConditionSpellHours(row) > 0, true);
  }
  for (int row : {kCommand, kCharmPerson, kAnimalFriendship, kHideousLaughter, kSleep}) {
    CHECK_EQ(Spell2024Of(row)->save == Ability::kWisdom, true);
  }
  CHECK_EQ(Spell2024Of(kColorSpray)->save == Ability::kConstitution, true);
  CHECK_EQ(Spell2024Of(kColorSpray)->shape == SpellShape2024::kCone, true);
  CHECK_EQ(Spell2024Of(kColorSpray)->feet, 15);
  CHECK_EQ(Spell2024Of(kSleep)->shape == SpellShape2024::kSphere, true);
  CHECK_EQ(Spell2024Of(kSleep)->feet, 5);
  CHECK_EQ(Spell2024Of(kCommand)->shape == SpellShape2024::kOne, true);
  CHECK_EQ(std::string(Spell2024Of(kHideousLaughter)->script) == "n5_s_laugh", true);
  CHECK_EQ(Spell2024Of(kCommandDrop) == nullptr, true);         // its words cast the master's script
  CHECK_EQ(Spell2024Of(kCharmPersonOrAnimal) == nullptr, true);  // NWN's: off the lists, not converted
  // Command's words: the radial's menu (no Approach), each word's row; no word - Halt.
  const ChoiceCantrip* command = ChoiceCantripOf(kCommand);
  CHECK_EQ(command != nullptr && command->options.size() == 4u, true);
  CHECK_EQ(command != nullptr && std::string(command->script) == "n5_s_command", true);
  CHECK_EQ(ChoiceCantripOf(kCommandGrovel) == command, true);
  CHECK_EQ(CommandWordOf(kCommandDrop) == CommandWord::kDrop, true);
  CHECK_EQ(CommandWordOf(kCommandFlee) == CommandWord::kFlee, true);
  CHECK_EQ(CommandWordOf(kCommandGrovel) == CommandWord::kGrovel, true);
  CHECK_EQ(CommandWordOf(kCommandHalt) == CommandWord::kHalt, true);
  CHECK_EQ(CommandWordOf(kCommand) == CommandWord::kHalt, true);
  for (const CantripOption& option : command->options) {
    CHECK_EQ(std::string(CommandWordName(CommandWordOf(option.spell))) == option.label, true);
  }
  // One creature, one more for each slot level above 1.
  CHECK_EQ(ConditionSpellTargets(1), 1);
  CHECK_EQ(ConditionSpellTargets(2), 2);
  CHECK_EQ(ConditionSpellTargets(4), 4);
  CHECK_EQ(ConditionSpellTargets(0), 1);
  // Ranges and durations (SRD 5.2.1).
  CHECK_EQ(ConditionSpellRangeFeet(kCommand), 60);
  CHECK_EQ(ConditionSpellRangeFeet(kSleep), 60);
  CHECK_EQ(ConditionSpellRangeFeet(kCharmPerson), 30);
  CHECK_EQ(ConditionSpellRangeFeet(kAnimalFriendship), 30);
  CHECK_EQ(ConditionSpellRangeFeet(kHideousLaughter), 30);
  CHECK_EQ(ConditionSpellRangeFeet(kColorSpray), 0);
  CHECK_EQ(ConditionSpellRounds(kCommand), 1);
  CHECK_EQ(ConditionSpellRounds(kColorSpray), 2);
  CHECK_EQ(ConditionSpellRounds(kHideousLaughter), 10);
  CHECK_EQ(ConditionSpellRounds(kSleep), 10);
  CHECK_EQ(ConditionSpellRounds(kCharmPerson), 0);
  CHECK_EQ(ConditionSpellHours(kCharmPerson), 1);
  CHECK_EQ(ConditionSpellHours(kAnimalFriendship), 24);
  CHECK_EQ(ConditionSpellHours(kSleep), 0);
  // Charm Person a Humanoid, Animal Friendship a Beast; the others anything.
  for (int type : {0, 1, 2, 3, 4, 5, 6, 12, 14, 15}) CHECK_EQ(IsHumanoidRacialType(type), true);
  for (int type : {-1, 7, 8, 9, 13, 18, 24, 25}) CHECK_EQ(IsHumanoidRacialType(type), false);
  for (int type : {8, 9, 25}) CHECK_EQ(IsBeastRacialType(type), true);
  for (int type : {6, 7, 19, 24}) CHECK_EQ(IsBeastRacialType(type), false);  // 19: Magical Beast, a Monstrosity
  CHECK_EQ(ConditionSpellTargetFits(kCharmPerson, 6), true);
  CHECK_EQ(ConditionSpellTargetFits(kCharmPerson, 8), false);
  CHECK_EQ(ConditionSpellTargetFits(kAnimalFriendship, 8), true);
  CHECK_EQ(ConditionSpellTargetFits(kAnimalFriendship, 6), false);
  CHECK_EQ(ConditionSpellTargetFits(kCommand, 24), true);  // no undead clause in 2024
  CHECK_EQ(ConditionSpellTargetFits(kHideousLaughter, 10), true);
  // Charm Person: Advantage when you or your allies are fighting it.
  CHECK_EQ(CharmSaveAdvantage(true, true), true);
  CHECK_EQ(CharmSaveAdvantage(true, false), false);
  CHECK_EQ(CharmSaveAdvantage(false, true), false);
  // Hideous Laughter: Advantage on the save damage brings, none at its turn's end.
  CHECK_EQ(LaughterSaveAdvantage(true), true);
  CHECK_EQ(LaughterSaveAdvantage(false), false);
  // Sleep: the second save - a failure Unconscious, a success awake; who doesn't sleep.
  CHECK_EQ(SleepAfterSecondSave(false) == SleepStage::kUnconscious, true);
  CHECK_EQ(SleepAfterSecondSave(true) == SleepStage::kAwake, true);
  for (int type : {1, 10, 16, 24, 29}) CHECK_EQ(SleepUnaffectedRacialType(type), true);
  for (int type : {0, 4, 6, 8, 12, 20}) CHECK_EQ(SleepUnaffectedRacialType(type), false);
  CHECK_EQ(kRouseFeet, 5);
  // Concentration: Hideous Laughter and Sleep; not Command, the charms or Color Spray.
  CHECK_EQ(IsConcentrationSpell(kHideousLaughter), true);
  CHECK_EQ(IsConcentrationSpell(kSleep), true);
  CHECK_EQ(IsConcentrationSpell(kCommand), false);
  CHECK_EQ(IsConcentrationSpell(kCommandHalt), false);
  CHECK_EQ(IsConcentrationSpell(kCharmPerson), false);
  CHECK_EQ(IsConcentrationSpell(kAnimalFriendship), false);
  CHECK_EQ(IsConcentrationSpell(kColorSpray), false);
}
