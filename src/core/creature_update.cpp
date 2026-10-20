#include "creature_update.h"

#include <vector>

#include "ee_layout.h"
#include "hooks.h"

namespace {

using AIUpdateFn = void (*)(void* creature);
AIUpdateFn g_ai_update = nullptr;
std::vector<CreatureUpdateListener> g_listeners;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

void AIUpdate(void* creature) {
  g_ai_update(creature);
  if (!creature) return;
  for (CreatureUpdateListener listener : g_listeners) listener(creature);
}

}  // namespace

bool AddCreatureUpdateListener(CreatureUpdateListener listener) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreature::AIUpdate (per-creature updates)", ee::kAIUpdate,
                          reinterpret_cast<void*>(&AIUpdate), reinterpret_cast<void**>(&g_ai_update));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_listeners.push_back(listener);
  return true;
}
