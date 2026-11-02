// Phase 6 part 2 (#120; docs/PLAN.md "Phase 6 — Action economy"; the owner, 2026-11-02, on #51: "At the end of
// the queue, if the PC ever targeted an enemy for normal attacks, a normal attack should be placed as a default
// action to be used if there are no other actions in the queue"; the design's point 2: "the last enemy ordered
// attacked, while alive and in sight; when it dies, the nearest enemy in sight; out of combat, nothing"),
// `actions` faithful - a player character's:
// - Armed by an attack order on an enemy (an AttackObject action - CNWSObject::AddAction: the player's click, a
//   script's ActionAttack, the default attack's own): that enemy is remembered.
// - When its queue runs dry in a fight - no orders, commandable (GetCommandable, +0xEC), not dead or dying; tried
//   at most once a second - it attacks, as a click would (CNWSCreature::AddAttackActions): the remembered enemy
//   while it stands (alive, an enemy, seen), else the nearest enemy it sees (rules::DefaultAttackRange: 10 m with a
//   melee weapon, [rules] ranged_retarget_range with a ranged one), which becomes the remembered one.
// - Disarmed when the fight is over (out of combat once it was in it; never in it a minute after the order), when
//   it dies, by a move order of the player's (a click on the ground - CNWSMessage::
//   HandlePlayerToServerInputWalkToWaypoint - or the keys, InputDriveControl: moved on purpose, it isn't walked back
//   into the fight; not a combat step, which keeps attacking), and by a click on its tile in the action queue strip;
//   armed again by the next attack order.
// - Players only: the AI fills monsters' and henchmen's queues itself, round by round.
// - In faithful mode it replaces the potion resume (rules_potion.cpp; NWN mode keeps that one).

#include "default_attack.h"

#include <windows.h>

#include <stdint.h>

#include <unordered_map>

#include "action_guard.h"
#include "action_order.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "rules/action_economy.h"

bool HoldsRangedWeapon(void* creature);  // light_sources.cpp (baseitems.2da RangedWeapon)

namespace {

constexpr const char* kWalkToWaypoint =
    "?HandlePlayerToServerInputWalkToWaypoint@CNWSMessage@@QEAAHPEAVCNWSPlayer@@@Z";
constexpr const char* kDriveControl = "?HandlePlayerToServerInputDriveControl@CNWSMessage@@QEAAHPEAVCNWSPlayer@@@Z";
constexpr const char* kPlayerGameObject = "?GetGameObject@CNWSPlayer@@QEAAPEAVCNWSObject@@XZ";
constexpr size_t kObjectCommandable = 0xEC;  // CGameObject: GetCommandable (ExecuteCommandGetCommandable, 89.8193.37)
constexpr ULONGLONG kRetryMs = 1000;         // a refused or failed attack order isn't given every update
constexpr ULONGLONG kNeverFoughtMs = 60000;  // armed, never in a fight: a stale order

// (target, target-switched flag, clear all actions first, add to the front) - a click is (t, 0, 1, 0).
using AddAttackActionsFn = int (*)(void* creature, uint32_t target, int switched, int clear_first, int front);
using NearestEnemyFn = uint32_t (*)(void* creature, float range, uint32_t exclude, int must_be_seen, int check_path);
using PlayerGameObjectFn = void* (*)(void* player);
using InputFn = int (*)(void* message, void* player);

struct Api {
  AddAttackActionsFn add_attack_actions = reinterpret_cast<AddAttackActionsFn>(EngineExport(ee::kAddAttackActions));
  NearestEnemyFn nearest_enemy = reinterpret_cast<NearestEnemyFn>(EngineExport(ee::kGetNearestEnemy));
  PlayerGameObjectFn player_object = reinterpret_cast<PlayerGameObjectFn>(EngineExport(kPlayerGameObject));
};

const Api& E() {
  static const Api api;
  return api;
}

struct Armed {
  uint32_t target = ee::kObjectInvalid;
  ULONGLONG armed_at = 0;
  ULONGLONG next_try = 0;
  bool fought = false;  // seen in combat since it was armed
};
std::unordered_map<uint32_t, Armed> g_armed;  // by player character id; the engine's server thread
InputFn g_walk = nullptr;
InputFn g_drive = nullptr;
int g_lines = 0;

bool Faithful() { return g_config.actions == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_lines < 400 && ++g_lines; }
uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }
bool InCombat(const void* creature) { return Field<int32_t>(creature, ee::kCreatureInCombat) != 0; }

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Disarm(void* creature, const char* why) {
  const auto it = creature ? g_armed.find(IdOf(creature)) : g_armed.end();
  if (it == g_armed.end()) return;
  if (LogSlot()) Log("default attack: 0x%08X off (%s)", it->first, why);
  g_armed.erase(it);
}

// CNWSObject::AddAction (never refuses): an attack order on an enemy arms a player character's default attack.
bool NoteAttackOrder(void* object, uint32_t action, uint32_t type1, const void* param1) {
  if (action != ee::kActionAttackObject || type1 != kActionParamObject || !param1 || !Faithful()) return false;
  const uint32_t id = IdOf(object);
  void* creature = CreatureById(id);
  if (!creature || !IsPlayer(creature)) return false;
  const uint32_t target = *static_cast<const uint32_t*>(param1);
  if (!CreatureById(target) || !engine::IsEnemy(creature, target)) return false;
  Armed& armed = g_armed[id];
  if (armed.target != target && LogSlot()) Log("default attack: 0x%08X on 0x%08X (an attack order)", id, target);
  if (armed.target == ee::kObjectInvalid) armed.armed_at = GetTickCount64();
  armed.target = target;
  return false;
}

