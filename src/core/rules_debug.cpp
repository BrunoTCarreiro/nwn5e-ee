// Test switches that ride on the attack roll ([debug] in nwn5e.ini):
//
// - creature_hp_multiplier / creature_min_hp: the first time a character
//   attacks a creature that is not a character, its current hit points are
//   multiplied and raised to the minimum, once per creature, so weak monsters
//   survive several hits. Remembered per creature object in memory, so a
//   creature loaded again (a new object) is boosted again.
// - attack_trace: each player character's attack roll with its round's
//   attack counts, d20, modifier and result.
//
// With test_commands both listen from the start and check their switch on
// every roll: a !test profile may turn them on for the session (#82).

#include <stdint.h>

#include <mutex>
#include <set>
#include <utility>

#include "attack_roll.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "log.h"

namespace {

std::set<std::pair<const void*, uint32_t>> g_boosted;  // (creature object, id)
std::mutex g_boosted_lock;
int g_trace_lines = 0;
constexpr int kTraceLimit = 400;

void BoostTestTarget(AttackRoll& roll) {
  const int multiplier = g_config.debug_creature_hp_multiplier > 1 ? g_config.debug_creature_hp_multiplier : 1;
  const int minimum = g_config.debug_creature_min_hp;
  if (multiplier <= 1 && minimum <= 0) return;
  void* target = CreatureById(Field<uint32_t>(roll.target, ee::kObjectId));
  void* attacker_stats = StatsOf(roll.attacker);
  void* target_stats = StatsOf(target);
  if (!attacker_stats || !target_stats) return;
  if (!ReadCreatureFacts(attacker_stats).character || ReadCreatureFacts(target_stats).character) return;
  const uint32_t target_id = Field<uint32_t>(target, ee::kObjectId);
  {
    std::lock_guard<std::mutex> lock(g_boosted_lock);
    if (!g_boosted.insert({target, target_id}).second) return;
  }
  const int hit_points = Field<int16_t>(target, ee::kObjectCurrentHitPoints);
  if (hit_points <= 0) return;
  int new_hit_points = hit_points * multiplier;
  if (new_hit_points < minimum) new_hit_points = minimum;
  if (new_hit_points > 30000) new_hit_points = 30000;
  if (new_hit_points == hit_points) return;
  SetField<int32_t>(target, ee::kObjectCurrentHitPoints, new_hit_points);  // as EE's SetCurrentHitPoints writes it
  Log("debug: creature 0x%08X hit points %d -> %d (creature_hp_multiplier=%d, creature_min_hp=%d)", target_id,
      hit_points, new_hit_points, multiplier, minimum);
}

void TraceAttackRoll(AttackRoll& roll) {
  if (!g_config.debug_attack_trace) return;
  void* stats = StatsOf(roll.attacker);
  if (!stats || !roll.data || g_trace_lines >= kTraceLimit) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.is_player) return;
  ++g_trace_lines;
  void* round = Field<void*>(roll.attacker, ee::kCreatureCombatRound);
  Log("trace: 0x%08X attack %d of main %d / off-hand %d / extra %d vs 0x%08X: d20 %d %+d -> result %d",
      ObjectIdOf(stats), round ? Field<uint8_t>(round, ee::kRoundCurrentAttack) + 1 : 0,
      round ? Field<int32_t>(round, ee::kRoundMainHandAttacks) : 0,
      round ? Field<int32_t>(round, ee::kRoundOffHandAttacks) : 0,
      round ? Field<int32_t>(round, ee::kRoundExtraAttacks) : 0, Field<uint32_t>(roll.target, ee::kObjectId),
      roll.data[ee::kAttackD20], Field<int32_t>(roll.data, ee::kAttackModifier), roll.data[ee::kAttackResult]);
}

}  // namespace

void InstallDebugSwitches() {
  if (g_config.debug_creature_hp_multiplier > 1 || g_config.debug_creature_min_hp > 0 ||
      g_config.debug_test_commands) {
    AddAttackRollListeners(&BoostTestTarget, nullptr);
  }
  if (g_config.debug_attack_trace || g_config.debug_test_commands) AddAttackRollListeners(nullptr, &TraceAttackRoll);
}
