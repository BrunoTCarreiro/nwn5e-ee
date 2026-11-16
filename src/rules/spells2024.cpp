#include "rules/spells2024.h"

#include <stdio.h>

#include <algorithm>
#include <cmath>

#include "rules/cantrips.h"

namespace rules {

namespace {

constexpr uint32_t kDamageTypePiercing = 2;

Spell2024 Damage(int row, const char* name, const char* script, SpellEffect2024 effect, int dice, int die,
                 int upcast_dice, uint32_t type) {
  Spell2024 spell;
  spell.row = row;
  spell.name = name;
  spell.script = script;
  spell.effect = effect;
  spell.dice = dice;
  spell.die = die;
  spell.upcast_dice = upcast_dice;
  spell.damage_type = type;
  return spell;
}

Spell2024 Saved(Spell2024 spell, Ability save, bool half) {
  spell.save = save;
  spell.half_on_save = half;
  return spell;
}

Spell2024 Area(Spell2024 spell, SpellShape2024 shape, int feet) {
  spell.shape = shape;
  spell.feet = feet;
  return spell;
}

Spell2024 Touch(Spell2024 spell) {
  spell.melee = true;
  return spell;
}

std::vector<Spell2024> Build() {
  using E = SpellEffect2024;
  std::vector<Spell2024> spells;
  spells.push_back(Area(Saved(Damage(kBurningHands, "Burning Hands", "n5_s_burnhands", E::kSave, 3, 6, 1,
                                     kDamageTypeFire),
                              Ability::kDexterity, true),
                        SpellShape2024::kCone, 15));
  {
    Spell2024 cure = Touch(Damage(kCureWounds, "Cure Wounds", "n5_s_curewounds", E::kHealing, 2, 8, 2, 0));
    spells.push_back(cure);
  }
  {
    Spell2024 missile = Damage(kMagicMissile, "Magic Missile", "n5_s_magicmiss", E::kDarts, 1, 4, 0, kDamageTypeForce);
    missile.flat = 1;
    spells.push_back(missile);
  }
  spells.push_back(Touch(Saved(Damage(kInflictWounds, "Inflict Wounds", "n5_s_inflict", E::kSave, 2, 10, 1,
                                      kDamageTypeNecrotic),
                               Ability::kConstitution, true)));
  spells.push_back(Area(Saved(Damage(kThunderwave, "Thunderwave", "n5_s_thunderwave", E::kSave, 2, 8, 1,
                                     kDamageTypeThunder),
                              Ability::kConstitution, true),
                        SpellShape2024::kCubeFromCaster, 15));
  spells.push_back(Damage(kIceKnife, "Ice Knife", "n5_s_iceknife", E::kSpellAttack, 1, 10, 0, kDamageTypePiercing));
  spells.push_back(Damage(kGuidingBolt, "Guiding Bolt", "n5_s_guidbolt", E::kSpellAttack, 4, 6, 1, kDamageTypeRadiant));
  spells.push_back(Damage(kRayOfSickness, "Ray of Sickness", "n5_s_raysick", E::kSpellAttack, 2, 8, 1,
                          kDamageTypePoison));
  {
    Spell2024 life = Damage(kFalseLife, "False Life", "n5_s_falselife", E::kTemporaryHitPoints, 2, 4, 0, 0);
    life.flat = 4;
    life.upcast_flat = 5;
    spells.push_back(life);
  }
  spells.push_back(Saved(Damage(kDissonantWhispers, "Dissonant Whispers", "n5_s_dissonant", E::kSave, 3, 6, 1,
                                kDamageTypePsychic),
                         Ability::kWisdom, true));
  {
    Spell2024 word = Damage(kHealingWord, "Healing Word", "n5_s_healword", E::kHealing, 2, 4, 2, 0);
    word.bonus_action = true;
    spells.push_back(word);
  }
  spells.push_back(Damage(kChromaticOrb, "Chromatic Orb", "n5_s_chromorb", E::kSpellAttack, 3, 8, 1, 0));
  spells.push_back(Touch(Damage(kHeroism, "Heroism", "n5_s_heroism", E::kBuff, 0, 0, 0, 0)));
  // Build 2a: the buffs.
  spells.push_back(Damage(kBless, "Bless", "n5_s_bless", E::kBuff, 0, 0, 0, 0));
  spells.push_back(Saved(Damage(kBane, "Bane", "n5_s_bane", E::kBuff, 0, 0, 0, 0), Ability::kCharisma, false));
  for (const BonusActionSpell& bonus : BonusActionSpells()) {
    if (bonus.spell == kHealingWord) continue;
    const char* script = bonus.spell == kShieldOfFaith ? "n5_s_shieldfaith"
                         : bonus.spell == kDivineFavor ? "n5_s_divfavor"
                         : bonus.spell == kSanctuary   ? "n5_s_sanctuary"
                         : bonus.spell == kExpeditiousRetreat ? "n5_s_expretreat"
                                                               : "n5_s_huntmark";
    Spell2024 spell = Damage(bonus.spell, bonus.name, script, E::kBuff, 0, 0, 0, 0);
    spell.bonus_action = true;
    spells.push_back(spell);
  }
  spells.push_back(Touch(Damage(kMageArmor, "Mage Armor", "n5_s_magearmor", E::kBuff, 0, 0, 0, 0)));
  spells.push_back(Touch(Damage(kProtectionFromEvilAndGood, "Protection from Evil and Good", "n5_s_protevil",
                                E::kBuff, 0, 0, 0, 0)));
  spells.push_back(Touch(Damage(kLongstrider, "Longstrider", "n5_s_longstrider", E::kBuff, 0, 0, 0, 0)));
  // Build 2b: conditions on creatures (a save, no damage).
  spells.push_back(Saved(Damage(kCommand, "Command", "n5_s_command", E::kCondition, 0, 0, 0, 0), Ability::kWisdom,
                        false));
  spells.push_back(Saved(Damage(kCharmPerson, "Charm Person", "n5_s_charmper", E::kCondition, 0, 0, 0, 0),
                        Ability::kWisdom, false));
  spells.push_back(Saved(Damage(kAnimalFriendship, "Animal Friendship", "n5_s_animalfr", E::kCondition, 0, 0, 0, 0),
                        Ability::kWisdom, false));
  spells.push_back(Saved(Damage(kHideousLaughter, "Hideous Laughter", "n5_s_laugh", E::kCondition, 0, 0, 0, 0),
                        Ability::kWisdom, false));
  spells.push_back(Area(Saved(Damage(kColorSpray, "Color Spray", "n5_s_colspray", E::kCondition, 0, 0, 0, 0),
                             Ability::kConstitution, false),
                       SpellShape2024::kCone, 15));
  spells.push_back(Area(Saved(Damage(kSleep, "Sleep", "n5_s_sleep", E::kCondition, 0, 0, 0, 0), Ability::kWisdom,
                             false),
                       SpellShape2024::kSphere, 5));
  // Build 2c: the areas.
  spells.push_back(Area(Saved(Damage(kEntangle, "Entangle", "n5_s_entangle", E::kArea, 0, 0, 0, 0), Ability::kStrength,
                             false),
                       SpellShape2024::kSquare, 20));
  spells.push_back(Area(Saved(Damage(kGrease, "Grease", "n5_s_grease", E::kArea, 0, 0, 0, 0), Ability::kDexterity,
                             false),
                       SpellShape2024::kSquare, 10));
  spells.push_back(Area(Saved(Damage(kFaerieFire, "Faerie Fire", "n5_s_faerie", E::kArea, 0, 0, 0, 0),
                             Ability::kDexterity, false),
                       SpellShape2024::kSquare, 20));
  spells.push_back(Area(Damage(kFogCloud, "Fog Cloud", "n5_s_fogcloud", E::kArea, 0, 0, 0, 0), SpellShape2024::kSphere,
                       20));
  // Build 3: reactions and smites.
  spells.push_back(Damage(kShield, "Shield", "n5_s_shield", E::kReaction, 0, 0, 0, 0));
  spells.push_back(Damage(kFeatherFall, "Feather Fall", "n5_s_featherfall", E::kReaction, 0, 0, 0, 0));
  for (Spell2024 smite : {Damage(kDivineSmite, "Divine Smite", "n5_s_divsmite", E::kSmite, 2, 8, 1, kDamageTypeRadiant),
                         Damage(kSearingSmite, "Searing Smite", "n5_s_searsmite", E::kSmite, 1, 6, 1, kDamageTypeFire),
                         Saved(Damage(kEnsnaringStrike, "Ensnaring Strike", "n5_s_ensnare", E::kSmite, 1, 6, 1,
                                      kDamageTypePiercing),
                               Ability::kStrength, false)}) {
    smite.bonus_action = true;  // paid by the hit (Q1 A)
    spells.push_back(smite);
  }
  // Build 4a: detection and utility.
  const auto utility = [](int row, const char* name, const char* script, bool ritual) {
    Spell2024 spell = Damage(row, name, script, E::kUtility, 0, 0, 0, 0);
    spell.ritual = ritual;
    return spell;
  };
  spells.push_back(utility(kIdentify, "Identify", "n5_s_identify", true));
  spells.push_back(utility(kAlarm, "Alarm", "n5_s_alarm", true));
  spells.push_back(utility(kComprehendLanguages, "Comprehend Languages", "n5_s_comprehend", true));
  spells.push_back(utility(kCreateOrDestroyWater, "Create or Destroy Water", "n5_s_water", false));
  spells.push_back(utility(kDetectEvilAndGood, "Detect Evil and Good", "n5_s_detectevil", false));
  spells.push_back(utility(kDetectMagic, "Detect Magic", "n5_s_detectmagic", true));
  spells.push_back(utility(kDetectPoisonAndDisease, "Detect Poison and Disease", "n5_s_detpoison", true));
  spells.push_back(utility(kGoodberry, "Goodberry", "n5_s_goodberry", false));
  spells.push_back(utility(kGoodberryEat, "Goodberry", "n5_s_berryeat", false));
  spells.push_back(utility(kIllusoryScript, "Illusory Script", "n5_s_illscript", true));
  spells.push_back(utility(kPurifyFoodAndDrink, "Purify Food and Drink", "n5_s_purify", true));
  spells.push_back(utility(kSpeakWithAnimals, "Speak with Animals", "n5_s_speakanimal", true));
  spells.push_back(utility(kUnseenServant, "Unseen Servant", "n5_s_servant", true));
  // Build 4b: the illusions.
  spells.push_back(utility(kDisguiseSelf, "Disguise Self", "n5_s_disguise", false));
  spells.push_back(utility(kSilentImage, "Silent Image", "n5_s_silentimg", false));
  spells.push_back(utility(kFindFamiliar, "Find Familiar", "n5_s_findfam", true));  // build 4c
  std::sort(spells.begin(), spells.end(), [](const Spell2024& a, const Spell2024& b) { return a.row < b.row; });
  return spells;
}

}  // namespace

const std::vector<Spell2024>& Spells2024() {
  static const std::vector<Spell2024> spells = Build();
  return spells;
}

const Spell2024* Spell2024Of(int row) {
  for (const Spell2024& spell : Spells2024()) {
    if (spell.row == row) return &spell;
  }
  return nullptr;
}

int CastSlotLevel(int spell_level, int chosen) {
  const int own = std::max(1, spell_level);
  if (chosen <= own) return own;
  return std::min(9, chosen);
}

int DiceAtSlot(int dice, int per_level, int spell_level, int slot_level) {
  return dice + per_level * std::max(0, slot_level - spell_level);
}

int SpellDiceAt(const Spell2024& spell, int slot_level) {
  return DiceAtSlot(spell.dice, spell.upcast_dice, spell.level, slot_level);
}

int CriticalDice(int dice, bool critical) { return critical ? 2 * dice : dice; }

int DamageAfterSave(int damage, bool saved, bool half_on_save) {
  if (damage <= 0) return 0;
  if (!saved) return damage;
  return half_on_save ? damage / 2 : 0;
}

int MagicMissileDarts(int slot_level) { return 3 + std::max(0, slot_level - 1); }

int FalseLifeTemporaryHitPoints(int roll, int slot_level) { return roll + 4 + 5 * std::max(0, slot_level - 1); }

bool TemporaryHitPointsReplace(int current, int offered) { return offered > current; }

int IceKnifeExplosionDice(int slot_level) { return DiceAtSlot(2, 1, 1, slot_level); }

bool HasMatchingDice(const std::vector<int>& rolls) {
  for (size_t i = 0; i < rolls.size(); ++i) {
    for (size_t j = i + 1; j < rolls.size(); ++j) {
      if (rolls[i] == rolls[j]) return true;
    }
  }
  return false;
}

int ChromaticOrbLeaps(int slot_level) { return std::max(1, slot_level); }

const std::vector<BonusActionSpell>& BonusActionSpells() {
  static const std::vector<BonusActionSpell> spells = {
      {kHealingWord, kHealingWordBonus, kFeatHealingWord, kHealingWordFeet, "Healing Word"},
      {kShieldOfFaith, kShieldOfFaithBonus, kFeatShieldOfFaith, 60, "Shield of Faith"},
      {kDivineFavor, kDivineFavorBonus, kFeatDivineFavor, 0, "Divine Favor"},
      {kSanctuary, kSanctuaryBonus, kFeatSanctuary, 30, "Sanctuary"},
      {kExpeditiousRetreat, kExpeditiousRetreatBonus, kFeatExpeditiousRetreat, 0, "Expeditious Retreat"},
      {kHuntersMark, kHuntersMarkBonus, kFeatHuntersMark, 90, "Hunter's Mark"},
  };
  return spells;
}

const BonusActionSpell* BonusActionSpellOf(int row) {
  for (const BonusActionSpell& spell : BonusActionSpells()) {
    if (spell.spell == row || spell.bonus_row == row) return &spell;
  }
  return nullptr;
}

bool IsSlotSpellBonusFeat(int feat) { return SlotSpellBonusFeatName(feat)[0] != '\0'; }

const char* SlotSpellBonusFeatName(int feat) {
  for (const BonusActionSpell& spell : BonusActionSpells()) {
    if (spell.feat == feat) return spell.name;
  }
  if (feat == kFeatExpeditiousDash) return "Expeditious Retreat: Dash";
  if (feat == kFeatHuntersMarkMove) return "Hunter's Mark: Move";
  return "";
}

int BlessTargets(int slot_level) { return 3 + std::max(0, slot_level - 1); }

int MageArmorBaseAc(int dexterity_modifier) { return 13 + dexterity_modifier; }

int HuntersMarkHours(int slot_level) { return slot_level >= 5 ? 24 : slot_level >= 3 ? 8 : 1; }

int LongstriderTargets(int slot_level) { return 1 + std::max(0, slot_level - 1); }

bool IsRitualSpell(int row) {
  const Spell2024* spell = Spell2024Of(row);
  if (spell) return spell->ritual;
  return AlarmModeOf(row) != AlarmMode::kNone || IsFamiliarCastRow(row);  // Alarm's kinds, Find Familiar's forms
}

bool CastAsRitual(bool ritual_spell, bool switch_on, bool in_combat, bool class_cast) {
  return ritual_spell && switch_on && !in_combat && class_cast;
}

bool IsFamiliarFormRow(int row) { return row >= kFamiliarBat && row <= kFamiliarWeasel; }

int FamiliarVariantRow(int form_row, int kind) {
  if (!IsFamiliarFormRow(form_row) || kind < 1 || kind > 3) return 0;
  return kFamiliarVariantFirst + (form_row - kFamiliarBat) * 3 + (kind - 1);
}

bool IsFamiliarCastRow(int row) {
  return IsFamiliarFormRow(row) || (row >= kFamiliarVariantFirst && row <= kFamiliarVariantLast);
}

int CastingMinutes(int row) {
  if (row == kFindFamiliar || IsFamiliarCastRow(row)) return 60;  // build 4c: an hour
  return AlarmModeOf(row) != AlarmMode::kNone || row == kAlarm || row == kIdentify || row == kIllusoryScript ? 1 : 0;
}

int RitualCastingMinutes(int casting_minutes) { return casting_minutes + kRitualMinutes; }

AlarmMode AlarmModeOf(int row) {
  if (row == kAlarmAudible) return AlarmMode::kAudible;
  if (row == kAlarmMental) return AlarmMode::kMental;
  return AlarmMode::kNone;
}

WaterMode WaterModeOf(int row) {
  if (row == kCreateWater) return WaterMode::kCreate;
  if (row == kDestroyWater) return WaterMode::kDestroy;
  return WaterMode::kNone;
}

int WaterCubeFeet(int slot_level) { return 30 + 5 * std::max(0, slot_level - 1); }

bool IsDetectSpell(int row) {
  return row == kDetectEvilAndGood || row == kDetectMagic || row == kDetectPoisonAndDisease;
}

const char* DetectEvilAndGoodKind(int nwn_racial_type, int nwn_good_evil) {
  if (!WardedAgainstRacialType(nwn_racial_type)) return nullptr;
  switch (nwn_racial_type) {
    case 7: return "an Aberration";
    case 16: return "an Elemental";
    case 17: return "a Fey";
    case 24: return "an Undead";
    case 20:
      if (nwn_good_evil == 5) return "a Fiend";
      if (nwn_good_evil == 4) return "a Celestial";
      return "a Celestial or a Fiend";
    default: return nullptr;
  }
}

const char* SpellSchoolName(char letter) {
  switch (letter) {
    case 'A': return "Abjuration";
    case 'C': return "Conjuration";
    case 'D': return "Divination";
    case 'E': return "Enchantment";
    case 'V': return "Evocation";
    case 'I': return "Illusion";
    case 'N': return "Necromancy";
    case 'T': return "Transmutation";
    default: return nullptr;
  }
}

const char* PoisonSenseKind(int item_property, int subtype, bool natural) {
  bool poison = false;
  bool disease = false;
  if (item_property == kItemPropertyOnHit) {
    poison = subtype == 19;
    disease = subtype == 20;
  } else if (item_property == kItemPropertyOnMonsterHit) {
    poison = subtype == 6;
    disease = subtype == 2;
  }
  if (poison) return natural ? "venomous (its natural weapons poison)" : "a poisoned weapon";
  if (disease) return "its attacks spread disease";
  return nullptr;
}

bool DetectNeedsSight(int row) { return row == kDetectMagic; }

int UtilityEffectMinutes(int row) {
  if (row == kComprehendLanguages || row == kUnseenServant) return 60;
  if (row == kSpeakWithAnimals) return 10;
  return 0;
}

int DisguiseAppearanceOf(int row) {
  switch (row) {
    case kDisguiseDwarf: return 0;
    case kDisguiseElf: return 1;
    case kDisguiseGnome: return 2;
    case kDisguiseHalfling: return 3;
    case kDisguiseHuman: return 6;
    case kDisguiseOrc: return 5;
    default: return -1;
  }
}

std::string PlayerHeadModel(bool female, int nwn_appearance, int phenotype, int head) {
  static const char kRace[] = {'d', 'e', 'g', 'a', 'h', 'o', 'h'};  // appearance.2da's RACE: D E G A H O H
  if (nwn_appearance < 0 || nwn_appearance > 6 || phenotype < 0 || phenotype > 9 || head < 0 || head > 999) return "";
  char text[24];
  snprintf(text, sizeof text, "p%c%c%d_head%03d", female ? 'f' : 'm', kRace[nwn_appearance], phenotype, head);
  return text;
}

bool SilentImageMoveAllowed(float feet_from_caster) {
  return feet_from_caster <= static_cast<float>(kSilentImageRangeFeet);
}

bool IsNoSlotOption(int row) { return row == kSilentImageMove || row == kFamiliarDismiss || row == kFamiliarRecall; }

int ChoiceOptionFeat(int row) {
  switch (row) {
    case kSilentImageMove: return kFeatSilentImageMove;
    case kFamiliarDismiss: return kFeatFamiliarDismiss;  // build 4c
    case kFamiliarRecall: return kFeatFamiliarRecall;
    default: return 0;
  }
}

bool WizardOnlySpell(int row) {
  return row == kAlarm || AlarmModeOf(row) != AlarmMode::kNone || row == kIdentify || row == kIllusoryScript ||
         row == kUnseenServant || row == kFindFamiliar || IsFamiliarCastRow(row);  // build 4c: Find Familiar
}

bool WardedAgainstRacialType(int nwn_racial_type) {
  // racialtypes.2da: 7 Aberration, 16 Elemental, 17 Fey, 20 Outsider, 24 Undead.
  return nwn_racial_type == 7 || nwn_racial_type == 16 || nwn_racial_type == 17 || nwn_racial_type == 20 ||
         nwn_racial_type == 24;
}

bool SanctuaryStopsOwnAttacks(bool on_caster, bool caster_is_player) { return on_caster && caster_is_player; }

MarkMove HuntersMarkMoveNow(bool quarry_down, bool keep_armed, bool other_creature_picked) {
  if (quarry_down) return MarkMove::kMove;
  return keep_armed && other_creature_picked ? MarkMove::kArm : MarkMove::kRefuse;
}

CommandWord CommandWordOf(int row) {
  switch (row) {
    case kCommandDrop: return CommandWord::kDrop;
    case kCommandFlee: return CommandWord::kFlee;
    case kCommandGrovel: return CommandWord::kGrovel;
  }
  return CommandWord::kHalt;
}

const char* CommandWordName(CommandWord word) {
  switch (word) {
    case CommandWord::kDrop: return "Drop";
    case CommandWord::kFlee: return "Flee";
    case CommandWord::kGrovel: return "Grovel";
    case CommandWord::kHalt: return "Halt";
  }
  return "Halt";
}

int ConditionSpellTargets(int slot_level) { return 1 + std::max(0, slot_level - 1); }

int ConditionSpellRangeFeet(int row) {
  switch (row) {
    case kCommand:
    case kSleep: return 60;
    case kCharmPerson:
    case kAnimalFriendship:
    case kHideousLaughter: return 30;
  }
  return 0;
}

int ConditionSpellRounds(int row) {
  switch (row) {
    case kCommand: return 1;
    case kColorSpray: return 2;
    case kHideousLaughter:
    case kSleep: return 10;
  }
  return 0;
}

int ConditionSpellHours(int row) { return row == kCharmPerson ? 1 : row == kAnimalFriendship ? 24 : 0; }

bool IsHumanoidRacialType(int nwn_racial_type) {
  // racialtypes.2da: 0-6 the player races, 12 Humanoid_Goblinoid, 14 Humanoid_Orc, 15 Humanoid_Reptilian (13,
  // Humanoid_Monstrous, is the 2024 Monstrosity).
  return (nwn_racial_type >= 0 && nwn_racial_type <= 6) || nwn_racial_type == 12 || nwn_racial_type == 14 ||
         nwn_racial_type == 15;
}

bool IsBeastRacialType(int nwn_racial_type) {
  return nwn_racial_type == 8 || nwn_racial_type == 9 || nwn_racial_type == 25;  // Animal, Beast, Vermin
}

bool ConditionSpellTargetFits(int row, int nwn_racial_type) {
  if (row == kCharmPerson) return IsHumanoidRacialType(nwn_racial_type);
  if (row == kAnimalFriendship) return IsBeastRacialType(nwn_racial_type);
  return true;
}

bool CharmSaveAdvantage(bool enemy, bool fighting) { return enemy && fighting; }

bool LaughterSaveAdvantage(bool after_damage) { return after_damage; }

SleepStage SleepAfterSecondSave(bool saved) { return saved ? SleepStage::kAwake : SleepStage::kUnconscious; }

bool SleepUnaffectedRacialType(int nwn_racial_type) {
  // 1 Elf, 10 Construct, 16 Elemental, 24 Undead, 29 Ooze (a half-elf sleeps).
  return nwn_racial_type == 1 || nwn_racial_type == 10 || nwn_racial_type == 16 || nwn_racial_type == 24 ||
         nwn_racial_type == 29;
}

bool PickedAgainst(bool enemy, bool charmed_by_caster) { return enemy && !charmed_by_caster; }

float DifficultTerrainFactor(float factor, bool inside) { return inside ? factor / 2.0f : factor; }

FaerieColour FaerieColourOf(int row) {
  if (row == kFaerieFireGreen) return FaerieColour::kGreen;
  if (row == kFaerieFireViolet) return FaerieColour::kViolet;
  return FaerieColour::kBlue;
}

const char* FaerieColourName(FaerieColour colour) {
  switch (colour) {
    case FaerieColour::kGreen: return "Green";
    case FaerieColour::kViolet: return "Violet";
    case FaerieColour::kBlue: return "Blue";
  }
  return "Blue";
}

int InvisibilityLeftSeconds(int left_when_suppressed, int seconds_suppressed) {
  if (left_when_suppressed < 0) return -1;
  return std::max(0, left_when_suppressed - std::max(0, seconds_suppressed));
}

int FogCloudRadiusFeet(int slot_level) { return 20 + 20 * std::max(0, slot_level - 1); }

bool ShieldTurnsHit(int total, int armor_class, bool natural_twenty) {
  return !natural_twenty && total < armor_class + kShieldAc;
}

bool IsSmiteSpell(int row) { return row == kDivineSmite || row == kSearingSmite || row == kEnsnaringStrike; }

int SmiteArmedIconRow(int row) {
  switch (row) {
    case kDivineSmite: return kSmiteArmedIconFirst;
    case kSearingSmite: return kSmiteArmedIconFirst + 1;
    case kEnsnaringStrike: return kSmiteArmedIconLast;
    default: return 0;
  }
}

bool SmiteRidesOn(int row, bool ranged, bool unarmed) {
  if (row == kEnsnaringStrike) return !unarmed;
  if (row == kDivineSmite || row == kSearingSmite) return !ranged;
  return false;
}

bool FiendOrUndead(int nwn_racial_type, bool evil) {
  return nwn_racial_type == 24 || (nwn_racial_type == 20 && evil);  // racialtypes.2da: Undead, Outsider
}

int DivineSmiteDice(int slot_level, bool fiend_or_undead) {
  return 2 + std::max(0, slot_level - 1) + (fiend_or_undead ? 1 : 0);
}

int SearingSmiteDice(int slot_level) { return 1 + std::max(0, slot_level - 1); }

int EnsnaringStrikeDice(int slot_level) { return 1 + std::max(0, slot_level - 1); }

bool EnsnaringSaveAdvantage(int nwn_creature_size) { return nwn_creature_size >= 4; }  // CREATURE_SIZE_LARGE

int AreaSpellRangeFeet(int row) {
  switch (row) {
    case kEntangle: return 90;
    case kGrease:
    case kFaerieFire: return 60;
    case kFogCloud: return 120;
  }
  return 0;
}

int HeroismTargets(int slot_level) { return 1 + std::max(0, slot_level - 1); }

int HeroismTemporaryHitPoints(int modifier) { return std::max(0, modifier); }

bool SameTurn(uint32_t rounds_then, uint32_t rounds_now, long long ms_since) {
  return rounds_then == rounds_now && ms_since >= 0 && ms_since < 6000;
}

bool InCone(float along, float side, float length, float slack) {
  if (along < -slack || along > length + slack) return false;
  // 2024's Cone: as wide as it is far from its point of origin - half that to each side.
  return std::fabs(side) <= std::max(0.0f, along) / 2.0f + slack;
}

bool InCubeFromCaster(float along, float side, float edge, float slack) {
  if (along < -slack || along > edge + slack) return false;
  return std::fabs(side) <= edge / 2.0f + slack;
}

}  // namespace rules
