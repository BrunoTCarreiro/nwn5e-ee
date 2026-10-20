#include "sheet.h"

#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"

namespace {

using UpdateCombatInformationFn = void (*)(void* stats);
UpdateCombatInformationFn g_update_combat_information = nullptr;
std::vector<CombatInformationListener> g_listeners;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

void UpdateCombatInformation(void* stats) {
  g_update_combat_information(stats);
  if (!stats) return;
  uint8_t* info = Field<uint8_t*>(stats, ee::kStatsCombatInformation);
  if (!info) return;
  for (CombatInformationListener listener : g_listeners) listener(stats, info);
}

}  // namespace

bool AddCombatInformationListener(CombatInformationListener listener) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreatureStats::UpdateCombatInformation (character sheet)", ee::kUpdateCombatInformation,
                          reinterpret_cast<void*>(&UpdateCombatInformation),
                          reinterpret_cast<void**>(&g_update_combat_information));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_listeners.push_back(listener);
  return true;
}
