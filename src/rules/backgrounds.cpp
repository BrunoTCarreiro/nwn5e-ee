#include "rules/backgrounds.h"

#include <ctype.h>

#include <algorithm>
#include <array>

namespace rules {

const std::vector<BackgroundInfo>& Backgrounds2024() {
  static const std::vector<BackgroundInfo> kBackgrounds = {
      {Background::kAcolyte, "Acolyte", {Ability::kIntelligence, Ability::kWisdom, Ability::kCharisma},
       "Magic Initiate (Cleric)", SkillBit(Skill2024::kInsight) | SkillBit(Skill2024::kReligion),
       "Calligrapher's Supplies"},
      {Background::kCriminal, "Criminal", {Ability::kDexterity, Ability::kConstitution, Ability::kIntelligence},
       "Alert",
       SkillBit(Skill2024::kSleightOfHand) | SkillBit(Skill2024::kStealth) | SkillBit(Skill2024::kThievesTools),
       "Thieves' Tools"},
      {Background::kSage, "Sage", {Ability::kConstitution, Ability::kIntelligence, Ability::kWisdom},
       "Magic Initiate (Wizard)", SkillBit(Skill2024::kArcana) | SkillBit(Skill2024::kHistory),
       "Calligrapher's Supplies"},
      {Background::kSoldier, "Soldier", {Ability::kStrength, Ability::kDexterity, Ability::kConstitution},
       "Savage Attacker", SkillBit(Skill2024::kAthletics) | SkillBit(Skill2024::kIntimidation), "a Gaming Set"},
  };
  return kBackgrounds;
}

const BackgroundInfo* BackgroundData(Background background) {
  for (const BackgroundInfo& info : Backgrounds2024()) {
    if (info.background == background) return &info;
  }
  return nullptr;
}

std::optional<Background> ParseBackground(const std::string& text) {
  std::string lower;
  for (char c : text) lower += static_cast<char>(tolower(static_cast<unsigned char>(c)));
  for (const BackgroundInfo& info : Backgrounds2024()) {
    std::string name = info.name;
    std::transform(name.begin(), name.end(), name.begin(), [](char c) { return static_cast<char>(tolower(c)); });
    if (lower == name) return info.background;
  }
  return std::nullopt;
}

std::string BackgroundIncreases(Background background, const std::vector<Ability>& chosen, int amounts[6]) {
  const BackgroundInfo* info = BackgroundData(background);
  if (!info) return "no such background";
  for (int i = 0; i < 6; ++i) amounts[i] = 0;
  if (chosen.size() != 2 && chosen.size() != 3) return "name two abilities (+2 and +1) or all three (+1 each)";
  for (size_t i = 0; i < chosen.size(); ++i) {
    if (std::find(std::begin(info->abilities), std::end(info->abilities), chosen[i]) == std::end(info->abilities)) {
      return std::string(AbilityName(chosen[i])) + " isn't one of the " + info->name + "'s abilities";
    }
    for (size_t j = 0; j < i; ++j) {
      if (chosen[j] == chosen[i]) return "name each ability once";
    }
    amounts[static_cast<int>(chosen[i])] = chosen.size() == 2 && i == 0 ? 2 : 1;
  }
  return "";
}

int EncodeAmounts(const int amounts[6]) {
  int encoded = 0;
  for (int i = 0; i < 6; ++i) encoded |= (std::clamp(amounts[i], 0, 3) & 3) << (2 * i);
  return encoded;
}

void DecodeAmounts(int encoded, int amounts[6]) {
  for (int i = 0; i < 6; ++i) amounts[i] = (encoded >> (2 * i)) & 3;
}

int EncodeAsi(const AbilityIncrease& asi) {
  return (static_cast<int>(asi.first) + 1) + (asi.second ? static_cast<int>(*asi.second) + 1 : 0) * 8;
}

std::optional<AbilityIncrease> DecodeAsi(int encoded) {
  const int first = encoded % 8 - 1, second = encoded / 8 - 1;
  if (first < 0 || first > 5 || second < -1 || second > 5) return std::nullopt;
  AbilityIncrease asi{static_cast<Ability>(first), std::nullopt};
  if (second >= 0) asi.second = static_cast<Ability>(second);
  return asi;
}

namespace {
// The +2 / +1 pairs over a background's abilities (indices 0-2), splits 0-5.
constexpr int kSplitPairs[6][2] = {{0, 1}, {0, 2}, {1, 0}, {1, 2}, {2, 0}, {2, 1}};
}  // namespace

int BackgroundFeat(Background background, int split) {
  const int index = static_cast<int>(background) - 1;  // Acolyte 0 .. Soldier 3
  if (index < 0 || index >= 4 || split < 0 || split >= kBackgroundSplits) return -1;
  return kBackgroundFeatFirst + index * kBackgroundSplits + split;
}

std::optional<BackgroundFeatChoice> BackgroundFromFeat(int feat) {
  const int row = feat - kBackgroundFeatFirst;
  if (row < 0 || row >= kBackgroundFeatCount) return std::nullopt;
  const auto background = static_cast<Background>(row / kBackgroundSplits + 1);
  const BackgroundInfo* info = BackgroundData(background);
  if (!info) return std::nullopt;
  const int split = row % kBackgroundSplits;
  BackgroundFeatChoice choice{background, {}};
  if (split == 6) {
    choice.chosen.assign(std::begin(info->abilities), std::end(info->abilities));
  } else {
    choice.chosen = {info->abilities[kSplitPairs[split][0]], info->abilities[kSplitPairs[split][1]]};
  }
  return choice;
}

Background RecommendedBackground2024(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian:
    case CharacterClass::kFighter:
    case CharacterClass::kPaladin:
    case CharacterClass::kRanger:
      return Background::kSoldier;
    case CharacterClass::kMonk:
    case CharacterClass::kRogue:
      return Background::kCriminal;
    case CharacterClass::kDruid:
    case CharacterClass::kWizard:
      return Background::kSage;
    case CharacterClass::kBard:
    case CharacterClass::kCleric:
    case CharacterClass::kSorcerer:
    case CharacterClass::kWarlock:
      return Background::kAcolyte;
  }
  return Background::kSoldier;
}

int RecommendedBackgroundSplit2024(Background background, CharacterClass character_class) {
  const BackgroundInfo* info = BackgroundData(background);
  if (!info) return -1;
  const std::array<int, 6> rank = RecommendedScores2024(character_class);
  int order[3] = {0, 1, 2};  // the background's abilities, the class's best first
  std::stable_sort(std::begin(order), std::end(order), [&](int a, int b) {
    return rank[static_cast<int>(info->abilities[a])] > rank[static_cast<int>(info->abilities[b])];
  });
  for (int split = 0; split < 6; ++split) {
    if (kSplitPairs[split][0] == order[0] && kSplitPairs[split][1] == order[1]) return split;
  }
  return 6;
}

}  // namespace rules
