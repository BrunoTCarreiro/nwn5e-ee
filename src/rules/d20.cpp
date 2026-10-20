#include "rules/d20.h"

#include <algorithm>

namespace rules {

RollMode ResolveRollMode(int advantage_sources, int disadvantage_sources) {
  const bool advantage = advantage_sources > 0;
  const bool disadvantage = disadvantage_sources > 0;
  if (advantage == disadvantage) return RollMode::kNormal;
  return advantage ? RollMode::kAdvantage : RollMode::kDisadvantage;
}

D20Roll RollD20(RollMode mode, Dice& dice) {
  const int first = dice.Roll(20);
  if (mode == RollMode::kNormal) return {first, first, 0};
  const int second = dice.Roll(20);
  const int kept = mode == RollMode::kAdvantage ? std::max(first, second) : std::min(first, second);
  return {kept, first, second};
}

int ExhaustionD20Penalty(int exhaustion_level) { return -2 * std::max(exhaustion_level, 0); }

bool IsDeadFromExhaustion(int exhaustion_level) { return exhaustion_level >= 6; }

}  // namespace rules
