#include "feat_use.h"

#include <stdint.h>

#include <vector>

#include "ee_layout.h"
#include "hooks.h"

namespace {

using UseFeatFn = int (*)(void* creature, uint16_t feat, uint16_t subfeat, uint32_t target, uint32_t area,
                          void* location);

UseFeatFn g_use_feat = nullptr;
std::vector<FeatUseGuard> g_guards;        // filled at install, then only read
std::vector<FeatUseListener> g_listeners;  // the same
std::vector<FeatUseHandler> g_handlers;    // the same
bool g_hooked = false;
bool g_hook_failed = false;

int UseFeat(void* creature, uint16_t feat, uint16_t subfeat, uint32_t target, uint32_t area, void* location) {
  if (creature) {
    for (FeatUseGuard guard : g_guards) {
      if (!guard(creature, feat)) return 0;
    }
    for (FeatUseHandler handler : g_handlers) {
      const int handled = handler(creature, feat, subfeat, target);
      if (handled >= 0) {
        if (handled) {
          for (FeatUseListener listener : g_listeners) listener(creature, feat);
        }
        return handled;
      }
    }
  }
  const int result = g_use_feat(creature, feat, subfeat, target, area, location);
  if (result && creature) {
    for (FeatUseListener listener : g_listeners) listener(creature, feat);
  }
  return result;
}

bool Hook() {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreature::UseFeat (feat-use guards and listeners)", ee::kUseFeat,
                          reinterpret_cast<void*>(&UseFeat), reinterpret_cast<void**>(&g_use_feat));
    g_hook_failed = !g_hooked;
  }
  return g_hooked;
}

}  // namespace

bool AddFeatUseGuard(FeatUseGuard guard) {
  if (!Hook()) return false;
  g_guards.push_back(guard);
  return true;
}

bool AddFeatUseListener(FeatUseListener listener) {
  if (!Hook()) return false;
  g_listeners.push_back(listener);
  return true;
}

bool AddFeatUseHandler(FeatUseHandler handler) {
  if (!Hook()) return false;
  g_handlers.push_back(handler);
  return true;
}
