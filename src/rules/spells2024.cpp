#include "rules/spells2024.h"

#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <iterator>

#include "rules/armor.h"
#include "rules/cantrips.h"
#include "rules/core.h"

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
    // Build 2a's buffs (not Healing Word, nor level 2's Bonus Action spells: build 1b's, below).
    if (bonus.spell == kHealingWord || bonus.spell == kSpiritualWeapon || bonus.spell == kFlameBlade ||
        bonus.spell == kDragonsBreath) {
      continue;
    }
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
  spells.push_back(utility(kFloatingDisk, "Floating Disk", "n5_s_floatdisk", true));  // build 4d
  // #39 Part 2, level 2, build 1a: damage and healing.
  {
    const auto level2 = [](Spell2024 spell) {
      spell.level = 2;
      return spell;
    };
    spells.push_back(level2(Damage(kAcidArrow, "Acid Arrow", "n5_s_acidarrow", E::kSpellAttack, 4, 4, 1,
                                   kDamageTypeAcid)));
    spells.push_back(level2(Area(Saved(Damage(kShatter, "Shatter", "n5_s_shatter", E::kSave, 3, 8, 1, kDamageTypeThunder),
                                       Ability::kConstitution, true),
                                 SpellShape2024::kSphere, kShatterFeet)));
    spells.push_back(level2(Saved(Damage(kMindSpike, "Mind Spike", "n5_s_mindspike", E::kSave, 3, 8, 1,
                                         kDamageTypePsychic),
                                  Ability::kWisdom, true)));
    Spell2024 shining = level2(Damage(kShiningSmite, "Shining Smite", "n5_s_shinesmite", E::kSmite, 2, 6, 1,
                                      kDamageTypeRadiant));
    shining.bonus_action = true;  // paid by the hit (build 3's Q1 A)
    spells.push_back(shining);
    spells.push_back(level2(Damage(kPrayerOfHealing, "Prayer of Healing", "n5_s_prayheal", E::kHealing, 2, 8, 1, 0)));
    // #171: Scorching Ray - each ray 2d6 Fire; the slot adds rays, not dice (upcast_dice 0).
    spells.push_back(level2(Damage(kScorchingRay, "Scorching Ray", "n5_s_scorchray", E::kSpellAttack, 2, 6, 0,
                                   kDamageTypeFire)));
    // Build 1b: Spiritual Weapon and Flame Blade (melee spell attacks, + the spellcasting ability modifier), Dragon's
    // Breath (the breath's 15-foot Cone, its type the menu's), Heat Metal (its damage; the save is the drop's).
    Spell2024 weapon = level2(Touch(Damage(kSpiritualWeapon, "Spiritual Weapon", "n5_s_spiritweap", E::kSpellAttack, 1,
                                           8, 1, kDamageTypeForce)));
    weapon.bonus_action = true;
    spells.push_back(weapon);
    Spell2024 blade = level2(Touch(Damage(kFlameBlade, "Flame Blade", "n5_s_flameblade", E::kSpellAttack, 3, 6, 1,
                                          kDamageTypeFire)));
    blade.bonus_action = true;
    spells.push_back(blade);
    Spell2024 breath = level2(Area(Saved(Damage(kDragonsBreath, "Dragon's Breath", "n5_s_dragbreath", E::kSave, 3, 6, 1,
                                                kDamageTypeFire),
                                         Ability::kDexterity, true),
                                   SpellShape2024::kCone, kDragonsBreathConeFeet));
    breath.bonus_action = true;
    spells.push_back(breath);
    spells.push_back(level2(Saved(Damage(kHeatMetal, "Heat Metal", "n5_s_heatmetal", E::kSave, 2, 8, 1, kDamageTypeFire),
                                  Ability::kConstitution, false)));
  }
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
      // #39 level 2 build 1b.
      {kSpiritualWeapon, kSpiritualWeaponBonus, kFeatSpiritualWeapon, kSpiritualWeaponFeet, "Spiritual Weapon"},
      {kFlameBlade, kFlameBladeBonus, kFeatFlameBlade, 0, "Flame Blade"},
      {kDragonsBreath, kDragonsBreathBonus, kFeatDragonsBreath, kDragonsBreathTouchFeet, "Dragon's Breath"},
  };
  return spells;
}

