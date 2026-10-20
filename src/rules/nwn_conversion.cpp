#include "rules/nwn_conversion.h"

#include <algorithm>
#include <iterator>

#include "rules/core.h"
#include "rules/monster.h"

namespace rules {

bool UsesCharacterRules(bool is_player_character, int associate_type) {
  return is_player_character || associate_type == kAssociateTypeHenchman;
}

int ProficiencyBonusForCreature(bool is_player_character, int total_level, double challenge_rating) {
  return is_player_character ? ProficiencyBonusForLevel(total_level)
                             : ProficiencyBonusForChallengeRating(challenge_rating);
}

int ConvertAttackModifier(int engine_attack_modifier, int base_attack_bonus, int proficiency_bonus) {
  return engine_attack_modifier - base_attack_bonus + proficiency_bonus;
}

int ConvertArmorClass(bool is_player_character, int engine_armor_class, double challenge_rating) {
  return is_player_character ? engine_armor_class : ExpectedMonsterArmorClass(challenge_rating);
}

std::optional<CharacterClass> ClassFromNwnId(int nwn_class_id) {
  static constexpr CharacterClass kBaseClasses[] = {
      CharacterClass::kBarbarian, CharacterClass::kBard,    CharacterClass::kCleric,  CharacterClass::kDruid,
      CharacterClass::kFighter,   CharacterClass::kMonk,    CharacterClass::kPaladin, CharacterClass::kRanger,
      CharacterClass::kRogue,     CharacterClass::kSorcerer, CharacterClass::kWizard,
  };
  // Decision 10: prestige classes count as the class they would be a 2024
  // subclass of (classes.2da rows 27-37 and 41).
  struct Prestige {
    int nwn_class_id;
    CharacterClass parent;
  };
  static constexpr Prestige kPrestigeClasses[] = {
      {27, CharacterClass::kRogue},    // Shadowdancer
      {28, CharacterClass::kBard},     // Harper Scout
      {29, CharacterClass::kFighter},  // Arcane Archer
      {30, CharacterClass::kRogue},    // Assassin
      {31, CharacterClass::kPaladin},  // Blackguard
      {32, CharacterClass::kPaladin},  // Champion of Torm
      {33, CharacterClass::kFighter},  // Weapon Master
      {34, CharacterClass::kWizard},   // Pale Master
      {35, CharacterClass::kDruid},    // Shifter
      {36, CharacterClass::kFighter},  // Dwarven Defender
      {37, CharacterClass::kSorcerer}, // Red Dragon Disciple
      {41, CharacterClass::kFighter},  // Purple Dragon Knight
  };
  if (nwn_class_id >= 0 && nwn_class_id < static_cast<int>(std::size(kBaseClasses))) return kBaseClasses[nwn_class_id];
  for (const Prestige& prestige : kPrestigeClasses) {
    if (prestige.nwn_class_id == nwn_class_id) return prestige.parent;
  }
  return std::nullopt;
}

std::vector<ClassLevels> GroupClassLevels(const std::vector<NwnClassSlot>& classes) {
  // Levels of a prestige class add to its parent class (Fighter 6 + Weapon
  // Master 6 is a Fighter 12); different classes still do not add up.
  std::vector<ClassLevels> by_class;
  for (const NwnClassSlot& slot : classes) {
    const auto character_class = ClassFromNwnId(slot.nwn_class_id);
    if (!character_class) continue;
    auto existing = std::find_if(by_class.begin(), by_class.end(),
                                 [&](const ClassLevels& entry) { return entry.character_class == *character_class; });
    if (existing != by_class.end()) {
      existing->levels += slot.levels;
    } else {
      by_class.push_back({*character_class, slot.levels});
    }
  }
  return by_class;
}

int MainHandAttacksPerRound(bool is_player_character, const std::vector<NwnClassSlot>& classes, int engine_attacks) {
  if (!is_player_character) return engine_attacks;
  return AttacksPerAttackAction(GroupClassLevels(classes));
}

int ConvertCriticalThreatFloor(int engine_threat_floor) { return engine_threat_floor > 20 ? engine_threat_floor : 20; }

int ConvertCriticalMultiplier(int engine_multiplier) { return engine_multiplier == 0 ? 0 : 2; }

int CriticalMultiplierFor(CritDamagePart part, int engine_multiplier) {
  if (engine_multiplier == 0) return 0;  // no weapon in this hand
  return part == CritDamagePart::kDice ? 2 : 1;
}

CritDamagePart EffectDamagePart(int dice_count) {
  return dice_count != 0 ? CritDamagePart::kDice : CritDamagePart::kFlatBonus;
}

int RemainingAttacksInRound(int total_attacks, int current_attack_index) {
  return std::max(0, total_attacks - 1 - current_attack_index);
}

bool IsAgainstSide(const AttackOrder& order, uint32_t attacker, int attacker_faction) {
  if (!order.exists) return false;
  if (order.target == attacker || order.target_master == attacker) return true;
  return order.target_faction >= 0 && order.target_faction == attacker_faction;
}

int NearestEnemyInTheFight(const RetargetCandidate* candidates, int count, float range) {
  int best = -1;
  float best_distance = range;
  for (int i = 0; i < count; ++i) {
    if (candidates[i].against_our_side && candidates[i].distance < best_distance) {
      best = i;
      best_distance = candidates[i].distance;
    }
  }
  return best;
}

bool IsPlausibleChallengeRating(float challenge_rating) {
  // Written so NaN fails both comparisons.
  return challenge_rating >= 0.0f && challenge_rating <= 60.0f;
}

}  // namespace rules
