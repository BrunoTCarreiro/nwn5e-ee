#include "rules/cantrips.h"

#include "rules/spells2024.h"

#include <stdio.h>

#include <algorithm>
#include <cmath>

namespace rules {

namespace {

// SRD 5.2.1's cantrips (CC-BY-4.0): the die, the damage type and how it reaches its target.
const std::vector<Cantrip> kCantrips = {
    {100, "n5_c_light", 0, 0, CantripRoll::kNone},                                  // Light (NWN's row)
    {144, "n5_c_rayfrost", 8, kDamageTypeCold, CantripRoll::kRangedSpellAttack},   // Ray of Frost (NWN's row)
    {424, "n5_c_acidsplash", 6, kDamageTypeAcid, CantripRoll::kDexteritySave},     // Acid Splash (NWN's row)
    {1050, "n5_c_firebolt", 10, kDamageTypeFire, CantripRoll::kRangedSpellAttack},
    {1051, "n5_c_chilltouch", 10, kDamageTypeNecrotic, CantripRoll::kMeleeSpellAttack},
    {1052, "n5_c_shockgrasp", 8, kDamageTypeLightning, CantripRoll::kMeleeSpellAttack},
    {1053, "n5_c_poisonspray", 12, kDamageTypePoison, CantripRoll::kRangedSpellAttack},
    {1054, "n5_c_sacredflame", 8, kDamageTypeRadiant, CantripRoll::kDexteritySave},
    {1055, "n5_c_starrywisp", 8, kDamageTypeRadiant, CantripRoll::kRangedSpellAttack},
    {1056, "n5_c_mockery", 6, kDamageTypePsychic, CantripRoll::kWisdomSave},
};

// Daze 37, Virtue 189, Cure Minor Wounds 33, Flare 416, Inflict Minor Wounds 431, Electric Jolt 439.
const std::vector<int> kRetired = {33, 37, 189, 416, 431, 439};

}  // namespace

const std::vector<Cantrip>& Cantrips() { return kCantrips; }

const Cantrip* CantripOf(int spell) {
  const auto at = std::lower_bound(kCantrips.begin(), kCantrips.end(), spell,
                                   [](const Cantrip& cantrip, int id) { return cantrip.spell < id; });
  return at != kCantrips.end() && at->spell == spell ? &*at : nullptr;
}

int CantripDice(int character_level) {
  return 1 + (character_level >= 5 ? 1 : 0) + (character_level >= 11 ? 1 : 0) + (character_level >= 17 ? 1 : 0);
}

int CantripDiceRolled(int character_level, bool critical) {
  return CantripDice(character_level) * (critical ? 2 : 1);
}

const std::vector<int>& RetiredCantrips() { return kRetired; }

bool IsRetiredCantrip(int spell) { return std::binary_search(kRetired.begin(), kRetired.end(), spell); }

namespace {

// SRD 5.2.1 (CC-BY-4.0): Resistance's damage types, Guidance's skills, Sorcerous Burst's types, in its order.
std::vector<ChoiceCantrip> BuildChoiceCantrips() {
  std::vector<ChoiceCantrip> all;
  ChoiceCantrip resistance{kResistance, "n5_c_resist", ChoiceKind::kDamageType,
                           {{1060, kDamageTypeAcid, "Acid"},
                            {1061, 1, "Bludgeoning"},
                            {1062, kDamageTypeCold, "Cold"},
                            {1063, kDamageTypeFire, "Fire"},
                            {1064, kDamageTypeLightning, "Lightning"},
                            {1065, kDamageTypeNecrotic, "Necrotic"},
                            {1066, 2, "Piercing"},
                            {1067, kDamageTypePoison, "Poison"},
                            {1068, kDamageTypeRadiant, "Radiant"},
                            {1069, 4, "Slashing"},
                            {1070, kDamageTypeThunder, "Thunder"}},
                           130};
  all.push_back(resistance);
  ChoiceCantrip guidance{kGuidance, "n5_c_guidance", ChoiceKind::kSkill, {}, 142};
  static const char* const kSkillLabels[] = {
      "Acrobatics", "Animal Handling", "Arcana",       "Athletics",  "Deception", "History",
      "Insight",    "Intimidation",    "Investigation", "Medicine",   "Nature",    "Perception",
      "Performance", "Persuasion",     "Religion",     "Sleight of Hand", "Stealth", "Survival"};
  for (int skill = 0; skill < 18; ++skill) {
    guidance.options.push_back({1071 + skill, static_cast<uint32_t>(skill), kSkillLabels[skill]});
  }
  all.push_back(guidance);
  ChoiceCantrip burst{kSorcerousBurst, "n5_c_sorcburst", ChoiceKind::kDamageType,
                      {{1089, kDamageTypeAcid, "Acid"},
                       {1090, kDamageTypeCold, "Cold"},
                       {1091, kDamageTypeFire, "Fire"},
                       {1092, kDamageTypeLightning, "Lightning"},
                       {1093, kDamageTypePoison, "Poison"},
                       {1094, kDamageTypePsychic, "Psychic"},
                       {1095, kDamageTypeThunder, "Thunder"}}};
  all.push_back(burst);
  // #39 2a build 1b: Chromatic Orb (a level 1 spell) the same way - its six types, each opening its slot levels.
  ChoiceCantrip orb{kChromaticOrb, "n5_s_chromorb", ChoiceKind::kDamageType,
                   {{kChromaticOrbAcid, kDamageTypeAcid, "Acid"},
                    {kChromaticOrbCold, kDamageTypeCold, "Cold"},
                    {kChromaticOrbFire, kDamageTypeFire, "Fire"},
                    {kChromaticOrbLightning, kDamageTypeLightning, "Lightning"},
                    {kChromaticOrbPoison, kDamageTypePoison, "Poison"},
                    {kChromaticOrbThunder, kDamageTypeThunder, "Thunder"}}};
  all.push_back(orb);
  ChoiceCantrip flame{kProduceFlame, "n5_c_produceflame", ChoiceKind::kMode,
                     {{kProduceFlameFlame, 0, "Flame"}, {kProduceFlameHurl, 1, "Hurl"}}};
  all.push_back(flame);
  // Build 3b: the damage type of the weapon cantrips (decision 3 A): theirs, or the weapon's own (0).
  ChoiceCantrip shillelagh{kShillelagh, "n5_c_shillelagh", ChoiceKind::kDamageType,
                           {{kShillelaghForce, kDamageTypeForce, "Force"}, {kShillelaghWeapon, 0, "Bludgeoning"}},
                           161};
  all.push_back(shillelagh);
  ChoiceCantrip true_strike{kTrueStrike, "n5_c_truestrike", ChoiceKind::kDamageType,
                            {{kTrueStrikeRadiant, kDamageTypeRadiant, "Radiant"},
                             {kTrueStrikeWeapon, 0, "Weapon's type"}}};
  all.push_back(true_strike);
  // Build 4a: the utility cantrips, each a menu of its effects (the option's value a UtilityEffect).
  const auto effect = [](UtilityEffect e) { return static_cast<uint32_t>(e); };
  ChoiceCantrip elementalism{kElementalism, "n5_c_utility", ChoiceKind::kMode,
                             {{1110, effect(UtilityEffect::kBeckonAir), "Beckon Air"},
                              {1111, effect(UtilityEffect::kBeckonEarth), "Beckon Earth"},
                              {1112, effect(UtilityEffect::kBeckonFire), "Beckon Fire"},
                              {1113, effect(UtilityEffect::kBeckonWater), "Beckon Water"}}};
  all.push_back(elementalism);
  ChoiceCantrip prestidigitation{kPrestidigitation, "n5_c_utility", ChoiceKind::kMode,
                                 {{1115, effect(UtilityEffect::kFirePlay), "Fire Play"},
                                  {kMagicMarkWhite, effect(UtilityEffect::kMagicMark), "Magic Mark: White"},
                                  {kMagicMarkGold, effect(UtilityEffect::kMagicMark), "Magic Mark: Gold"},
                                  {kMagicMarkGold + 1, effect(UtilityEffect::kMagicMark), "Magic Mark: Red"},
                                  {kMagicMarkGold + 2, effect(UtilityEffect::kMagicMark), "Magic Mark: Green"},
                                  {kMagicMarkGold + 3, effect(UtilityEffect::kMagicMark), "Magic Mark: Blue"},
                                  {kMagicMarkGold + 4, effect(UtilityEffect::kMagicMark), "Magic Mark: Violet"},
                                  {kMagicMarkRandom, effect(UtilityEffect::kMagicMark), "Magic Mark: Random"}}};
  all.push_back(prestidigitation);
  ChoiceCantrip thaumaturgy{kThaumaturgy, "n5_c_utility", ChoiceKind::kMode,
                            {{1118, effect(UtilityEffect::kBoomingVoice), "Booming Voice"},
                             {1119, effect(UtilityEffect::kThaumaturgyFirePlay), "Fire Play"},
                             {1120, effect(UtilityEffect::kInvisibleHand), "Invisible Hand"},
                             {kPhantomWhispers, effect(UtilityEffect::kPhantomWhispers), "Phantom Sound: Whispers"},
                             {kPhantomThunder, effect(UtilityEffect::kPhantomThunder), "Phantom Sound: Thunder"}}};
  all.push_back(thaumaturgy);
  // Build 4b.
  ChoiceCantrip druidcraft{kDruidcraft, "n5_c_utility", ChoiceKind::kMode,
                           {{1107, effect(UtilityEffect::kFirePlay), "Fire Play"},
                            {1108, effect(UtilityEffect::kAnimalSound), "Animal Sound"}}};
  all.push_back(druidcraft);
  ChoiceCantrip illusion{kMinorIllusion, "n5_c_utility", ChoiceKind::kMode,
                         {{1124, effect(UtilityEffect::kIllusionVoices), "Sound: Voices"},
                          {1125, effect(UtilityEffect::kIllusionFootsteps), "Sound: Footsteps"},
                          {1126, effect(UtilityEffect::kIllusionRoar), "Sound: Roar"},
                          {1127, effect(UtilityEffect::kIllusionThunder), "Sound: Thunder"},
                          {1128, effect(UtilityEffect::kImageTreasure), "Image: Treasure"},
                          {1141, effect(UtilityEffect::kImageCorpse), "Image: Corpse"},
                          {1129, effect(UtilityEffect::kImageHumanoid), "Image: Humanoid"},
                          {1130, effect(UtilityEffect::kImageCreature), "Image: Small Creature"}}};
  all.push_back(illusion);
  ChoiceCantrip lights{kDancingLights, "n5_c_utility", ChoiceKind::kMode,
                       {{kDancingLightsLights, effect(UtilityEffect::kDancingLights), "Lights"},
                        {kDancingLightsMove, effect(UtilityEffect::kDancingLightsMove), "Move"}}};
  all.push_back(lights);
  return all;
}

const std::vector<ChoiceCantrip>& Choices() {
  static const std::vector<ChoiceCantrip> kChoices = BuildChoiceCantrips();
  return kChoices;
}

}  // namespace

const std::vector<ChoiceCantrip>& ChoiceCantrips() { return Choices(); }

const ChoiceCantrip* ChoiceCantripOf(int spell) {
  for (const ChoiceCantrip& cantrip : Choices()) {
    if (cantrip.master == spell) return &cantrip;
    for (const CantripOption& option : cantrip.options) {
      if (option.spell == spell) return &cantrip;
    }
  }
  return nullptr;
}

const CantripOption* ChoiceOptionOf(int spell) {
  for (const ChoiceCantrip& cantrip : Choices()) {
    for (const CantripOption& option : cantrip.options) {
      if (option.spell == spell) return &option;
    }
  }
  return nullptr;
}

int EffectIconOf(int spell) {
  for (const ChoiceCantrip& cantrip : Choices()) {
    if (cantrip.icon < 0) continue;
    if (cantrip.master == spell) return cantrip.icon;
    for (size_t i = 0; i < cantrip.options.size(); ++i) {
      if (cantrip.options[i].spell == spell) return cantrip.icon + 1 + static_cast<int>(i);
    }
  }
  return -1;
}

RadialPage RadialChoicePage(int count, int first) {
  RadialPage page;
  if (count <= 0 || first < 0 || first >= count) return page;
  const int left = count - first;
  if (left <= kRadialSlots) {
    page.shown = left;
    return page;
  }
  page.shown = kRadialSlots - 1;
  page.more = true;
  return page;
}

namespace {
constexpr int kChoiceFirstShift = 44;
constexpr long long kChoiceFirstMask = 0x1FLL << kChoiceFirstShift;
constexpr long long kMenuPageBits = 0x1FFLL << 40;  // #36's from-level (40-43) and the choice page (44-48)
}  // namespace

int RadialChoiceFirst(long long param) { return static_cast<int>((param & kChoiceFirstMask) >> kChoiceFirstShift); }

long long RadialChoiceWithFirst(long long param, int first) {
  return (param & ~kChoiceFirstMask) | (static_cast<long long>(first & 0x1F) << kChoiceFirstShift);
}

long long RadialChoiceCast(long long param, int variant) {
  return (param & ~kMenuPageBits & ~0xFFFFLL) | static_cast<long long>(variant & 0xFFFF);
}

int BonusActionFeatOfSpell(int spell) {
  if (spell == kProduceFlame || spell == kProduceFlameFlame) return kFeatProduceFlame;  // casting it: the Flame
  if (spell == kShillelagh || spell == kShillelaghForce) return kFeatShillelaghForce;   // casting it: Force
  if (spell == kShillelaghWeapon) return kFeatShillelaghWeapon;
  return -1;
}

bool IsBonusActionSpellFeat(int feat) {
  return feat == kFeatProduceFlame || feat == kFeatShillelaghForce || feat == kFeatShillelaghWeapon ||
         feat == kFeatHealingWord;  // #39 2a build 1b: its cast's slot spent by rules_spells2024.cpp
}

int SorcerousBurstDamage(int dice, int extra_cap, int (*roll_d8)(void* context), void* context, int* extra_out) {
  int total = 0;
  int extra = 0;
  int pending = dice > 0 ? dice : 0;
  while (pending > 0) {
    --pending;
    const int roll = roll_d8(context);
    total += roll;
    if (roll == 8 && extra < extra_cap) {
      ++extra;
      ++pending;
    }
  }
  if (extra_out) *extra_out = extra;
  return total;
}

int ResistanceReduction(int damage, int d4) {
  if (damage <= 0 || d4 <= 0) return 0;
  return d4 < damage ? d4 : damage;
}

int SpareTheDyingRangeFeet(int character_level) {
  int feet = 15;
  if (character_level >= 5) feet *= 2;
  if (character_level >= 11) feet *= 2;
  if (character_level >= 17) feet *= 2;
  return feet;
}

bool IsShillelagh(int spell) {
  return spell == kShillelagh || spell == kShillelaghForce || spell == kShillelaghWeapon;
}

bool IsTrueStrike(int spell) {
  return spell == kTrueStrike || spell == kTrueStrikeRadiant || spell == kTrueStrikeWeapon;
}

bool IsShillelaghWeapon(int nwn_base_item) {
  return nwn_base_item == 28 || nwn_base_item == 50 || nwn_base_item == 45;
}

WeaponDie ShillelaghDie(int character_level) {
  if (character_level >= 17) return {2, 6};
  if (character_level >= 11) return {1, 12};
  if (character_level >= 5) return {1, 10};
  return {1, 8};
}

WeaponDie BetterDice(WeaponDie a, WeaponDie b) {
  return b.count * (b.sides + 1) > a.count * (a.sides + 1) ? b : a;  // twice the average
}

int TrueStrikeExtraDice(int character_level, bool critical) {
  int dice = 0;
  if (character_level >= 5) ++dice;
  if (character_level >= 11) ++dice;
  if (character_level >= 17) ++dice;
  return critical ? dice * 2 : dice;
}

int WeaponCantripAbility(bool true_strike, int normal, int casting) {
  if (true_strike) return casting;
  return casting > normal ? casting : normal;
}

uint32_t WeaponCantripDamageType(int spell) {
  if (const CantripOption* option = ChoiceOptionOf(spell)) {
    return IsShillelagh(spell) || IsTrueStrike(spell) ? option->value : 0;
  }
  if (spell == kShillelagh || spell == kTrueStrike) return ChoiceCantripOf(spell)->options.front().value;
  return 0;
}

namespace {
constexpr int kBaseWeaponIndex = 12;  // DAMAGE_TYPE_BASE_WEAPON (4096)
}  // namespace

int DamageTypeIndex(uint32_t type) {
  if (type == 0 || (type & (type - 1)) != 0) return -1;
  int index = 0;
  while ((type >> index) != 1u) ++index;
  return index;
}

void AddDamage(int16_t* entries, int count, uint32_t type, int amount) {
  const int index = DamageTypeIndex(type);
  if (!entries || index < 0 || index >= count || amount <= 0) return;
  entries[index] = static_cast<int16_t>((entries[index] > 0 ? entries[index] : 0) + amount);
}

UtilityEffect UtilityEffectOf(int spell) {
  if (spell == kMageHand) return UtilityEffect::kMageHand;
  const ChoiceCantrip* cantrip = ChoiceCantripOf(spell);
  if (!cantrip || cantrip->kind != ChoiceKind::kMode || cantrip->options.empty()) return UtilityEffect::kNone;
  if (cantrip->master != kElementalism && cantrip->master != kPrestidigitation && cantrip->master != kThaumaturgy &&
      cantrip->master != kDruidcraft && cantrip->master != kMinorIllusion && cantrip->master != kDancingLights) {
    return UtilityEffect::kNone;  // Produce Flame's modes are its own
  }
  const CantripOption* option = ChoiceOptionOf(spell);
  return static_cast<UtilityEffect>(option ? option->value : cantrip->options.front().value);
}

FirePlay FirePlayOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kFirePlay:
    case UtilityEffect::kMageHand: return FirePlay::kToggle;
    case UtilityEffect::kBeckonFire: return FirePlay::kLight;
    case UtilityEffect::kBeckonWater: return FirePlay::kSnuff;
    case UtilityEffect::kThaumaturgyFirePlay: return FirePlay::kDim;
    default: return FirePlay::kNone;
  }
}