const BonusActionSpell* BonusActionSpellOf(int row) {
  // #39 level 2 build 1b: a menu's row (Dragon's Breath's types) is its master's.
  const ChoiceCantrip* menu = ChoiceOptionOf(row) ? ChoiceCantripOf(row) : nullptr;
  const int spell_row = menu ? menu->master : row;
  for (const BonusActionSpell& spell : BonusActionSpells()) {
    if (spell.spell == spell_row || spell.bonus_row == row) return &spell;
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
  if (feat == kFeatSpiritualWeaponStrike) return "Spiritual Weapon: Strike";  // #39 level 2 build 1b
  if (feat == kFeatFlameBladeEvoke) return "Flame Blade: Evoke";
  if (feat == kFeatHeatMetalAgain) return "Heat Metal: Again";
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
  if (row == kPrayerOfHealing) return kPrayerOfHealingMinutes;      // level 2 build 1a
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

std::string PlayerModelPrefix(bool female, int nwn_appearance, int phenotype) {
  static const char kRace[] = {'d', 'e', 'g', 'a', 'h', 'o', 'h'};  // appearance.2da's RACE: D E G A H O H
  if (nwn_appearance < 0 || nwn_appearance > 6 || phenotype < 0 || phenotype > 9) return "";
  char text[8];
  snprintf(text, sizeof text, "p%c%c%d_", female ? 'f' : 'm', kRace[nwn_appearance], phenotype);
  return text;
}

std::string PlayerHeadModel(bool female, int nwn_appearance, int phenotype, int head) {
  const std::string prefix = PlayerModelPrefix(female, nwn_appearance, phenotype);
  if (prefix.empty() || head < 0 || head > 999) return "";
  char text[8];
  snprintf(text, sizeof text, "%03d", head);
  return prefix + "head" + text;
}

const std::vector<int>& DisguiseSkinColours(int nwn_appearance) {
  // pal_skin01 rows the game's people of each species wear (their blueprints'), with the common natural ones.
  static const std::vector<int> kDwarf{1, 2, 3, 13, 18};
  static const std::vector<int> kElf{1, 2, 13, 29, 30};
  static const std::vector<int> kSmall{1, 2, 3, 13, 16};  // gnome, halfling
  static const std::vector<int> kOrc{30, 41};
  static const std::vector<int> kHuman{1, 2, 3, 13, 16, 24, 44, 47};
  static const std::vector<int> kNone;
  switch (nwn_appearance) {
    case 0: return kDwarf;
    case 1: return kElf;
    case 2:
    case 3: return kSmall;
    case 5: return kOrc;
    case 4:
    case 6: return kHuman;
    default: return kNone;
  }
}

const std::vector<int>& DisguiseHairColours(int nwn_appearance) {
  static const std::vector<int> kDwarf{1, 4, 5, 16, 17, 18, 20};
  static const std::vector<int> kElf{1, 3, 4, 5, 16, 23};
  static const std::vector<int> kSmall{1, 3, 4, 5, 16, 17, 18, 20};  // gnome, halfling
  static const std::vector<int> kOrc{1, 4, 18, 53};
  static const std::vector<int> kHuman{1, 3, 4, 5, 16, 17, 18, 20, 23, 46};
  static const std::vector<int> kNone;
  switch (nwn_appearance) {
    case 0: return kDwarf;
    case 1: return kElf;
    case 2:
    case 3: return kSmall;
    case 5: return kOrc;
    case 4:
    case 6: return kHuman;
    default: return kNone;
  }
}

const std::vector<ArmourPiece>& DisguiseArmourPieces() {
  // ITEM_APPR_ARMOR_MODEL_*: 0/1 feet, 2/3 shins, 5/4 thighs, 6 pelvis, 7 torso, 8 belt, 9 neck, 10/11 forearms,
  // 12/13 biceps, 14/15 shoulders, 16/17 hands (the right side first, the left alike).
  static const std::vector<ArmourPiece> kPieces{
      {kArmourTorso, -1, "parts_chest", "chest"}, {6, -1, "parts_pelvis", "pelvis"},
      {8, -1, "parts_belt", "belt"},                {9, -1, "parts_neck", "neck"},
      {0, 1, "parts_foot", "footr"},                {2, 3, "parts_shin", "shinr"},
      {5, 4, "parts_legs", "legr"},                 {10, 11, "parts_forearm", "forer"},
      {12, 13, "parts_bicep", "bicepr"},            {14, 15, "parts_shoulder", "shor"},
      {16, 17, "parts_hand", "handr"}};
  return kPieces;
}

bool SilentImageMoveAllowed(float feet_from_caster) {
  return feet_from_caster <= static_cast<float>(kSilentImageRangeFeet);
}

bool IsNoSlotOption(int row) {
  return row == kSilentImageMove || row == kFamiliarDismiss || row == kFamiliarRecall ||
         row == kHeatMetalAgain;  // #39 level 2 build 1b: Heat Metal's menu
}

int ChoiceOptionFeat(int row) {
  switch (row) {
    case kSilentImageMove: return kFeatSilentImageMove;
    case kFamiliarDismiss: return kFeatFamiliarDismiss;  // build 4c
    case kFamiliarRecall: return kFeatFamiliarRecall;
    case kHeatMetalAgain: return kFeatHeatMetalAgain;  // #39 level 2 build 1b: in Heat Metal's menu while it lasts
    default: return 0;
  }
}

const std::vector<int>& WizardOnlyRows() {
  static const std::vector<int> kRows{kIdentify,        kRayOfEnfeeblement, kProtectionFromEvilAndGood,
                                      kHideousLaughter, kLongstrider,       kAlarm,
                                      kIllusoryScript,  kUnseenServant,     kFindFamiliar,
                                      kFloatingDisk,    kAcidArrow};
  return kRows;
}

bool WizardOnlySpell(int row) {
  const std::vector<int>& rows = WizardOnlyRows();
  return std::find(rows.begin(), rows.end(), row) != rows.end() || AlarmModeOf(row) != AlarmMode::kNone ||
         IsFamiliarCastRow(row);  // build 4c: Find Familiar's forms and kinds
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

bool IsSmiteSpell(int row) {
  return row == kDivineSmite || row == kSearingSmite || row == kEnsnaringStrike || row == kShiningSmite;
}

int SmiteArmedIconRow(int row) {
  switch (row) {
    case kDivineSmite: return kSmiteArmedIconFirst;
    case kSearingSmite: return kSmiteArmedIconFirst + 1;
    case kEnsnaringStrike: return kSmiteArmedIconLast;
    case kShiningSmite: return kShiningSmiteArmedIcon;
    default: return 0;
  }
}

bool SmiteRidesOn(int row, bool ranged, bool unarmed) {
  if (row == kEnsnaringStrike) return !unarmed;
  if (row == kDivineSmite || row == kSearingSmite || row == kShiningSmite) return !ranged;
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

// #39 Part 2, level 2, build 1a.
int AcidArrowMissDamage(int initial_roll) { return initial_roll > 0 ? initial_roll / 2 : 0; }

int AcidArrowLaterDice(int slot_level) { return DiceAtSlot(2, 1, 2, slot_level); }

int ShiningSmiteDice(int slot_level) { return DiceAtSlot(2, 1, 2, slot_level); }

bool ShatterSaveDisadvantage(int nwn_racial_type) { return nwn_racial_type == 10; }  // racialtypes.2da: Construct

bool IsSmiteArmedIcon(int icon_row) {
  return (icon_row >= kSmiteArmedIconFirst && icon_row <= kSmiteArmedIconLast) || icon_row == kShiningSmiteArmedIcon;
}

std::vector<uint32_t> PrayerOfHealingTargetsOf(uint32_t caster, std::vector<PrayerCandidate> candidates) {
  const float reach = FeetToMetres(kPrayerOfHealingFeet);
  std::stable_sort(candidates.begin(), candidates.end(), [caster](const PrayerCandidate& a, const PrayerCandidate& b) {
    if ((a.id == caster) != (b.id == caster)) return a.id == caster;
    return a.metres < b.metres;
  });
  std::vector<uint32_t> picked;
  for (const PrayerCandidate& one : candidates) {
    if (static_cast<int>(picked.size()) >= kPrayerOfHealingTargets) break;
    if (one.prayed || (one.id != caster && one.metres > reach)) continue;
    if (std::find(picked.begin(), picked.end(), one.id) != picked.end()) continue;
    picked.push_back(one.id);
  }
  return picked;
}

// #39 Part 2, level 2, build 1b.
bool SpiritualWeaponReaches(float metres_from_force, float slack) {
  return metres_from_force <= FeetToMetres(kSpiritualWeaponMoveFeet + kSpiritualWeaponReachFeet) + slack;
}

ForceOrder SpiritualWeaponOrder(bool at_creature, float metres, float slack) {
  if (at_creature) return SpiritualWeaponReaches(metres, slack) ? ForceOrder::kStrike : ForceOrder::kTooFar;
  return metres <= FeetToMetres(kSpiritualWeaponMoveFeet) + slack ? ForceOrder::kMove : ForceOrder::kTooFar;
}

float SpiritualWeaponStrikePoint(float force_x, float force_y, float target_x, float target_y, float stand_off,
                                 float* x, float* y) {
  const float dx = target_x - force_x;
  const float dy = target_y - force_y;
  const float distance = std::sqrt(dx * dx + dy * dy);
  if (distance <= stand_off) {
    *x = force_x;
    *y = force_y;
    return 0.0f;
  }
  const float along = (distance - stand_off) / distance;
  *x = force_x + dx * along;
  *y = force_y + dy * along;
  return distance - stand_off;
}

float SpiritualWeaponMoveSeconds(float metres) {
  return metres > 0.0f ? metres / kSpiritualWeaponMetresPerSecond : 0.0f;
}

bool SpiritualWeaponFlies(float metres) { return metres >= kSpiritualWeaponLeastFlightMetres; }

int SpiritualWeaponLandingMs(float metres) {
  return kSpiritualWeaponLandingSlackMs + static_cast<int>(SpiritualWeaponMoveSeconds(metres) * 1000.0f);
}

float SpiritualWeaponFacing(float x, float y, float to_x, float to_y) {
  const float dx = to_x - x;
  const float dy = to_y - y;
  if (dx * dx + dy * dy < 0.0001f) return 0.0f;
  float degrees = std::atan2(dy, dx) * 57.29578f;
  if (degrees < 0.0f) degrees += 360.0f;
  return degrees;
}

bool SpiritualWeaponStrikesAtOnce(bool at_creature, bool at_caster) { return at_creature && !at_caster; }

bool IsDragonsBreathRow(int row) {
  return row == kDragonsBreath || row == kDragonsBreathBonus || (row >= kDragonsBreathAcid && row <= kDragonsBreathPoison);
}

namespace {
constexpr uint32_t kBreathTypes[] = {kDamageTypeAcid, kDamageTypeCold, kDamageTypeFire, kDamageTypeLightning,
                                     kDamageTypePoison};
constexpr int kBreathTypeCount = static_cast<int>(sizeof(kBreathTypes) / sizeof(kBreathTypes[0]));
int BreathTypeIndex(uint32_t damage_type) {
  for (int i = 0; i < kBreathTypeCount; ++i) {
    if (kBreathTypes[i] == damage_type) return i;
  }
  return -1;
}
}  // namespace

uint32_t DragonsBreathType(int row) {
  if (row >= kDragonsBreathAcid && row <= kDragonsBreathPoison) return kBreathTypes[row - kDragonsBreathAcid];
  if (IsDragonsBreathExhaleRow(row)) return kBreathTypes[row - kDragonsBreathExhaleFirst];
  return row == kDragonsBreath || row == kDragonsBreathBonus ? kDamageTypeFire : 0;
}

int DragonsBreathExhaleRow(uint32_t damage_type) {
  const int i = BreathTypeIndex(damage_type);
  return i < 0 ? -1 : kDragonsBreathExhaleFirst + i;
}

int DragonsBreathExhaleFeat(uint32_t damage_type) {
  const int i = BreathTypeIndex(damage_type);
  return i < 0 ? -1 : kFeatDragonsBreathExhaleFirst + i;
}

bool IsDragonsBreathExhaleRow(int row) { return row >= kDragonsBreathExhaleFirst && row <= kDragonsBreathExhaleLast; }

bool IsDragonsBreathExhaleFeat(int feat) {
  return feat >= kFeatDragonsBreathExhaleFirst && feat <= kFeatDragonsBreathExhaleLast;
}

bool MetalWeaponBaseItem(int base_item) {
  // baseitems.2da: 0 Shortsword, 1 Longsword, 2 Battleaxe, 3 Bastard Sword, 4 Light Flail, 5 Warhammer, 9 Light Mace,
  // 10 Halberd, 12 Two-Bladed Sword, 13 Greatsword, 18 Greataxe, 22 Dagger, 32 Dire Mace, 33 Double Axe, 35 Heavy
  // Flail, 37 Light Hammer, 38 Handaxe, 40 Kama, 41 Katana, 42 Kukri, 47 Morningstar, 51 Rapier, 53 Scimitar, 55
  // Scythe, 58 Spear, 59 Shuriken, 60 Sickle, 63 Throwing Axe, 78 Dwarven Waraxe, 95 Trident. Not: bows, crossbows,
  // the club, the quarterstaff, the sling, darts, the whip, staves, rods, wands, shields, creature weapons.
  static const int kMetal[] = {0, 1, 2, 3, 4, 5, 9, 10, 12, 13, 18, 22, 32, 33, 35, 37, 38, 40, 41, 42, 47, 51, 53, 55,
                               58, 59, 60, 63, 78, 95};
  return std::find(std::begin(kMetal), std::end(kMetal), base_item) != std::end(kMetal);
}

bool MetalArmourTier(int nwn_base_ac) {
  // armor.h: the Medium and Heavy tiers (4-8) are all metal - Scale Mail, Chain Mail, Splint, Half Plate, Plate.
  const ArmorCategory category = ArmorForNwnTier(nwn_base_ac).category;
  return category == ArmorCategory::kMedium || category == ArmorCategory::kHeavy;
}

HeatedObject HeatMetalObject(int right_hand_base_item, int left_hand_base_item, int armour_tier) {
  if (MetalWeaponBaseItem(right_hand_base_item)) return HeatedObject::kRightHand;
  if (MetalWeaponBaseItem(left_hand_base_item)) return HeatedObject::kLeftHand;
  if (MetalArmourTier(armour_tier)) return HeatedObject::kArmour;
  return HeatedObject::kNone;
}

HeatOutcome HeatMetalOutcome(HeatedObject object, int damage, bool save_failed) {
  if (object == HeatedObject::kNone || damage <= 0) return HeatOutcome::kNothing;
  // "drop the object if it can": a weapon, yes; armour can't be, nor a strapped shield as the burn lands (2024:
  // doffing a shield takes the Utilize action) - it keeps hold.
  const bool can_drop = object == HeatedObject::kRightHand || object == HeatedObject::kLeftHand;
  return save_failed && can_drop ? HeatOutcome::kDrops : HeatOutcome::kDisadvantage;
}

HeatChoice HeatMetalChoiceOf(int row) {
  switch (row) {
    case kHeatMetalWeapon: return HeatChoice::kWeapon;
    case kHeatMetalShield: return HeatChoice::kShield;
    case kHeatMetalArmour: return HeatChoice::kArmour;
    default: return HeatChoice::kAny;
  }
}

bool MetalShieldBaseItem(int base_item) { return base_item == 14 || base_item == 56 || base_item == 57; }

HeatedObject HeatMetalObjectFor(HeatChoice choice, int right_hand_base_item, int left_hand_base_item, int armour_tier) {
  switch (choice) {
    case HeatChoice::kWeapon:
      if (MetalWeaponBaseItem(right_hand_base_item)) return HeatedObject::kRightHand;
      return MetalWeaponBaseItem(left_hand_base_item) ? HeatedObject::kLeftHand : HeatedObject::kNone;
    case HeatChoice::kShield:
      return MetalShieldBaseItem(left_hand_base_item) ? HeatedObject::kShield : HeatedObject::kNone;
    case HeatChoice::kArmour:
      return MetalArmourTier(armour_tier) ? HeatedObject::kArmour : HeatedObject::kNone;
    case HeatChoice::kAny:
      break;
  }
  const HeatedObject first = HeatMetalObject(right_hand_base_item, left_hand_base_item, armour_tier);
  if (first != HeatedObject::kNone) return first;
  return MetalShieldBaseItem(left_hand_base_item) ? HeatedObject::kShield : HeatedObject::kNone;
}

AfterDrop HeatMetalAfterDrop(bool carries_other_weapon, int intelligence, bool spell_lasts) {
  if (carries_other_weapon) return AfterDrop::kDrawOther;
  if (spell_lasts && intelligence >= kHeatMetalWaryIntelligence) return AfterDrop::kLeave;
  return AfterDrop::kPickUp;
}

bool WeaponBaseItem(int base_item) {
  // Besides the metal ones: 6 heavy crossbow, 7 light crossbow, 8 longbow, 11 shortbow, 28 club, 31 dart, 50
  // quarterstaff, 61 sling, 108 whip.
  static const int kOther[] = {6, 7, 8, 11, 28, 31, 50, 61, 108};
  return MetalWeaponBaseItem(base_item) ||
         std::find(std::begin(kOther), std::end(kOther), base_item) != std::end(kOther);
}

int DamageTakenOf(int heard, int lost) {
  if (heard >= 0) return heard;
  return lost > 0 ? lost : 0;
}

int NpcBonusRepeatChance(int intelligence, int wisdom, int base, int per_mod) {
  const int modifier = std::max(AbilityModifier(intelligence), AbilityModifier(wisdom));
  return std::min(kNpcBonusRepeatMost, std::max(kNpcBonusRepeatLeast, base + per_mod * modifier));
}

bool NpcBonusRepeats(int chance, int d100) { return d100 >= 1 && d100 <= chance; }

TouchReach BonusSpellTouchReach(int feet, bool out_of_reach, bool walked) {
  if (!out_of_reach) return TouchReach::kCast;
  const bool touch = feet > 0 && feet <= kDragonsBreathTouchFeet;  // within 5 feet: a touch
  return touch && !walked ? TouchReach::kWalk : TouchReach::kTooFar;
}

HeatAgain HeatMetalAgainNow(bool cast_this_turn, bool held_or_worn, float metres, float slack) {
  if (cast_this_turn) return HeatAgain::kCastThisTurn;
  if (!held_or_worn) return HeatAgain::kNotHeld;
  if (metres > FeetToMetres(kHeatMetalFeet) + slack) return HeatAgain::kOutOfRange;
  return HeatAgain::kOk;
}

const std::vector<int>& SorcererOnlyRows() {
  static const std::vector<int> kRows{kSorcerousBurst, kFlameBlade};
  return kRows;
}

bool SorcererOnlySpell(int row) {
  const std::vector<int>& rows = SorcererOnlyRows();
  return std::find(rows.begin(), rows.end(), row) != rows.end();
}

const SpellActionLeaf* SpellActionLeafOf(int spell) {
  static const SpellActionLeaf kLeaves[] = {
      {kHuntersMark, kFeatHuntersMarkMove, "Hunter's Mark", "Hunter's Mark: Move"},
      {kSpiritualWeapon, kFeatSpiritualWeaponStrike, "Spiritual Weapon", "Spiritual Weapon: Strike"},
      {kFlameBlade, kFeatFlameBladeEvoke, "Flame Blade", "Flame Blade: Evoke"},
  };
  for (const SpellActionLeaf& leaf : kLeaves) {
    if (leaf.spell == spell) return &leaf;
  }
  return nullptr;
}

BladeHand FlameBladeHand(bool in_hand, bool seen_held, uint64_t ms_since_made, uint64_t grace_ms) {
  if (in_hand) return BladeHand::kHeld;
  if (!seen_held && ms_since_made < grace_ms) return BladeHand::kOnItsWay;
  return BladeHand::kLetGo;
}

bool FlameBladePutsBack(bool had_old_weapon, bool hand_holds_blade, bool hand_empty) {
  return had_old_weapon && (hand_holds_blade || hand_empty);
}

bool IsBonusActionSpellOrder(int row) {
  const BonusActionSpell* bonus = BonusActionSpellOf(row);
  return bonus && row != bonus->bonus_row;
}

}  // namespace rules
