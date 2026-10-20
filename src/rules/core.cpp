#include "rules/core.h"

#include <algorithm>
#include <cmath>

namespace rules {

int AbilityModifier(int score) {
  const int above_ten = score - 10;
  // C++ division truncates toward zero; the rules round down.
  return above_ten >= 0 ? above_ten / 2 : -((1 - above_ten) / 2);
}

int ProficiencyBonusForLevel(int level) {
  const int clamped = std::clamp(level, 1, 20);
  return 2 + (clamped - 1) / 4;
}

int ProficiencyBonusForChallengeRating(double challenge_rating) {
  // CR 0 and fractional CRs share the CR 1-4 tier.
  const int whole = challenge_rating < 1 ? 1 : static_cast<int>(std::floor(challenge_rating));
  return 2 + (std::min(whole, 30) - 1) / 4;
}

int SaveDifficultyClass(int ability_modifier, int proficiency_bonus) {
  return 8 + proficiency_bonus + ability_modifier;
}

int SpellAttackBonus(int ability_modifier, int proficiency_bonus) { return proficiency_bonus + ability_modifier; }

}  // namespace rules