DoorPlay DoorPlayOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kInvisibleHand:
    case UtilityEffect::kMageHand: return DoorPlay::kToggle;
    case UtilityEffect::kBeckonAir: return DoorPlay::kClose;
    default: return DoorPlay::kNone;
  }
}

int MagicMarkVisual(int roll) {
  if (roll < 1 || roll > kMagicMarkVisuals) roll = 1;
  return kMagicMarkFirstVisual + roll - 1;
}

int MagicMarkUprightVisual(int roll) {
  if (roll < 1 || roll > kMagicMarkVisuals) roll = 1;
  return kMagicMarkUprightFirstVisual + roll - 1;
}

int MagicMarkTintOf(int row) {
  if (row == kMagicMarkWhite) return static_cast<int>(MarkTint::kWhite);
  if (row >= kMagicMarkGold && row < kMagicMarkGold + kMagicMarkTints - 1) return row - kMagicMarkGold + 1;
  return -1;
}

std::string MagicMarkModel(int symbol, int tint, bool upright) {
  static const char* const kLetters[kMagicMarkTints] = {"", "g", "r", "n", "b", "v"};
  if (symbol < 1 || symbol > kMagicMarkVisuals) symbol = 1;
  if (tint < 0 || tint >= kMagicMarkTints) tint = 0;
  char name[24];
  snprintf(name, sizeof(name), "n5v_mark%02d%s%s", symbol, kLetters[tint], upright ? "u" : "");
  return name;
}

