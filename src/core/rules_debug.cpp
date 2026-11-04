// Test switches that ride on the attack roll ([debug] in nwn5e.ini):
//
// - creature_hp_multiplier / creature_min_hp: the first time a character
//   attacks a creature that is not a character, its current hit points are
//   multiplied and raised to the minimum, once per creature, so weak monsters
//   survive several hits. Remembered per creature object in memory, so a
//   creature loaded again (a new object) is boosted again. 0.72.1 (owner,
//   2026-11-04: "somehow while testing HS-1 the orc does immediately from one
//   acid flask" - a grenade makes no attack roll, and the orc's 4 hit points
//   went at its first failed save): also before a creature's save against a
//   character (a grenade, a bomb's cloud, a spell), and as !spawn makes a
//   hostile one (a flask's patch has neither).
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

bool BoostWanted() { return g_config.debug_creature_hp_multiplier > 1 || g_config.debug_creature_min_hp > 0; }

// `target` (a creature, not a character - or, `party_too`, a !spawn ally: a henchman counts as a character) boosted
// once: its hit points multiplied and raised to the minimum.
void Boost(void* target, bool party_too = false) {
  const int multiplier = g_config.debug_creature_hp_multiplier > 1 ? g_config.debug_creature_hp_multiplier : 1;
  const int minimum = g_config.debug_creature_min_hp;
  void* target_stats = target ? StatsOf(target) : nullptr;
  if (!target_stats || (!party_too && ReadCreatureFacts(target_stats).character)) return;
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

void BoostTestTarget(AttackRoll& roll) {
  if (!BoostWanted()) return;
  void* attacker_stats = StatsOf(roll.attacker);
  if (!attacker_stats || !ReadCreatureFacts(attacker_stats).character) return;
  Boost(CreatureById(Field<uint32_t>(roll.target, ee::kObjectId)));
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

// 0.72.1: before a creature's save against a character (rules_save_roll.cpp), and as !spawn makes a hostile one
// (test_commands.cpp).
void BoostTestCreatureForSave(void* creature, uint32_t versus) {
  if (!BoostWanted() || !creature) return;
  void* source = CreatureById(versus);
  void* source_stats = source ? StatsOf(source) : nullptr;
  if (!source_stats || !ReadCreatureFacts(source_stats).character) return;
  Boost(creature);
}

// 0.73.1 (owner: "the henchmen will need to be made stronger, they can't deal with a firebomb" - the !spawn ally,
// a level 1 orc, kept its 4 hit points): an ally spawned for a test gets the floor too.
void BoostTestCreatureById(uint32_t id, bool ally) {
  if (BoostWanted()) Boost(CreatureById(id), ally);
}

void InstallDebugSwitches() {
  if (g_config.debug_creature_hp_multiplier > 1 || g_config.debug_creature_min_hp > 0 ||
      g_config.debug_test_commands) {
    AddAttackRollListeners(&BoostTestTarget, nullptr);
  }
  if (g_config.debug_attack_trace || g_config.debug_test_commands) AddAttackRollListeners(nullptr, &TraceAttackRoll);
}
