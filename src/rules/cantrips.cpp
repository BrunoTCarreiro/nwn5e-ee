#include "rules/cantrips.h"

#include <algorithm>

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
  ChoiceCantrip flame{kProduceFlame, "n5_c_produceflame", ChoiceKind::kMode,
                     {{kProduceFlameFlame, 0, "Flame"}, {kProduceFlameHurl, 1, "Hurl"}}};
  all.push_back(flame);
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
  return -1;
}

bool IsBonusActionSpellFeat(int feat) { return feat == kFeatProduceFlame; }

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

}  // namespace rules
