#include "rest.h"

#include <vector>

#include "ee_layout.h"
#include "hooks.h"

namespace {

using ResetFn = void (*)(void* stats);
ResetFn g_reset = nullptr;
std::vector<RestListener> g_listeners;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

void ResetFeatRemainingUses(void* stats) {
  g_reset(stats);
  for (RestListener listener : g_listeners) listener(stats);
}

}  // namespace

namespace {
std::vector<ShortRestListener> g_short_rest_listeners;  // filled at install, then only read
}  // namespace

void AddShortRestListener(ShortRestListener listener) { g_short_rest_listeners.push_back(listener); }

std::string AfterShortRest(void* creature) {
  std::string words;
  for (ShortRestListener listener : g_short_rest_listeners) words += listener(creature);
  return words;
}

bool AddRestListener(RestListener listener) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreatureStats::ResetFeatRemainingUses (after a rest)", ee::kResetFeatRemainingUses,
                          reinterpret_cast<void*>(&ResetFeatRemainingUses), reinterpret_cast<void**>(&g_reset));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_listeners.push_back(listener);
  return true;
}