namespace {

std::vector<int> FreeSymbols(const std::vector<bool>& taken) {
  std::vector<int> free;
  for (int symbol = 1; symbol <= kMagicMarkVisuals; ++symbol) {
    if (!(symbol - 1 < static_cast<int>(taken.size()) && taken[symbol - 1])) free.push_back(symbol);
  }
  if (free.empty()) {
    for (int symbol = 1; symbol <= kMagicMarkVisuals; ++symbol) free.push_back(symbol);
  }
  return free;
}

}  // namespace

int FreeMarkSymbols(const std::vector<bool>& taken) { return static_cast<int>(FreeSymbols(taken).size()); }

int FreeMarkSymbol(int roll, const std::vector<bool>& taken) {
  const std::vector<int> free = FreeSymbols(taken);
  roll = std::clamp(roll, 1, static_cast<int>(free.size()));
  return free[roll - 1];
}

int MagicMarkRow(int symbol, int tint, bool upright) {
  if (symbol < 1 || symbol > kMagicMarkVisuals) symbol = 1;
  if (tint <= 0 || tint >= kMagicMarkTints) return upright ? MagicMarkUprightVisual(symbol) : MagicMarkVisual(symbol);
  return kMagicMarkTintFirstVisual + (tint - 1) * kMagicMarkTintStride + (upright ? kMagicMarkUprightOffset : 0) +
         symbol - 1;
}

