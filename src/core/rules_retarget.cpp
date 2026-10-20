// Decision 14 (2024 Extra Attack): when an attack kills its target and the
// attacker has attacks left this round, the remaining attacks go to the
// nearest enemy instead of being lost (port of 1.69's rules_retarget.cpp;
// docs/EE-PORT.md E1).
//
// The round's target (combat round +0x2f4c) is switched without adding an
// attack, the way EE's own cleave switches it, after ResolvePostMeleeDamage /
// ResolvePostRangedDamage have marked the kill (attack data +0x68) — unless
// the engine already moved on (Cleave / Great Cleave, Circle Kick), so those
// keep their bonus attack as in NWN.
//
// - Melee: the engine's own cleave search, GetNewCombatTarget(killed).
// - Ranged: only an enemy already in the fight — one whose attack target
//   (NWScript's GetAttackTarget) is the attacker, one of its associates or a
//   creature of its faction (owner, 2026-10-17: a bow must not pick an idle,
//   caged or sleeping enemy) — that the attacker sees, within [rules]
//   ranged_retarget_range metres (default 40) capped at the weapon's range.

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "attack_roll.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/nwn_conversion.h"

namespace {

using PostDamageFn = void (*)(void* creature, void* target);
using TotalAttacksFn = uint8_t (*)(void* round);
using NewCombatTargetFn = void* (*)(void* creature, uint32_t exclude);
using MaxAttackRangeFn = float (*)(void* creature, uint32_t target, int a, int b);
using FactionFn = void* (*)(void* creature);

PostDamageFn g_resolve_post_melee_damage = nullptr;
PostDamageFn g_resolve_post_ranged_damage = nullptr;

struct Api {
  TotalAttacksFn total_attacks = reinterpret_cast<TotalAttacksFn>(EngineExport(ee::kGetTotalAttacks));
  NewCombatTargetFn new_combat_target = reinterpret_cast<NewCombatTargetFn>(EngineExport(ee::kGetNewCombatTarget));
  MaxAttackRangeFn max_attack_range = reinterpret_cast<MaxAttackRangeFn>(EngineExport(ee::kMaxAttackRange));
  FactionFn faction = reinterpret_cast<FactionFn>(EngineExport(ee::kGetFaction));
};

const Api& E() {
  static const Api api;
  return api;
}

LogOnce g_retarget_logged(500);

// Is `target_id` the attacker, one of its associates, or of its faction?
bool AgainstSide(void* attacker, uint32_t attacker_id, uint32_t target_id) {
  if (target_id == attacker_id) return true;
  void* target = CreatureById(target_id);
  if (!target) return false;
  if (Field<uint32_t>(target, ee::kCreatureMaster) == attacker_id) return true;
  void* faction = E().faction ? E().faction(target) : nullptr;
  return faction && faction == E().faction(attacker);
}

struct RangedSearch {
  void* attacker;
  uint32_t attacker_id;
  uint32_t exclude;
  float range;
  std::vector<rules::RetargetCandidate> candidates;
  std::vector<uint32_t> ids;
  int in_the_fight = 0;
};

void ConsiderRangedCandidate(void* context, uint32_t id, void* creature) {
  auto* search = static_cast<RangedSearch*>(context);
  if (id == search->exclude || !engine::IsEnemy(search->attacker, id) || engine::IsDeadOrDying(creature)) return;
  if (!engine::Sees(search->attacker, id)) return;
  const float distance = engine::EdgeDistance(search->attacker, creature);
  if (!(distance < search->range)) return;
  const bool fighting = AgainstSide(search->attacker, search->attacker_id,
                                    Field<uint32_t>(creature, ee::kCreatureAttackTarget));
  search->candidates.push_back({distance, fighting});
  search->ids.push_back(id);
  if (fighting) ++search->in_the_fight;
}

void RetargetAfterKill(void* attacker, void* round, uint32_t killed, bool ranged) {
  if (!E().total_attacks) return;
  const int total = E().total_attacks(round);
  const int index = Field<uint8_t>(round, ee::kRoundCurrentAttack);
  if (rules::RemainingAttacksInRound(total, index) <= 0) return;
  const uint32_t id = Field<uint32_t>(attacker, ee::kObjectId);
  uint32_t next = ee::kObjectInvalid;
  RangedSearch search{attacker, id, killed, 0.0f, {}, {}, 0};
  if (ranged) {
    const float weapon = E().max_attack_range ? E().max_attack_range(attacker, ee::kObjectInvalid, 0, 0) : 40.0f;
    search.range = std::min(static_cast<float>(g_config.ranged_retarget_range), weapon);
    engine::ForEachOtherCreatureInArea(attacker, &ConsiderRangedCandidate, &search);
    const int best = rules::NearestEnemyInTheFight(search.candidates.data(), static_cast<int>(search.candidates.size()),
                                                   search.range);
    if (best >= 0) next = search.ids[best];
  } else if (E().new_combat_target) {
    if (void* creature = E().new_combat_target(attacker, killed)) next = Field<uint32_t>(creature, ee::kObjectId);
  }
  if (next != ee::kObjectInvalid) {
    SetField<uint32_t>(round, ee::kRoundTarget, next);
    SetField<int32_t>(attacker, ee::kCreatureTargetSwitched, 1);
  }
  if (!g_config.log_attacks || !g_retarget_logged.First(LogOnce::Key(id ^ killed, index, total))) return;
  if (ranged) {
    Log("retarget (ranged): object 0x%08X killed 0x%08X with attack %d of %d; %zu seen enemies within %.0f m, %d in "
        "the fight; %s 0x%08X",
        id, killed, index + 1, total, search.candidates.size(), search.range, search.in_the_fight,
        next != ee::kObjectInvalid ? "remaining attacks go to" : "none picked", next);
  } else {
    Log("retarget (melee): object 0x%08X killed 0x%08X with attack %d of %d; %s 0x%08X", id, killed, index + 1, total,
        next != ee::kObjectInvalid ? "remaining attacks go to" : "no enemy in reach", next);
  }
}

void AfterPostDamage(void* creature, void* target, bool ranged, uint32_t round_target_before) {
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  const uint8_t* data = CurrentAttackData(creature);
  if (!round || !target || !data || Field<int32_t>(data, ee::kAttackKilledTarget) != 1) return;
  if (Field<uint32_t>(round, ee::kRoundTarget) != round_target_before) return;  // the engine moved on (cleave)
  RetargetAfterKill(creature, round, Field<uint32_t>(target, ee::kObjectId), ranged);
}

uint32_t RoundTarget(void* creature) {
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  return round ? Field<uint32_t>(round, ee::kRoundTarget) : ee::kObjectInvalid;
}

void ResolvePostMeleeDamage(void* creature, void* target) {
  const uint32_t before = RoundTarget(creature);
  g_resolve_post_melee_damage(creature, target);
  AfterPostDamage(creature, target, false, before);
}

void ResolvePostRangedDamage(void* creature, void* target) {
  const uint32_t before = RoundTarget(creature);
  g_resolve_post_ranged_damage(creature, target);
  AfterPostDamage(creature, target, true, before);
}

}  // namespace

void InstallRetargetRules() {
  if (!g_config.retarget_remaining_attacks) return;
  HookExport("CNWSCreature::ResolvePostMeleeDamage (remaining attacks move on)", ee::kResolvePostMeleeDamage,
             reinterpret_cast<void*>(&ResolvePostMeleeDamage), reinterpret_cast<void**>(&g_resolve_post_melee_damage));
  HookExport("CNWSCreature::ResolvePostRangedDamage (remaining attacks move on)", ee::kResolvePostRangedDamage,
             reinterpret_cast<void*>(&ResolvePostRangedDamage),
             reinterpret_cast<void**>(&g_resolve_post_ranged_damage));
}
