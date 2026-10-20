#include <string>

#include "check.h"
#include "rules/saves.h"

using namespace rules;

// SRD 5.2 class tables. Breaks caught: a class with the wrong pair (e.g. the
// 3e good saves: Monk all three, Rogue Reflex only), or a class proficient in
// more or fewer than two saves.
TEST(ClassSaveProficienciesAre2024) {
  CHECK_EQ(HasSaveProficiency(CharacterClass::kFighter, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kFighter, Ability::kConstitution), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kFighter, Ability::kDexterity), false);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRogue, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRogue, Ability::kIntelligence), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWizard, Ability::kIntelligence), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWizard, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWizard, Ability::kConstitution), false);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kMonk, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kMonk, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kMonk, Ability::kWisdom), false);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBarbarian, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBarbarian, Ability::kConstitution), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBard, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kBard, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kCleric, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kCleric, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kDruid, Ability::kIntelligence), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kDruid, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kPaladin, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kPaladin, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRanger, Ability::kStrength), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kRanger, Ability::kDexterity), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kSorcerer, Ability::kConstitution), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kSorcerer, Ability::kCharisma), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWarlock, Ability::kWisdom), true);
  CHECK_EQ(HasSaveProficiency(CharacterClass::kWarlock, Ability::kCharisma), true);
  const CharacterClass all[] = {CharacterClass::kBarbarian, CharacterClass::kBard,    CharacterClass::kCleric,
                                CharacterClass::kDruid,     CharacterClass::kFighter, CharacterClass::kMonk,
                                CharacterClass::kPaladin,   CharacterClass::kRanger,  CharacterClass::kRogue,
                                CharacterClass::kSorcerer,  CharacterClass::kWarlock, CharacterClass::kWizard};
  const Ability abilities[] = {Ability::kStrength,     Ability::kDexterity, Ability::kConstitution,
                               Ability::kIntelligence, Ability::kWisdom,    Ability::kCharisma};
  for (CharacterClass c : all) {
    int count = 0;
    for (Ability a : abilities) count += HasSaveProficiency(c, a) ? 1 : 0;
    CHECK_EQ(count, 2);
  }
}

// Breaks caught: the proficiency bonus added without proficiency, or other
// bonuses (effects, items) dropped.
TEST(SavingThrowBonusIsModifierPlusProficiency) {
  CHECK_EQ(SavingThrowBonus(3, true, 2, 0), 5);
  CHECK_EQ(SavingThrowBonus(3, false, 2, 0), 3);
  CHECK_EQ(SavingThrowBonus(-1, true, 3, 0), 2);
  CHECK_EQ(SavingThrowBonus(2, false, 4, 1), 3);
}

// Phase 1c proposed default (PLAN.md). Breaks caught: a 3e save read with the
// wrong ability.
TEST(NwnSavesMapToAbilities) {
  CHECK_EQ(AbilityForNwnSave(NwnSave::kFortitude), Ability::kConstitution);
  CHECK_EQ(AbilityForNwnSave(NwnSave::kReflex), Ability::kDexterity);
  CHECK_EQ(AbilityForNwnSave(NwnSave::kWill), Ability::kWisdom);
}

// C3: Great Fortitude / Lightning Reflexes / Iron Will give proficiency in
// the matching save (like 2024 Resilient). Breaks caught: a feat granting the
// wrong save, or none.
TEST(SaveFeatsGiveProficiency) {
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kFortitude, true, false, false), true);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kReflex, true, false, false), false);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kReflex, false, true, false), true);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kWill, false, false, true), true);
  CHECK_EQ(FeatGivesSaveProficiency(NwnSave::kWill, true, true, false), false);
}

// C3: 2024 Paladin Aura of Protection. Breaks caught: before paladin level 6;
// a Charisma modifier of 0 or less not raised to +1; an unconscious paladin
// still projecting it; allies beyond 10 ft (3 m) included.
TEST(AuraOfProtection) {
  CHECK_EQ(AuraOfProtectionBonus(6, 3, true, 0.0f), 3);
  CHECK_EQ(AuraOfProtectionBonus(5, 3, true, 0.0f), 0);
  CHECK_EQ(AuraOfProtectionBonus(10, 0, true, 2.0f), 1);
  CHECK_EQ(AuraOfProtectionBonus(10, -1, true, 2.0f), 1);
  CHECK_EQ(AuraOfProtectionBonus(10, 4, false, 0.0f), 0);
  CHECK_EQ(AuraOfProtectionBonus(10, 4, true, 3.0f), 4);
  CHECK_EQ(AuraOfProtectionBonus(10, 4, true, 3.1f), 0);
}

