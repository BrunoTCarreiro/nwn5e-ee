// Owner request (2026-10-19): after a player character drinks a potion in a
// fight it goes back to attacking — NWN drops the attack order when an item
// is used (EE too: tested 2026-10-20, EP20). [rules] resume_attack_after_potion.
// (Port of 1.69's rules_potion.cpp; docs/EE-PORT.md E2.)
//
// EE: CNWSCreature::UseItem queues the drink (item_use.cpp's shared hook, 0.50.0). For a player character in
// combat drinking a potion (base items 49, 54, 104) whose last attack was
// within 30 s, that target is noted; once the drink is over and the character
// has no orders (its per-creature update), the attack is given as a click
// would: on that target, or — if it fell — on the nearest enemy it sees and
// can reach within 10 m, and only while it is still in combat.
// 0.4.4 fix (owner's test of 0.4.3): 0.3.0 queued the attack right behind the
// drink at UseItem, and EE then dropped the drink — the potion was never
// drunk mid-fight, and after the fight it only sent the character at a
// nearby hostile.
// 0.5.3 (owner's test of 0.5.2): the Fighter's Second Wind (a bonus action in
// 2024) drops the attack order the same way — CNWSCreature::UseFeat queues
// the cast and the attack is lost; the same resume follows a Second Wind
// used mid-fight. 0.22.0: Rage (Barbarian B1) too; UseFeat is the shared
// hook (feat_use.cpp).

#include <stdint.h>

#include <windows.h>

#include <unordered_map>

#include "attack_roll.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "feat_use.h"
#include "hooks.h"
#include "item_use.h"
#include "log.h"
#include "rules/class_features.h"

namespace {

// (target, target-switched flag, clear all actions first, add to the front) — a click is (t, 0, 1, 0).
using AddAttackActionsFn = int (*)(void* creature, uint32_t target, int switched, int clear_first, int front);
using NearestEnemyFn = uint32_t (*)(void* creature, float range, uint32_t exclude, int must_be_seen, int check_path);
struct Api {
  AddAttackActionsFn add_attack_actions = reinterpret_cast<AddAttackActionsFn>(EngineExport(ee::kAddAttackActions));
  NearestEnemyFn nearest_enemy = reinterpret_cast<NearestEnemyFn>(EngineExport(ee::kGetNearestEnemy));
};

const Api& E() {
  static const Api api;
  return api;
}

struct Noted {
  uint32_t target;
  DWORD tick;
  const char* what = "";  // what interrupted the attack: "drinking", "Second Wind"
};
// The engine's server thread only.
std::unordered_map<uint32_t, Noted> g_last_target;  // player character -> its last attack's target
std::unordered_map<uint32_t, Noted> g_resume;       // drinking -> the target to go back to
constexpr DWORD kLastTargetMaxMs = 30000;           // a fight's last attack, not an old one
constexpr DWORD kResumeWindowMs = 10000;            // the drink and its animation
constexpr float kFallbackRange = 10.0f;
int g_log_lines = 0;

bool LogSlot() { return g_config.log_attacks && g_log_lines < 200 && ++g_log_lines; }
uint32_t IdOf(const void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }
bool InCombat(const void* creature) { return Field<int32_t>(creature, ee::kCreatureInCombat) != 0; }

void NoteTarget(AttackRoll& roll) {
  void* stats = StatsOf(roll.attacker);
  if (!stats || !ReadCreatureFacts(stats).is_player) return;
  g_last_target[IdOf(roll.attacker)] = Noted{IdOf(roll.target), GetTickCount()};
}

bool IsPotion(const void* item) {
  const int row = engine::BaseItemRow(item);
  return row == 49 || row == 54 || row == 104;
}

// A player character in combat starts something that drops its attack:
// note the last target (if it attacked within 30 s) to go back to after it.
void NoteResume(void* creature, const char* what) {
  const uint32_t id = IdOf(creature);
  const auto last = g_last_target.find(id);
  if (last == g_last_target.end() || GetTickCount() - last->second.tick > kLastTargetMaxMs) return;
  g_resume[id] = Noted{last->second.target, GetTickCount(), what};
  if (LogSlot()) {
    Log("resume: 0x%08X %s mid-fight; will go back to attacking 0x%08X after it", id, what, last->second.target);
  }
}

// After a use the engine queued (item_use.cpp).
void AfterItemUse(void* creature, uint32_t, void* item) {
  void* stats = StatsOf(creature);
  if (!stats || !ReadCreatureFacts(stats).is_player || !InCombat(creature) || !IsPotion(item)) return;
  NoteResume(creature, "drinking");
}

// Second Wind (Fighter F2): the feat rows nwn5e grants; Rage (Barbarian B1).
const char* ResumesAfterFeat(int feat) {
  if (feat >= rules::kFeatSecondWind2 && feat <= rules::kFeatSecondWind4) return "Second Wind";
  if (rules::IsRageFeat(feat)) return "Rage";
  return nullptr;
}

// After a feat use the engine queued (feat_use.cpp).
void AfterFeatUse(void* creature, int feat) {
  const char* what = ResumesAfterFeat(feat);
  if (!what) return;
  void* stats = StatsOf(creature);
  if (stats && ReadCreatureFacts(stats).is_player && InCombat(creature)) NoteResume(creature, what);
}

// After the engine's update: the drink is over when the character has no
// orders left (a new order from the player cancels the resume).
void OnCreatureUpdate(void* creature) {
  if (g_resume.empty()) return;
  const auto it = g_resume.find(IdOf(creature));
  if (it == g_resume.end()) return;
  if (GetTickCount() - it->second.tick > kResumeWindowMs || engine::IsDeadOrDying(creature)) {
    g_resume.erase(it);
    return;
  }
  if (!engine::HasNoOrders(creature)) return;  // still drinking (or given another order)
  const uint32_t id = it->first;
  uint32_t target = it->second.target;
  const char* what = it->second.what;
  g_resume.erase(it);
  if (!InCombat(creature)) {
    if (LogSlot()) Log("resume: 0x%08X done %s; the fight is over, no attack", id, what);
    return;
  }
  void* last = CreatureById(target);
  const bool last_stands = last && !engine::IsDeadOrDying(last) && engine::IsEnemy(creature, target) &&
                           engine::Sees(creature, target);
  if (!last_stands) {
    target = E().nearest_enemy ? E().nearest_enemy(creature, kFallbackRange, ee::kObjectInvalid, 1, 1)
                               : ee::kObjectInvalid;
  }
  if (target == ee::kObjectInvalid || !E().add_attack_actions) {
    if (LogSlot()) Log("resume: 0x%08X done %s; no enemy to go back to", id, what);
    return;
  }
  E().add_attack_actions(creature, target, 0, 1, 0);  // as a click
  if (LogSlot()) {
    Log("resume: 0x%08X done %s; attacks 0x%08X again%s", id, what, target,
        last_stands ? "" : " (its last target fell: the nearest enemy it sees and can reach)");
  }
}

}  // namespace

void InstallPotionRules() {
  if (!g_config.resume_attack_after_potion) return;
  if (!AddAttackRollListeners(&NoteTarget, nullptr)) return;
  if (!AddCreatureUpdateListener(&OnCreatureUpdate)) return;
  if (!AddItemUseListener(&AfterItemUse)) Log("resume: no item-use hook - no attack after a potion");
  if (!AddFeatUseListener(&AfterFeatUse)) Log("resume: no feat-use hook - no attack after Second Wind or Rage");
}
