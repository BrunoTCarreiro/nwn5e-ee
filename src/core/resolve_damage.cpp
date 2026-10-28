#include "resolve_damage.h"

#include <intrin.h>

#include <vector>

#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using ResolveDamageFn = void (*)(void* attacker, void* target);

ResolveDamageFn g_resolve_damage = nullptr;
std::vector<ResolveDamageSkip> g_skips;              // filled at install, then only read
std::vector<ResolveDamageListener> g_listeners;
bool g_hooked = false;
bool g_hook_failed = false;
thread_local void* t_attacker = nullptr;  // while the engine's step runs
thread_local void* t_target = nullptr;

void ResolveDamage(void* attacker, void* target) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  for (ResolveDamageSkip skip : g_skips) {
    if (skip(attacker, target, caller)) return;
  }
  void* const outer_attacker = t_attacker;
  void* const outer_target = t_target;
  t_attacker = attacker;
  t_target = target;
  g_resolve_damage(attacker, target);
  t_attacker = outer_attacker;
  t_target = outer_target;
  for (ResolveDamageListener listener : g_listeners) listener(attacker, target);
}

bool Hook() {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreature::ResolveDamage (the Shove deals none; Uncanny Dodge halves a hit)",
                          ee::kResolveDamage, reinterpret_cast<void*>(&ResolveDamage),
                          reinterpret_cast<void**>(&g_resolve_damage));
    g_hook_failed = !g_hooked;
  }
  return g_hooked;
}

}  // namespace

bool ResolvingDamage(void** attacker, void** target) {
  if (!t_attacker) return false;
  if (attacker) *attacker = t_attacker;
  if (target) *target = t_target;
  return true;
}

bool DamageRollIsEstimate(void* attacker) { return g_hooked && attacker && t_attacker != attacker; }

bool AddResolveDamageSkip(ResolveDamageSkip skip) {
  if (!Hook()) return false;
  g_skips.push_back(skip);
  return true;
}

bool AddResolveDamageListener(ResolveDamageListener listener) {
  if (!Hook()) return false;
  g_listeners.push_back(listener);
  return true;
}