// C2 NWN mode: a creature is proficient in a save its class tables make
// "good" (+2 at level 1). Breaks caught: poor tables (+0) counted.
TEST(GoodNwnSaveTables) {
  CHECK_EQ(IsGoodNwnSaveTable(2), true);
  CHECK_EQ(IsGoodNwnSaveTable(0), false);
  CHECK_EQ(IsGoodNwnSaveTable(1), false);
}

// C6: NWN's fixed DCs onto the 2024 scale, 10 + (DC - 10) / 2 rounded half
// up, within 10..30. Breaks caught: the PLAN.md examples drifting; low DCs
// dropping under 10; huge DCs unbounded.
TEST(NwnFixedDcsConvert) {
  CHECK_EQ(ConvertNwnFixedDc(14), 12);
  CHECK_EQ(ConvertNwnFixedDc(20), 15);
  CHECK_EQ(ConvertNwnFixedDc(26), 18);
  CHECK_EQ(ConvertNwnFixedDc(30), 20);
  CHECK_EQ(ConvertNwnFixedDc(40), 25);
  CHECK_EQ(ConvertNwnFixedDc(15), 13);
  CHECK_EQ(ConvertNwnFixedDc(10), 10);
  CHECK_EQ(ConvertNwnFixedDc(5), 10);
  CHECK_EQ(ConvertNwnFixedDc(60), 30);
}

// C5: 2024 spell scroll DCs by spell level. Breaks caught: any row of the
// table wrong; levels outside 0..9 not clamped.
TEST(SpellScrollDcs) {
  CHECK_EQ(SpellScrollDc(0), 13);
  CHECK_EQ(SpellScrollDc(1), 13);
  CHECK_EQ(SpellScrollDc(2), 13);
  CHECK_EQ(SpellScrollDc(3), 15);
  CHECK_EQ(SpellScrollDc(4), 15);
  CHECK_EQ(SpellScrollDc(5), 17);
  CHECK_EQ(SpellScrollDc(6), 17);
  CHECK_EQ(SpellScrollDc(7), 18);
  CHECK_EQ(SpellScrollDc(8), 18);
  CHECK_EQ(SpellScrollDc(9), 19);
  CHECK_EQ(SpellScrollDc(12), 19);
  CHECK_EQ(SpellScrollDc(-1), 13);
}

// C5 NWN mode: Spell Focus / Greater / Epic (+2 / +4 / +6 in NWN) count as
// +1 / +2 / +3.
TEST(SpellFocusInNwnMode) {
  CHECK_EQ(NwnModeSpellFocusDcBonus(0), 0);
  CHECK_EQ(NwnModeSpellFocusDcBonus(2), 1);
  CHECK_EQ(NwnModeSpellFocusDcBonus(4), 2);
  CHECK_EQ(NwnModeSpellFocusDcBonus(6), 3);
}

// C1-C3 put together. Breaks caught: a creature proficient in faithful mode;
// NWN mode losing creature good saves; a save feat counted twice in NWN mode
// (proficiency and +2); the aura in NWN mode or NWN's extras in faithful;
// magic effects dropped in either mode.
TEST(SaveValueByMode) {
  SaveFacts fighter;
  fighter.save = NwnSave::kFortitude;
  fighter.ability_modifier = 2;
  fighter.proficiency_bonus = 3;
  fighter.character = true;
  fighter.first_class = CharacterClass::kFighter;
  CHECK_EQ(SaveValue(fighter, RuleVariant::kFaithful), 5);
  CHECK_EQ(SaveValue(fighter, RuleVariant::kNwn), 5);
  SaveFacts fighter_will = fighter;
  fighter_will.save = NwnSave::kWill;
  fighter_will.ability_modifier = 1;
  CHECK_EQ(SaveValue(fighter_will, RuleVariant::kFaithful), 1);
  fighter_will.save_feat = true;  // Iron Will
  fighter_will.nwn_extras = 2;    // NWN's +2 for it
  CHECK_EQ(SaveValue(fighter_will, RuleVariant::kFaithful), 4);
  CHECK_EQ(SaveValue(fighter_will, RuleVariant::kNwn), 3);
  SaveFacts ogre;
  ogre.save = NwnSave::kFortitude;
  ogre.ability_modifier = 4;
  ogre.proficiency_bonus = 2;
  ogre.good_nwn_table = true;
  CHECK_EQ(SaveValue(ogre, RuleVariant::kFaithful), 4);
  CHECK_EQ(SaveValue(ogre, RuleVariant::kNwn), 6);
  SaveFacts buffed = fighter;
  buffed.effects = 1;
  buffed.aura = 3;
  buffed.nwn_extras = 2;
  CHECK_EQ(SaveValue(buffed, RuleVariant::kFaithful), 9);
  CHECK_EQ(SaveValue(buffed, RuleVariant::kNwn), 8);
}

