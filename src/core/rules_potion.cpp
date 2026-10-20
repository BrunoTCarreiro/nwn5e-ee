// Owner request (2026-10-19): after a player character drinks a potion in a
// fight it goes back to attacking — NWN drops the attack order when an item
// is used (EE too: tested 2026-10-20, EP20). [rules] resume_attack_after_potion.
// (Port of 1.69's rules_potion.cpp; docs/EE-PORT.md E2.)
//
// EE: CNWSCreature::UseItem queues the drink; right after it, for a player
// character drinking a potion (base items 49, 54, 104) whose last attack was
// within 30 s, an attack on that target is queued behind the drink — or, if
// that target fell, on the nearest enemy it sees within 10 m. The attack
// starts when the drink ends; no polling.

#include <stdint.h>

#include <windows.h>

#include <mutex>
#include <unordered_map>

#include "attack_roll.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"

namespace {

struct Vec3 {
  float x, y, z;
};

using UseItemFn = int (*)(void* creature, uint32_t item, uint8_t active_property, uint8_t sub_property,
                          uint32_t target, Vec3 location, uint32_t area, int flag);
using AddAttackActionsFn = int (*)(void* creature, uint32_t target, int a, int b, int c);
using NearestEnemyFn = uint32_t (*)(void* creature, float range, uint32_t exclude, int must_be_seen, int check_path);
using ItemByIdFn = void* (*)(void* app, uint32_t id);

UseItemFn g_use_item = nullptr;

struct Api {
  AddAttackActionsFn add_attack_actions = reinterpret_cast<AddAttackActionsFn>(EngineExport(ee::kAddAttackActions));
  NearestEnemyFn nearest_enemy = reinterpret_cast<NearestEnemyFn>(EngineExport(ee::kGetNearestEnemy));
  ItemByIdFn item_by_id = reinterpret_cast<ItemByIdFn>(EngineExport(ee::kGetItemByGameObjectId));
  void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
};

const Api& E() {
  static const Api api;
  return api;
}

struct Noted {
  uint32_t target;
  DWORD tick;
};
std::unordered_map<uint32_t, Noted> g_last_target;  // player character -> its last attack's target
std::mutex g_lock;
constexpr DWORD kLastTargetMaxMs = 30000;  // a fight's last attack, not an old one
constexpr float kFallbackRange = 10.0f;
int g_log_lines = 0;

bool LogSlot() { return g_config.log_attacks && g_log_lines < 200 && ++g_log_lines; }

void NoteTarget(AttackRoll& roll) {
  void* stats = StatsOf(roll.attacker);
  if (!stats || !ReadCreatureFacts(stats).is_player) return;
  std::lock_guard<std::mutex> lock(g_lock);
  g_last_target[Field<uint32_t>(roll.attacker, ee::kObjectId)] =
      Noted{Field<uint32_t>(roll.target, ee::kObjectId), GetTickCount()};
}

bool IsPotion(uint32_t item_id) {
  if (!E().item_by_id || !E().app_manager || !*E().app_manager) return false;
  void* app = Field<void*>(*E().app_manager, ee::kAppManagerServerApp);
  void* item = app ? E().item_by_id(app, item_id) : nullptr;
  const int row = engine::BaseItemRow(item);
  return row == 49 || row == 54 || row == 104;
}

int UseItem(void* creature, uint32_t item, uint8_t active_property, uint8_t sub_property, uint32_t target,
            Vec3 location, uint32_t area, int flag) {
  const int result = g_use_item(creature, item, active_property, sub_property, target, location, area, flag);
  void* stats = StatsOf(creature);
  if (!result || !stats || !ReadCreatureFacts(stats).is_player || !IsPotion(item)) return result;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  uint32_t resume = ee::kObjectInvalid;
  {
    std::lock_guard<std::mutex> lock(g_lock);
    const auto last = g_last_target.find(id);
    if (last == g_last_target.end() || GetTickCount() - last->second.tick > kLastTargetMaxMs) return result;
    resume = last->second.target;
  }
  void* last = CreatureById(resume);
  const bool last_stands = last && !engine::IsDeadOrDying(last) && engine::IsEnemy(creature, resume) &&
                           engine::Sees(creature, resume);
  if (!last_stands) {
    resume = E().nearest_enemy ? E().nearest_enemy(creature, kFallbackRange, ee::kObjectInvalid, 1, 0) : ee::kObjectInvalid;
  }
  if (resume == ee::kObjectInvalid || !E().add_attack_actions) {
    if (LogSlot()) Log("potion: 0x%08X drinks mid-fight; no enemy to go back to", id);
    return result;
  }
  E().add_attack_actions(creature, resume, 0, 0, 0);
  if (LogSlot()) {
    Log("potion: 0x%08X drinks mid-fight; attacks 0x%08X after it%s", id, resume,
        last_stands ? "" : " (its last target fell: the nearest enemy it sees)");
  }
  return result;
}

}  // namespace

void InstallPotionRules() {
  if (!g_config.resume_attack_after_potion) return;
  if (!AddAttackRollListeners(&NoteTarget, nullptr)) return;
  HookExport("CNWSCreature::UseItem (back to attacking after a potion)", ee::kUseItem,
             reinterpret_cast<void*>(&UseItem), reinterpret_cast<void**>(&g_use_item));
}