namespace {

constexpr float kPi = 3.14159265358979f;

float Degrees(float radians) {
  float d = radians * 180.0f / kPi;
  while (d < 0.0f) d += 360.0f;
  while (d >= 360.0f) d -= 360.0f;
  return d;
}

}  // namespace

MarkSpot MarkOnDoor(float dx, float dy, float dz, float facing, float cx, float cy) {
  // The door's frame: its +y along its facing, so world = rotate(facing - 90) local.
  const float a = (facing - 90.0f) * kPi / 180.0f;
  const float ca = std::cos(a), sa = std::sin(a);
  const float wx = cx - dx, wy = cy - dy;
  float lx = wx * ca + wy * sa;  // the caster in the door's frame (rotate by -a)
  float ly = -wx * sa + wy * ca;
  const float length = std::sqrt(lx * lx + ly * ly);
  if (length < 0.01f) {
    lx = 0.0f;
    ly = 1.0f;
  } else {
    lx /= length;
    ly /= length;
  }
  // Where the line from the centre leaves the slab: through a side face (x) or the front / back face (y).
  const float tx = lx != 0.0f ? kDoorHalfWidth / std::fabs(lx) : 1e9f;
  const float ty = ly != 0.0f ? kDoorHalfDepth / std::fabs(ly) : 1e9f;
  float hx, hy, nx, ny;
  if (ty <= tx) {
    hx = lx * ty;
    hy = ly * ty;
    nx = 0.0f;
    ny = ly >= 0.0f ? 1.0f : -1.0f;
  } else {
    hx = lx * tx;
    hy = ly * tx;
    nx = lx >= 0.0f ? 1.0f : -1.0f;
    ny = 0.0f;
  }
  hx += nx * kMarkGap;
  hy += ny * kMarkGap;
  MarkSpot spot;
  spot.x = dx + hx * ca - hy * sa;
  spot.y = dy + hx * sa + hy * ca;
  spot.z = dz + kMarkDoorHeight;
  spot.facing = Degrees(a + std::atan2(ny, nx));
  return spot;
}