// A move order of the player's: the character was moved on purpose - no default attack walks it back. Unless it
// still attacks after it (a combat step).
void DisarmUnlessAttacking(void* player, const char* why) {
  if (g_armed.empty() || !player || !E().player_object) return;
  void* object = E().player_object(player);
  void* creature = object ? CreatureById(IdOf(object)) : nullptr;
  if (!creature) return;
  for (void* node : QueueNodes(creature)) {
    if (Field<int32_t>(node, ee::kActionId) == ee::kActionAttackObject) return;
  }
  Disarm(creature, why);
}

int OnWalkToWaypoint(void* message, void* player) {
  const int result = g_walk(message, player);
  DisarmUnlessAttacking(player, "a move order");
  return result;
}

int OnDriveControl(void* message, void* player) {
  const int result = g_drive(message, player);
  DisarmUnlessAttacking(player, "moved by the keys");
  return result;
}

void OnCreatureUpdate(void* creature) {
  if (g_armed.empty() || !creature) return;
  const auto it = g_armed.find(IdOf(creature));
  if (it == g_armed.end()) return;
  Armed& armed = it->second;
  const ULONGLONG now = GetTickCount64();
  if (engine::IsDeadOrDying(creature)) {
    Disarm(creature, "dead or dying");
    return;
  }
  if (InCombat(creature)) {
    armed.fought = true;
  } else {
    if (armed.fought) {
      Disarm(creature, "the fight is over");
    } else if (now - armed.armed_at > kNeverFoughtMs) {
      Disarm(creature, "no fight a minute after the order");
    }
    return;
  }
  if (!engine::HasNoOrders(creature) || Field<int32_t>(creature, kObjectCommandable) == 0) return;
  if (now < armed.next_try) return;
  armed.next_try = now + kRetryMs;
  const uint32_t id = it->first;
  const uint32_t last = armed.target;
  void* last_creature = CreatureById(last);
  const bool stands = last_creature && !engine::IsDeadOrDying(last_creature) && engine::IsEnemy(creature, last) &&
                      engine::Sees(creature, last);
  uint32_t nearest = ee::kObjectInvalid;
  if (!stands && E().nearest_enemy) {
    const float range = rules::DefaultAttackRange(HoldsRangedWeapon(creature), g_config.ranged_retarget_range);
    nearest = E().nearest_enemy(creature, range, ee::kObjectInvalid, 1, 1);
  }
  const rules::DefaultAttackPick pick = rules::PickDefaultAttack(true, true, stands, nearest != ee::kObjectInvalid);
  if (pick == rules::DefaultAttackPick::kNone || !E().add_attack_actions) return;  // none in sight: kept armed
  const uint32_t target = pick == rules::DefaultAttackPick::kLastTarget ? last : nearest;
  armed.target = target;
  E().add_attack_actions(creature, target, 0, 1, 0);  // as a click
  if (LogSlot()) {
    Log("default attack: 0x%08X attacks 0x%08X (%s)", id, target,
        pick == rules::DefaultAttackPick::kLastTarget ? "its queue ran dry"
                                                      : "its last target fell or went out of sight: the nearest enemy");
  }
}

}  // namespace

uint32_t DefaultAttackTarget(void* creature) {
  if (!Faithful() || g_armed.empty() || !creature) return ee::kObjectInvalid;
  const auto it = g_armed.find(IdOf(creature));
  if (it == g_armed.end() || !InCombat(creature)) return ee::kObjectInvalid;
  return it->second.target;
}

bool CancelDefaultAttack(void* creature) {
  if (!creature || g_armed.find(IdOf(creature)) == g_armed.end()) return false;
  Disarm(creature, "its tile clicked in the action queue");
  return true;
}

void InstallDefaultAttack() {
  if (!Faithful()) return;
  const bool orders = AddActionGuard(&NoteAttackOrder);
  const bool updates = AddCreatureUpdateListener(&OnCreatureUpdate);
  const bool walk = HookExport("CNWSMessage::HandlePlayerToServerInputWalkToWaypoint (the default attack)",
                               kWalkToWaypoint, reinterpret_cast<void*>(&OnWalkToWaypoint),
                               reinterpret_cast<void**>(&g_walk));
  const bool drive = HookExport("CNWSMessage::HandlePlayerToServerInputDriveControl (the default attack)",
                                kDriveControl, reinterpret_cast<void*>(&OnDriveControl),
                                reinterpret_cast<void**>(&g_drive));
  if (!orders || !updates || !E().add_attack_actions) {
    Log("SKIP default attack: %s", !orders    ? "the attack orders aren't hooked"
                                   : !updates ? "no creature updates"
                                              : "no AddAttackActions");
    return;
  }
  Log("default attack: on - a player character's queue run dry in a fight attacks its last target, else the nearest "
      "enemy it sees%s", walk && drive ? "; a move order turns it off" : " (a move order is NOT seen)");
}