// C4: faithful has no automatic success or failure; NWN mode: a natural 20
// always saves, a natural 1 always fails when the game option is on.
TEST(NaturalRollsOnSaves) {
  CHECK_EQ(SaveSucceeds(20, 0, 25, false, false), false);
  CHECK_EQ(SaveSucceeds(20, 0, 25, true, false), true);
  CHECK_EQ(SaveSucceeds(1, 15, 10, false, false), true);
  CHECK_EQ(SaveSucceeds(1, 15, 10, false, true), false);
  CHECK_EQ(SaveSucceeds(12, 3, 15, false, false), true);
  CHECK_EQ(SaveSucceeds(11, 3, 15, false, false), false);
}

// C1: spells that use another ability in 2024. Breaks caught: a table spell
// left on the default ability; a spell whose 2024 save already matches (e.g.
// Fireball 58, Hold Person 82) or an unknown id given an override.
TEST(SpellSaveAbilitiesAre2024) {
  CHECK_EQ(static_cast<int>(*SaveAbilityForNwnSpell(53)), static_cast<int>(Ability::kStrength));
  CHECK_EQ(static_cast<int>(*SaveAbilityForNwnSpell(55)), static_cast<int>(Ability::kIntelligence));
  CHECK_EQ(static_cast<int>(*SaveAbilityForNwnSpell(430)), static_cast<int>(Ability::kCharisma));
  CHECK_EQ(static_cast<int>(*SaveAbilityForNwnSpell(25)), static_cast<int>(Ability::kConstitution));
  CHECK_EQ(SaveAbilityForNwnSpell(58).has_value(), false);
  CHECK_EQ(SaveAbilityForNwnSpell(82).has_value(), false);
  CHECK_EQ(SaveAbilityForNwnSpell(-1).has_value(), false);
}

// C1: an overriding ability decides proficiency. Breaks caught: a Fighter not
// proficient in a Strength save (or proficient in an Intelligence one); a
// creature given proficiency in faithful mode.
TEST(SaveValueWithSpellAbility) {
  SaveFacts facts;
  facts.save = NwnSave::kReflex;  // Entangle: NWN Reflex, 2024 Strength
  facts.ability = Ability::kStrength;
  facts.ability_modifier = 3;
  facts.proficiency_bonus = 2;
  facts.character = true;
  facts.first_class = CharacterClass::kFighter;
  CHECK_EQ(SaveValue(facts, RuleVariant::kFaithful), 5);
  facts.ability = Ability::kIntelligence;
  facts.ability_modifier = 0;
  CHECK_EQ(SaveValue(facts, RuleVariant::kFaithful), 0);
  facts.character = false;
  facts.ability = Ability::kStrength;
  facts.ability_modifier = 3;
  CHECK_EQ(SaveValue(facts, RuleVariant::kFaithful), 3);
}

// C8: the labels that change. Breaks caught: one of the three saves missed in
// a label family; an item name ("Ring of Fortitude +1", 56644) or a dialogue
// line starting with "Will" (44902) renamed.
TEST(SaveLabelsAre2024) {
  CHECK_EQ(std::string(Faithful2024SaveLabel(317)) == "Constitution", true);
  CHECK_EQ(std::string(Faithful2024SaveLabel(978)) == "Dexterity", true);
  CHECK_EQ(std::string(Faithful2024SaveLabel(745)) == "Wisdom Save", true);
  CHECK_EQ(std::string(Faithful2024SaveLabel(520)) == "Constitution Save", true);
  CHECK_EQ(std::string(Faithful2024SaveLabel(6563)) == "Saving Throw Type: Dexterity", true);
  CHECK_EQ(std::string(Faithful2024SaveLabel(66233)) == "Wisdom Saving Throw Increased", true);
  CHECK_EQ(Faithful2024SaveLabel(56644) == nullptr, true);
  CHECK_EQ(Faithful2024SaveLabel(44902) == nullptr, true);
  CHECK_EQ(Faithful2024SaveLabel(0x1000000 | 317) == nullptr, true);
}

