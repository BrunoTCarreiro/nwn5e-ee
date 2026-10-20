#include "rules/saves.h"

namespace rules {

bool HasSaveProficiency(CharacterClass first_class, Ability ability) {
  using A = Ability;
  switch (first_class) {
    case CharacterClass::kBarbarian:
    case CharacterClass::kFighter:
      return ability == A::kStrength || ability == A::kConstitution;
    case CharacterClass::kBard:
      return ability == A::kDexterity || ability == A::kCharisma;
    case CharacterClass::kCleric:
    case CharacterClass::kPaladin:
    case CharacterClass::kWarlock:
      return ability == A::kWisdom || ability == A::kCharisma;
    case CharacterClass::kDruid:
    case CharacterClass::kWizard:
      return ability == A::kIntelligence || ability == A::kWisdom;
    case CharacterClass::kMonk:
    case CharacterClass::kRanger:
      return ability == A::kStrength || ability == A::kDexterity;
    case CharacterClass::kRogue:
      return ability == A::kDexterity || ability == A::kIntelligence;
    case CharacterClass::kSorcerer:
      return ability == A::kConstitution || ability == A::kCharisma;
  }
  return false;
}

int SavingThrowBonus(int ability_modifier, bool proficient, int proficiency_bonus, int other_bonus) {
  return ability_modifier + (proficient ? proficiency_bonus : 0) + other_bonus;
}

bool FeatGivesSaveProficiency(NwnSave save, bool great_fortitude, bool lightning_reflexes, bool iron_will) {
  switch (save) {
    case NwnSave::kFortitude:
      return great_fortitude;
    case NwnSave::kReflex:
      return lightning_reflexes;
    case NwnSave::kWill:
      return iron_will;
  }
  return false;
}

int AuraOfProtectionBonus(int paladin_level, int charisma_modifier, bool conscious, float distance_m) {
  if (paladin_level < 6 || !conscious || !(distance_m <= 3.0f)) return 0;
  return charisma_modifier > 1 ? charisma_modifier : 1;
}

bool IsGoodNwnSaveTable(int bonus_at_level_1) { return bonus_at_level_1 >= 2; }

int ConvertNwnFixedDc(int dc) {
  const int half = dc >= 10 ? (dc - 10 + 1) / 2 : -((10 - dc) / 2);
  const int converted = 10 + half;
  return converted < 10 ? 10 : (converted > 30 ? 30 : converted);
}

int ShoveDc(int strength_modifier, int proficiency_bonus) { return 8 + strength_modifier + proficiency_bonus; }

Ability ShoveSaveAbility(int strength_save, int dexterity_save) {
  return strength_save > dexterity_save ? Ability::kStrength : Ability::kDexterity;
}

bool ShoveSizeAllowed(int shover_size, int target_size) { return target_size <= shover_size + 1; }

int SpellScrollAttackBonus(int spell_level) { return SpellScrollDc(spell_level) - 8; }

int SpellAttackBonus2024(int proficiency_bonus, int casting_modifier) { return proficiency_bonus + casting_modifier; }

int SpellScrollDc(int spell_level) {
  if (spell_level <= 2) return 13;
  if (spell_level <= 4) return 15;
  if (spell_level <= 6) return 17;
  if (spell_level <= 8) return 18;
  return 19;
}

int NwnModeSpellFocusDcBonus(int nwn_focus_bonus) { return nwn_focus_bonus / 2; }

int SaveValue(const SaveFacts& facts, RuleVariant variant) {
  const Ability ability = facts.ability ? *facts.ability : AbilityForNwnSave(facts.save);
  const bool class_proficient =
      facts.character && facts.first_class && HasSaveProficiency(*facts.first_class, ability);
  if (variant == RuleVariant::kNwn) {
    const bool proficient = facts.character ? class_proficient : facts.good_nwn_table;
    return facts.ability_modifier + (proficient ? facts.proficiency_bonus : 0) + facts.effects + facts.nwn_extras;
  }
  const bool proficient = class_proficient || (facts.character && facts.save_feat);
  return facts.ability_modifier + (proficient ? facts.proficiency_bonus : 0) + facts.effects + facts.aura;
}

bool SaveSucceeds(int d20, int bonus, int dc, bool natural_20_succeeds, bool natural_1_fails) {
  if (d20 == 20 && natural_20_succeeds) return true;
  if (d20 == 1 && natural_1_fails) return false;
  return d20 + bonus >= dc;
}

Ability AbilityForNwnSave(NwnSave save) {
  switch (save) {
    case NwnSave::kFortitude:
      return Ability::kConstitution;
    case NwnSave::kReflex:
      return Ability::kDexterity;
    case NwnSave::kWill:
      return Ability::kWisdom;
  }
  return Ability::kConstitution;
}

std::optional<Ability> SaveAbilityForNwnSpell(int spell_id) {
  switch (spell_id) {
    case 53:   // Entangle
    case 75:   // Gust of Wind
    case 375:  // Evard's Black Tentacles
      return Ability::kStrength;
    case 55:   // Feeblemind
      return Ability::kIntelligence;
    case 69:   // Greater Planar Binding
    case 96:   // Lesser Planar Binding
    case 128:  // Planar Binding
    case 430:  // Banishment
    case 449:  // Bane
      return Ability::kCharisma;
    case 25:   // Cone of Cold
    case 183:  // Sunbeam
    case 427:  // Sunburst
    case 431:  // Inflict Minor Wounds
    case 432:  // Inflict Light Wounds
    case 433:  // Inflict Moderate Wounds
    case 434:  // Inflict Serious Wounds
    case 435:  // Inflict Critical Wounds
    case 611:  // Inflict Serious Wounds (Blackguard)
    case 612:  // Inflict Critical Wounds (Blackguard)
      return Ability::kConstitution;
    default:
      return std::nullopt;
  }
}

const char* Faithful2024SaveLabel(uint32_t strref) {
  switch (strref) {
    case 317:  // "Fortitude"
    case 977:
      return "Constitution";
    case 318:  // "Reflex"
    case 978:
      return "Dexterity";
    case 319:  // "Will"
    case 979:
      return "Wisdom";
    case 520:
      return "Constitution Save";
    case 519:
      return "Dexterity Save";
    case 745:
      return "Wisdom Save";
    case 6562:
      return "Saving Throw Type: Constitution";
    case 6563:
      return "Saving Throw Type: Dexterity";
    case 6564:
      return "Saving Throw Type: Wisdom";
    case 66232:
      return "Constitution Saving Throw Increased";
    case 66231:
      return "Dexterity Saving Throw Increased";
    case 66233:
      return "Wisdom Saving Throw Increased";
    default:
      return nullptr;
  }
}

int FeedbackSaveKind(Ability ability) { return kFeedbackSaveKindFirst + static_cast<int>(ability); }

uint32_t SaveLabelStrrefForFeedbackKind(int kind) {
  const int index = kind - kFeedbackSaveKindFirst;
  return index >= 0 && index < 6 ? kSaveLabelStrrefFirst + static_cast<uint32_t>(index) : 0;
}

const char* SaveLabelForStrref(uint32_t strref) {
  static const char* const kLabels[] = {"Strength Save",     "Dexterity Save", "Constitution Save",
                                        "Intelligence Save", "Wisdom Save",    "Charisma Save"};
  if (strref < kSaveLabelStrrefFirst || strref >= kSaveLabelStrrefFirst + 6) return nullptr;
  return kLabels[strref - kSaveLabelStrrefFirst];
}

}  // namespace rules
