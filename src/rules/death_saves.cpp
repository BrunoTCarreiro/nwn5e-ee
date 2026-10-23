#include "rules/death_saves.h"

#include <algorithm>

namespace rules {

namespace {

constexpr int kSavesNeeded = 3;
constexpr int kDeathSaveDc = 10;

DeathSaveResult AddFailures(DeathSaves& saves, int failures) {
  saves.failures += failures;
  return saves.failures >= kSavesNeeded ? DeathSaveResult::kDead : DeathSaveResult::kFailure;
}

}  // namespace

DeathSave RollDeathSave(DeathSaves& saves, RollMode mode, int modifier, int twenty_from, Dice& dice) {
  DeathSave save;
  save.roll = RollD20(mode, dice);
  save.total = save.roll.kept + modifier;
  if (save.roll.kept >= twenty_from) {
    saves = DeathSaves{};  // regaining HP resets the counts
    save.result = DeathSaveResult::kWake;
  } else if (save.roll.kept == 1) {
    save.result = AddFailures(saves, 2);
  } else if (save.total >= kDeathSaveDc) {
    if (++saves.successes >= kSavesNeeded) {
      saves = DeathSaves{};
      saves.stable = true;
      save.result = DeathSaveResult::kStable;
    } else {
      save.result = DeathSaveResult::kSuccess;
    }
  } else {
    save.result = AddFailures(saves, 1);
  }
  return save;
}

bool DamageAtZeroHitPoints(DeathSaves& saves, int damage, int max_hit_points, bool critical) {
  if (damage <= 0) return false;
  if (damage >= max_hit_points) return true;
  saves.stable = false;
  return AddFailures(saves, critical ? 2 : 1) == DeathSaveResult::kDead;
}

bool IsMassiveDamage(int damage_left_over, int max_hit_points) {
  return max_hit_points > 0 && damage_left_over >= max_hit_points;
}

bool HasDefyDeath(int fighter_level) { return fighter_level >= 18; }

int ExhaustionSpeedDecreasePercent(int exhaustion_level) {
  const int level = std::clamp(exhaustion_level, 0, 5);
  return (level * 5 * 100 + 15) / 30;  // rounded: 17, 33, 50, 67, 83
}

}  // namespace rules