// C1 + C8: a save on a spell's own ability is named by that ability in the
// combat log (the owner saw "Dexterity Save vs. Cold" for a Constitution save,
// 2026-10-19). Breaks caught: a kind that collides with NWN's 1-3 or does not
// fit the 8-bit message field; a private string id on a dialog.tlk row or with
// the custom-table flag; the wrong ability's label; NWN's kinds relabelled.
TEST(SaveFeedbackNamesTheSpellsAbility) {
  CHECK_EQ(FeedbackSaveKind(Ability::kStrength), 0x11);
  CHECK_EQ(FeedbackSaveKind(Ability::kCharisma), 0x16);
  CHECK_EQ(SaveLabelStrrefForFeedbackKind(1) == 0, true);
  CHECK_EQ(SaveLabelStrrefForFeedbackKind(3) == 0, true);
  CHECK_EQ(SaveLabelStrrefForFeedbackKind(0x17) == 0, true);
  CHECK_EQ((kSaveLabelStrrefFirst & 0x1000000) == 0 && kSaveLabelStrrefFirst > 0x100000, true);
  const uint32_t con = SaveLabelStrrefForFeedbackKind(FeedbackSaveKind(Ability::kConstitution));
  CHECK_EQ(std::string(SaveLabelForStrref(con)) == "Constitution Save", true);
  CHECK_EQ(std::string(SaveLabelForStrref(SaveLabelStrrefForFeedbackKind(0x11))) == "Strength Save", true);
  CHECK_EQ(std::string(SaveLabelForStrref(SaveLabelStrrefForFeedbackKind(0x16))) == "Charisma Save", true);
  CHECK_EQ(SaveLabelForStrref(519) == nullptr, true);
  CHECK_EQ(SaveLabelForStrref(kSaveLabelStrrefFirst + 6) == nullptr, true);
}

// SK10. Breaks caught: a scroll's attack bonus off the 2024 table (+5 / +7 / +9
// / +10 / +11); the spell attack bonus not PB + casting modifier.
TEST(SpellAttacksAre2024) {
  CHECK_EQ(SpellScrollAttackBonus(0), 5);
  CHECK_EQ(SpellScrollAttackBonus(2), 5);
  CHECK_EQ(SpellScrollAttackBonus(3), 7);
  CHECK_EQ(SpellScrollAttackBonus(5), 9);
  CHECK_EQ(SpellScrollAttackBonus(7), 10);
  CHECK_EQ(SpellScrollAttackBonus(9), 11);
  CHECK_EQ(SpellAttackBonus2024(2, 3), 5);
  CHECK_EQ(SpellAttackBonus2024(6, -1), 5);
}

// SK6. Breaks caught: the shove DC off 8 + Str + PB; the target not using its
// better save (or Strength winning a tie); a two-sizes-larger target allowed
// (NWN's Improved Knockdown) or a one-size-larger one refused.
TEST(ShoveIs2024) {
  CHECK_EQ(ShoveDc(3, 2), 13);
  CHECK_EQ(ShoveDc(-1, 4), 11);
  CHECK_EQ(static_cast<int>(ShoveSaveAbility(5, 2)), static_cast<int>(Ability::kStrength));
  CHECK_EQ(static_cast<int>(ShoveSaveAbility(1, 4)), static_cast<int>(Ability::kDexterity));
  CHECK_EQ(static_cast<int>(ShoveSaveAbility(3, 3)), static_cast<int>(Ability::kDexterity));
  CHECK_EQ(ShoveSizeAllowed(3, 4), true);   // medium shoves large
  CHECK_EQ(ShoveSizeAllowed(3, 5), false);  // not huge
  CHECK_EQ(ShoveSizeAllowed(2, 4), false);  // small does not shove large
  CHECK_EQ(ShoveSizeAllowed(3, 1), true);
}
