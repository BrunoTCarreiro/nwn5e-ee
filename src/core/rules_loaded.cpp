#include "rules_loaded.h"

#include <vector>

#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using RulesCtorFn = void* (*)(void* rules);
using ReloadAllFn = void (*)(void* rules);

RulesCtorFn g_rules_ctor = nullptr;
ReloadAllFn g_reload_all = nullptr;
std::vector<RulesLoadedHandler> g_loaded;  // filled at install, then only read
std::vector<RulesLoadingHandler> g_loading;
bool g_hooked = false;
bool g_hook_failed = false;

void* RulesCtor(void* rules) {
  for (RulesLoadingHandler handler : g_loading) handler();
  void* result = g_rules_ctor(rules);
  for (RulesLoadedHandler handler : g_loaded) handler(rules, false);
  return result;
}

void ReloadAll(void* rules) {
  g_reload_all(rules);
  for (RulesLoadedHandler handler : g_loaded) handler(rules, true);
}

bool Hook() {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWRules::CNWRules (tables loaded)", ee::kRulesConstructor,
                          reinterpret_cast<void*>(&RulesCtor), reinterpret_cast<void**>(&g_rules_ctor)) &&
               HookExport("CNWRules::ReloadAll (tables loaded again)", ee::kRulesReloadAll,
                          reinterpret_cast<void*>(&ReloadAll), reinterpret_cast<void**>(&g_reload_all));
    g_hook_failed = !g_hooked;
  }
  return g_hooked;
}

}  // namespace

bool OnRulesLoaded(RulesLoadedHandler handler) {
  if (!Hook()) return false;
  g_loaded.push_back(handler);
  void** rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  if (rules && *rules) handler(*rules, true);  // loaded before nwn5e started
  return true;
}

bool BeforeRulesLoad(RulesLoadingHandler handler) {
  if (!Hook()) return false;
  g_loading.push_back(handler);
  return true;
}