MarkSpot MarkOnPlaceable(float px, float py, float cx, float cy, float surface, float ground) {
  const float wx = px - cx, wy = py - cy;
  const float length = std::sqrt(wx * wx + wy * wy);
  MarkSpot spot;
  spot.z = ground + kMarkObjectHeight;
  if (length < 0.01f) {  // the caster on it: the placeable's own spot
    spot.x = px;
    spot.y = py;
    return spot;
  }
  const float ux = wx / length, uy = wy / length;  // from the caster to the placeable
  float from_caster = surface >= 0.0f ? surface * length - kMarkGap : length - kMarkNoMeshReach;
  if (from_caster < 0.0f) from_caster = 0.0f;
  spot.x = cx + ux * from_caster;
  spot.y = cy + uy * from_caster;
  spot.facing = Degrees(std::atan2(-uy, -ux));  // toward the caster
  return spot;
}

bool FillsElementCube(UtilityEffect effect) {
  return effect == UtilityEffect::kBeckonAir || effect == UtilityEffect::kBeckonEarth ||
         effect == UtilityEffect::kBeckonFire || effect == UtilityEffect::kBeckonWater;
}

int CantripRangeFeet(int spell) {
  const UtilityEffect effect = UtilityEffectOf(spell);
  if (effect == UtilityEffect::kNone || effect == UtilityEffect::kBoomingVoice) {
    return 0;  // on yourself (or not one)
  }
  const ChoiceCantrip* cantrip = ChoiceCantripOf(spell);
  if (cantrip && cantrip->master == kPrestidigitation) return 10;
  if (cantrip && cantrip->master == kDancingLights) return kDancingLightsRangeFeet;
  return 30;
}

