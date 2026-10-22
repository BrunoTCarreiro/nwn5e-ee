#include "rules/progression.h"

#include <algorithm>

#include "rules/abilities.h"

namespace rules {

namespace {

constexpr uint32_t kXp2024[kMaxLevel2024] = {0,      300,    900,    2700,   6500,   14000,  23000,
                                             34000,  48000,  64000,  85000,  100000, 120000, 140000,
                                             165000, 195000, 225000, 265000, 305000, 355000};

int Score(const int scores[6], Ability ability) { return scores[static_cast<int>(ability)]; }

}  // namespace

uint32_t Xp2024ForLevel(int level) { return kXp2024[std::clamp(level, 1, kMaxLevel2024) - 1]; }

uint32_t NwnXpForLevel(int level) {
  const uint32_t l = static_cast<uint32_t>(std::clamp(level, 1, kXpTableSize));
  return 500 * l * (l - 1);
}

uint32_t FaithfulXpForLevel(int level) {
  level = std::clamp(level, 1, kXpTableSize);
  if (level <= kMaxLevel2024) return Xp2024ForLevel(level);
  return Xp2024ForLevel(kMaxLevel2024) + NwnXpForLevel(level) - NwnXpForLevel(kMaxLevel2024);
}

int FaithfulLevelForXp(uint32_t xp) {
  int level = 1;
  while (level < kXpTableSize && xp >= FaithfulXpForLevel(level + 1)) ++level;
  return level;
}

uint32_t ScriptSetXp2024(uint32_t requested, uint32_t current_xp, int current_level) {
  current_level = std::clamp(current_level, 1, kXpTableSize);
  const uint32_t floor = FaithfulXpForLevel(current_level);
  for (int level = 1; level <= kXpTableSize; ++level) {
    if (requested != NwnXpForLevel(level)) continue;
    if (level != current_level) return FaithfulXpForLevel(level);
    return std::max(floor, std::min(requested, current_xp));
  }
  return std::max(requested, floor);
}

bool IsFeatLevel2024(CharacterClass character_class, int class_level) {
  switch (class_level) {
    case 4: case 8: case 12: case 16: case 19: return true;
    case 6: case 14: return character_class == CharacterClass::kFighter;
    case 10: return character_class == CharacterClass::kRogue;
    default: return false;
  }
}

int FeatLevelsReached2024(CharacterClass character_class, int class_level) {
  int count = 0;
  for (int level = 1; level <= class_level && level <= kMaxLevel2024; ++level) {
    if (IsFeatLevel2024(character_class, level)) ++count;
  }
  return count;
}

bool MeetsPrimaryAbility(CharacterClass character_class, const int scores[6]) {
  auto at_least_13 = [scores](Ability ability) { return Score(scores, ability) >= 13; };
  switch (character_class) {
    case CharacterClass::kBarbarian: return at_least_13(Ability::kStrength);
    case CharacterClass::kBard: return at_least_13(Ability::kCharisma);
    case CharacterClass::kCleric: return at_least_13(Ability::kWisdom);
    case CharacterClass::kDruid: return at_least_13(Ability::kWisdom);
    case CharacterClass::kFighter: return at_least_13(Ability::kStrength) || at_least_13(Ability::kDexterity);
    case CharacterClass::kMonk: return at_least_13(Ability::kDexterity) && at_least_13(Ability::kWisdom);
    case CharacterClass::kPaladin: return at_least_13(Ability::kStrength) && at_least_13(Ability::kCharisma);
    case CharacterClass::kRanger: return at_least_13(Ability::kDexterity) && at_least_13(Ability::kWisdom);
    case CharacterClass::kRogue: return at_least_13(Ability::kDexterity);
    case CharacterClass::kSorcerer: return at_least_13(Ability::kCharisma);
    case CharacterClass::kWarlock: return at_least_13(Ability::kCharisma);
    case CharacterClass::kWizard: return at_least_13(Ability::kIntelligence);
  }
  return true;
}

const char* PrimaryAbilityText(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian: return "Strength 13";
    case CharacterClass::kBard: return "Charisma 13";
    case CharacterClass::kCleric: return "Wisdom 13";
    case CharacterClass::kDruid: return "Wisdom 13";
    case CharacterClass::kFighter: return "Strength or Dexterity 13";
    case CharacterClass::kMonk: return "Dexterity and Wisdom 13";
    case CharacterClass::kPaladin: return "Strength and Charisma 13";
    case CharacterClass::kRanger: return "Dexterity and Wisdom 13";
    case CharacterClass::kRogue: return "Dexterity 13";
    case CharacterClass::kSorcerer: return "Charisma 13";
    case CharacterClass::kWarlock: return "Charisma 13";
    case CharacterClass::kWizard: return "Intelligence 13";
  }
  return "";
}

std::vector<Ability> PrimaryAbilities(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian: return {Ability::kStrength};
    case CharacterClass::kBard: return {Ability::kCharisma};
    case CharacterClass::kCleric: return {Ability::kWisdom};
    case CharacterClass::kDruid: return {Ability::kWisdom};
    case CharacterClass::kFighter: return {Ability::kStrength, Ability::kDexterity};
    case CharacterClass::kMonk: return {Ability::kDexterity, Ability::kWisdom};
    case CharacterClass::kPaladin: return {Ability::kStrength, Ability::kCharisma};
    case CharacterClass::kRanger: return {Ability::kDexterity, Ability::kWisdom};
    case CharacterClass::kRogue: return {Ability::kDexterity};
    case CharacterClass::kSorcerer: return {Ability::kCharisma};
    case CharacterClass::kWarlock: return {Ability::kCharisma};
    case CharacterClass::kWizard: return {Ability::kIntelligence};
  }
  return {};
}

std::string MulticlassRefusalText(CharacterClass next, CharacterClass failing, const int scores[6]) {
  static const char* const kNames[] = {"Barbarian", "Bard",    "Cleric", "Druid",    "Fighter", "Monk",
                                       "Paladin",   "Ranger",  "Rogue",  "Sorcerer", "Warlock", "Wizard"};
  std::string have;
  for (Ability ability : PrimaryAbilities(failing)) {
    have += (have.empty() ? "" : ", ") + std::string(AbilityName(ability)) + " " + std::to_string(Score(scores, ability));
  }
  const std::string head = std::string("Multiclassing (2024): you can't take a level in ") +
                           kNames[static_cast<int>(next)] + " now - ";
  if (failing == next) return head + "it needs " + PrimaryAbilityText(failing) + ", and you have " + have + ".";
  return head + "every class you have needs its score too, and " + kNames[static_cast<int>(failing)] + " needs " +
         PrimaryAbilityText(failing) + ": you have " + have + ".";
}

bool CanMulticlassInto(CharacterClass next, const std::vector<CharacterClass>& current, const int scores[6],
                       CharacterClass* failing) {
  if (current.empty() || std::find(current.begin(), current.end(), next) != current.end()) return true;
  std::vector<CharacterClass> all = current;
  all.insert(all.begin(), next);
  for (CharacterClass character_class : all) {
    if (MeetsPrimaryAbility(character_class, scores)) continue;
    if (failing) *failing = character_class;
    return false;
  }
  return true;
}

}  // namespace rules
