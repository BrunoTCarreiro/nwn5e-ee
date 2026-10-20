#include "attack_roll.h"

#include <intrin.h>

#include <vector>

#include "ee_layout.h"
#include "log.h"

namespace {

using ResolveAttackRollFn = void (*)(void* creature, void* target);
using GetAttackFn = uint8_t* (*)(void* round, int index);

ResolveAttackRollFn g_resolve_attack_roll = nullptr;
GetAttackFn g_get_attack = nullptr;
std::vector<AttackRollListener> g_before;  // filled at install, then only read
std::vector<AttackRollListener> g_after;
std::vector<AttackRollSkipper> g_skippers;
bool g_hooked = false;
bool g_hook_failed = false;
thread_local AttackRoll* t_current = nullptr;

// The attack ResolveAttackRoll is about to resolve: it reads the same round
// and index first thing (EE +0x31..+0x42).
uint8_t* AttackDataFor(void* creature) {
  if (!g_get_attack || !creature) return nullptr;
  void* round = *reinterpret_cast<void**>(static_cast<uint8_t*>(creature) + ee::kCreatureCombatRound);
  if (!round) return nullptr;
  const int index = *(static_cast<uint8_t*>(round) + ee::kRoundCurrentAttack);
  return g_get_attack(round, index);
}

void ResolveAttackRoll(void* creature, void* target) {
  if (!target) {  // the engine returns at once without a target
    g_resolve_attack_roll(creature, target);
    return;
  }
  AttackRoll roll;
  roll.attacker = creature;
  roll.target = target;
  roll.data = AttackDataFor(creature);
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  for (AttackRollSkipper skipper : g_skippers) {
    if (skipper(roll, caller)) return;
  }
  AttackRoll* outer = t_current;
  t_current = &roll;
  for (AttackRollListener listener : g_before) listener(roll);
  g_resolve_attack_roll(creature, target);
  for (AttackRollListener listener : g_after) listener(roll);
  t_current = outer;
}

}  // namespace

bool AddAttackRollListeners(AttackRollListener before, AttackRollListener after) {
  if (!g_hooked && !g_hook_failed) {
    g_get_attack = reinterpret_cast<GetAttackFn>(EngineExport(ee::kGetAttack));
    if (!g_get_attack) Log("note: no export %s - attack data unknown to the attack-roll rules", ee::kGetAttack);
    g_hooked = HookExport("CNWSCreature::ResolveAttackRoll", ee::kResolveAttackRoll,
                          reinterpret_cast<void*>(&ResolveAttackRoll),
                          reinterpret_cast<void**>(&g_resolve_attack_roll));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  if (before) g_before.push_back(before);
  if (after) g_after.push_back(after);
  return true;
}

bool AddAttackRollSkipper(AttackRollSkipper skipper) {
  if (!AddAttackRollListeners(nullptr, nullptr)) return false;
  g_skippers.push_back(skipper);
  return true;
}

AttackRoll* CurrentAttackRoll() { return t_current; }

uint8_t* CurrentAttackData(void* creature) {
  if (!g_get_attack) g_get_attack = reinterpret_cast<GetAttackFn>(EngineExport(ee::kGetAttack));
  return AttackDataFor(creature);
}

CodeRange ResolveAttackRollCode() {
  static const CodeRange code = FunctionAt(EngineExport(ee::kResolveAttackRoll));
  return code;
}