SoundPlay SoundPlayOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kPhantomWhispers:
    case UtilityEffect::kIllusionVoices:
    case UtilityEffect::kIllusionFootsteps: return SoundPlay::kDraws;
    case UtilityEffect::kAnimalSound:
    case UtilityEffect::kPhantomThunder:
    case UtilityEffect::kIllusionRoar:
    case UtilityEffect::kIllusionThunder: return SoundPlay::kScares;
    default: return SoundPlay::kNone;
  }
}

int SoundSizeOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kIllusionRoar: return 4;  // a Large beast's (the bear's)
    case UtilityEffect::kIllusionThunder:
    case UtilityEffect::kPhantomThunder:
    case UtilityEffect::kAnimalSound: return 3;  // Medium
    default: return 0;
  }
}

SoundReaction ReactionToSound(SoundPlay play, bool sentient, bool hostile, int size, int sound_size) {
  switch (play) {
    case SoundPlay::kDraws: return hostile ? SoundReaction::kLook : SoundReaction::kIgnore;
    case SoundPlay::kScares:
      if (!hostile || size < sound_size) return sentient ? SoundReaction::kFleeThenLook : SoundReaction::kFlee;
      return sentient ? SoundReaction::kLookThenSearch : SoundReaction::kLook;
    default: return SoundReaction::kIgnore;
  }
}

