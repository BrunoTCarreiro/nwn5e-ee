#pragma once

// Phase 5b (docs/PLAN.md "Phase 5 — rests, healing, death", decided by the
// owner 2026-10-23): Death Saving Throws and Exhaustion, SRD 5.2.
// - A Death Saving Throw is a D20 Test with no ability: 10 or higher a
//   success, lower a failure; a natural 1 counts as two failures; a natural
//   20 regains 1 HP. Three successes: Stable (the counts reset); three
//   failures: dead.
// - Damage at 0 HP: a failure, two from a critical hit; damage at least the
//   HP maximum kills outright. A Stable creature that takes damage is no
//   longer Stable.
// - Massive damage: dropping to 0 HP with damage left over at least the HP
//   maximum kills outright.
// - Defy Death (Champion 18, Survivor): advantage on Death Saving Throws, and
//   an 18-20 counts as a 20.
// - Exhaustion: -2 per level to D20 Tests (rules/d20.h), -5 ft speed per
//   level; level 6 is death.

#include "rules/d20.h"

namespace rules {

struct DeathSaves {
  int successes = 0;
  int failures = 0;
  bool stable = false;
};

enum class DeathSaveResult {
  kSuccess,  // a success, not yet the third
  kFailure,  // one or two failures, not yet the third
  kStable,   // the third success: Stable, the counts reset
  kDead,     // the third failure
  kWake,     // a natural 20 (Defy Death: 18-20): regains 1 HP
};

struct DeathSave {
  D20Roll roll;
  int total = 0;  // the kept die + modifier
  DeathSaveResult result = DeathSaveResult::kFailure;
};

// One Death Saving Throw. `modifier` is added to the die (the Exhaustion
// penalty); `twenty_from` is the lowest natural roll that counts as a 20
// (20, or 18 with Defy Death). A Stable creature doesn't roll: callers skip it.
DeathSave RollDeathSave(DeathSaves& saves, RollMode mode, int modifier, int twenty_from, Dice& dice);

// Damage taken at 0 HP. True when it kills: the third failure, or damage at
// least the HP maximum.
bool DamageAtZeroHitPoints(DeathSaves& saves, int damage, int max_hit_points, bool critical);

// Dropping to 0 HP with `damage_left_over` past 0: at least the maximum kills.
bool IsMassiveDamage(int damage_left_over, int max_hit_points);

// Defy Death: Champion (Fighter) 18.
bool HasDefyDeath(int fighter_level);
constexpr int kDefyDeathTwentyFrom = 18;

// Exhaustion's speed penalty, 5 ft per level, as a percent of a 30 ft speed
// (NWN's EffectMovementSpeedDecrease): 17, 33, 50, 67, 83; 0 at level 0.
int ExhaustionSpeedDecreasePercent(int exhaustion_level);

}  // namespace rules
