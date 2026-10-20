#include "rules/abilities.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>
#include <string>

namespace rules {

namespace {

int Index(Ability ability) { return static_cast<int>(ability); }

// Adds up to `amount` to `score` (tracked in `delta`) without passing 20.
void Raise(int index, int amount, const int nwn_base[6], int delta[6]) {
  const int room = std::max(0, kAbilityIncreaseCap - (nwn_base[index] + delta[index]));
  delta[index] += std::min(amount, room);
}

}  // namespace

void FaithfulAbilityDelta(const int nwn_base[6], const int level_bumps[6], const int background[6],
                          const std::vector<AbilityIncrease>& asis, int delta[6]) {
  for (int i = 0; i < 6; ++i) delta[i] = -level_bumps[i];
  for (int i = 0; i < 6; ++i) Raise(i, background[i], nwn_base, delta);
  for (const AbilityIncrease& asi : asis) {
    if (asi.second && *asi.second != asi.first) {
      Raise(Index(asi.first), 1, nwn_base, delta);
      Raise(Index(*asi.second), 1, nwn_base, delta);
    } else {
      Raise(Index(asi.first), 2, nwn_base, delta);
    }
  }
}

std::vector<Ability> AsiPriority2024(CharacterClass character_class, int strength, int dexterity) {
  switch (character_class) {
    case CharacterClass::kBarbarian: return {Ability::kStrength, Ability::kConstitution};
    case CharacterClass::kBard: return {Ability::kCharisma, Ability::kDexterity, Ability::kConstitution};
    case CharacterClass::kCleric:
    case CharacterClass::kDruid: return {Ability::kWisdom, Ability::kConstitution};
    case CharacterClass::kFighter:
      return {dexterity > strength ? Ability::kDexterity : Ability::kStrength, Ability::kConstitution};
    case CharacterClass::kMonk:
    case CharacterClass::kRanger: return {Ability::kDexterity, Ability::kWisdom, Ability::kConstitution};
    case CharacterClass::kPaladin: return {Ability::kStrength, Ability::kCharisma, Ability::kConstitution};
    case CharacterClass::kRogue: return {Ability::kDexterity, Ability::kConstitution};
    case CharacterClass::kSorcerer:
    case CharacterClass::kWarlock: return {Ability::kCharisma, Ability::kConstitution};
    case CharacterClass::kWizard: return {Ability::kIntelligence, Ability::kConstitution};
  }
  return {Ability::kConstitution};
}

std::vector<AbilityIncrease> AutomaticAsis(int count, const std::vector<Ability>& priority, const int scores[6]) {
  int current[6];
  for (int i = 0; i < 6; ++i) current[i] = scores[i];
  std::vector<AbilityIncrease> asis;
  for (int n = 0; n < count; ++n) {
    for (Ability ability : priority) {
      if (current[Index(ability)] >= kAbilityIncreaseCap) continue;
      asis.push_back({ability, std::nullopt});
      current[Index(ability)] = std::min(kAbilityIncreaseCap, current[Index(ability)] + 2);
      break;
    }
  }
  return asis;
}

const char* AbilityName(Ability ability) {
  static const char* const kNames[] = {"Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma"};
  const int i = Index(ability);
  return i >= 0 && i < 6 ? kNames[i] : "";
}

const char* AbilityShortName(Ability ability) {
  static const char* const kNames[] = {"str", "dex", "con", "int", "wis", "cha"};
  const int i = Index(ability);
  return i >= 0 && i < 6 ? kNames[i] : "";
}

std::optional<Ability> ParseAbility(const char* text) {
  std::string lower;
  for (const char* c = text; *c; ++c) lower += static_cast<char>(tolower(static_cast<unsigned char>(*c)));
  for (int i = 0; i < 6; ++i) {
    const Ability ability = static_cast<Ability>(i);
    std::string name = AbilityName(ability);
    std::transform(name.begin(), name.end(), name.begin(), [](char c) { return static_cast<char>(tolower(c)); });
    if (lower == AbilityShortName(ability) || lower == name) return ability;
  }
  return std::nullopt;
}

}  // namespace rules