const char* SoundResrefOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kPhantomWhispers: return "as_pl_chantingm1";
    case UtilityEffect::kIllusionVoices: return "as_pl_comtalk1";
    case UtilityEffect::kIllusionFootsteps: return "fs_dirt_hard1";
    case UtilityEffect::kAnimalSound: return "c_wolf_bat1";
    case UtilityEffect::kIllusionRoar: return "c_bear_bat1";
    case UtilityEffect::kPhantomThunder:
    case UtilityEffect::kIllusionThunder: return "as_wt_thundercl1";
    default: return "";
  }
}

ImageKind ImageKindOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kImageTreasure: return ImageKind::kTreasure;
    case UtilityEffect::kImageCorpse: return ImageKind::kCorpse;
    case UtilityEffect::kImageHumanoid: return ImageKind::kHumanoid;
    case UtilityEffect::kImageCreature: return ImageKind::kCreature;
    default: return ImageKind::kNone;
  }
}

const char* ImageBlueprintOf(ImageKind kind) {
  switch (kind) {
    case ImageKind::kTreasure: return "plc_treasurelrg";
    case ImageKind::kCorpse: return "plc_corpse1";
    case ImageKind::kHumanoid: return "nw_commale";
    case ImageKind::kCreature: return "nw_cat";
    default: return "";
  }
}

