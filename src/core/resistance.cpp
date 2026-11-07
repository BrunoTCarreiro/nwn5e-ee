#include "resistance.h"

#include <algorithm>
#include <vector>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using DoDamageImmunityFn = int (*)(void* object, void* attacker, int damage, uint32_t flags, int simulate,
                                   int feedback);

DoDamageImmunityFn g_do_damage_immunity = nullptr;
std::vector<ResistanceProvider> g_providers;  // filled at install, then only read
std::vector<DamageReducer> g_reducers;        // the same
bool g_hooked = false;
bool g_hook_failed = false;
int g_log_lines = 0;

int DoDamageImmunity(void* object, void* attacker, int damage, uint32_t flags, int simulate, int feedback) {
  int left = g_do_damage_immunity(object, attacker, damage, flags, simulate, feedback);
  if (!object || left <= 0) return left;
  if (!g_reducers.empty()) {
    int off = 0;
    for (DamageReducer reducer : g_reducers) off += reducer(object, flags, left - off, simulate != 0);
    off = std::min(std::max(off, 0), left);
    left -= off;
    damage -= off;
    if (left <= 0) return 0;
  }
  uint32_t resisted = 0;
  for (ResistanceProvider provider : g_providers) resisted |= provider(object);
  if (!(resisted & flags)) return left;
  const int halved = std::min(left, damage / 2);
  if (!simulate && g_config.log_attacks && g_log_lines < 300) {
    ++g_log_lines;
    Log("resistance: 0x%08X resists damage type 0x%X: %d -> %d", Field<uint32_t>(object, ee::kObjectId), flags,
        left, halved);
  }
  return halved;
}

}  // namespace

bool Hooked() {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSObject::DoDamageImmunity (X4: 2024 Resistance)", ee::kDoDamageImmunity,
                          reinterpret_cast<void*>(&DoDamageImmunity), reinterpret_cast<void**>(&g_do_damage_immunity));
    g_hook_failed = !g_hooked;
  }
  return g_hooked;
}

bool AddResistanceProvider(ResistanceProvider provider) {
  if (!Hooked()) return false;
  g_providers.push_back(provider);
  return true;
}

bool AddDamageReducer(DamageReducer reducer) {
  if (!Hooked()) return false;
  g_reducers.push_back(reducer);
  return true;
}
