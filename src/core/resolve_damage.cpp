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

void ResolveDamage(void* attacker, void* target) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  for (ResolveDamageSkip skip : g_skips) {
    if (skip(attacker, target, caller)) return;
  }
  g_resolve_damage(attacker, target);
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