bool ImageIsObject(ImageKind kind) { return kind == ImageKind::kTreasure || kind == ImageKind::kCorpse; }

const char* ImageNameOf(ImageKind kind) {
  switch (kind) {
    case ImageKind::kTreasure: return "Treasure Pile";
    case ImageKind::kCorpse: return "Corpse";
    default: return "";
  }
}

bool SeesThroughIllusion(int d20, int investigation_bonus, int dc) { return d20 + investigation_bonus >= dc; }

bool IsSentient(int nwn_racial_type, int intelligence_score) {
  switch (nwn_racial_type) {
    case 8:   // animal
    case 9:   // beast
    case 10:  // construct
    case 25:  // vermin
    case 29:  // ooze
      return false;
    default: return intelligence_score >= 4;
  }
}

ImageReaction ReactionToImage(ImageKind kind, bool sentient, bool hostile, int size, int image_size) {
  switch (kind) {
    case ImageKind::kTreasure: return sentient && hostile ? ImageReaction::kTouch : ImageReaction::kIgnore;
    case ImageKind::kCorpse:
      if (!hostile) return ImageReaction::kIgnore;
      return sentient ? ImageReaction::kStudy : ImageReaction::kTouch;
    case ImageKind::kHumanoid:
      if (sentient) return hostile ? ImageReaction::kStudy : ImageReaction::kIgnore;
      return hostile ? ImageReaction::kAttack : ImageReaction::kFlee;
    case ImageKind::kCreature:
      if (!hostile || size < image_size) return sentient ? ImageReaction::kFleeThenStudy : ImageReaction::kFlee;
      return sentient ? ImageReaction::kAttackThenSearch : ImageReaction::kAttack;
    default: return ImageReaction::kIgnore;
  }
}

AfterStudy AfterStudyOf(ImageKind kind, bool sees_through, bool hostile) {
  if (sees_through) return hostile ? AfterStudy::kVanishThenSearch : AfterStudy::kVanishThenGoBack;
  return kind == ImageKind::kCreature ? AfterStudy::kFleeAgain : AfterStudy::kGoBack;
}

bool SearchesAfterVanish(bool sentient, bool hostile) { return sentient && hostile; }

bool DancingLightsMoveAllowed(float feet_from_lights, float feet_from_caster) {
  return feet_from_lights <= static_cast<float>(kDancingLightsMoveFeet) &&
         feet_from_caster <= static_cast<float>(kDancingLightsRangeFeet);
}

int PatchAreaEffectOf(UtilityEffect effect) {
  if (effect == UtilityEffect::kBeckonWater) return kPuddleAreaEffect;
  if (effect == UtilityEffect::kBeckonEarth) return kDustAreaEffect;
  return -1;
}

int PatchSecondsOf(UtilityEffect effect) {
  if (effect == UtilityEffect::kBeckonWater) return 60;
  if (effect == UtilityEffect::kBeckonEarth) return 600;
  return 0;
}

const char* PatchBlueprintOf(UtilityEffect effect) {
  if (effect == UtilityEffect::kBeckonWater) return "nw_plc_puddle1";
  if (effect == UtilityEffect::kBeckonEarth) return "x3_plc_dirt001";
  return "";
}

float PatchScaleOf(UtilityEffect effect) {
  if (effect == UtilityEffect::kBeckonEarth) return 0.6f;  // 2.5 m to 1.5 m
  return 1.0f;
}

int MoveWeaponDamage(int16_t* entries, int count, uint32_t type) {
  const int index = DamageTypeIndex(type);
  if (!entries || index < 0 || index >= count || index == kBaseWeaponIndex || count <= kBaseWeaponIndex) return 0;
  const int moved = entries[kBaseWeaponIndex];
  if (moved <= 0) return 0;
  AddDamage(entries, count, type, moved);
  entries[kBaseWeaponIndex] = 0;
  return moved;
}

}  // namespace rules
