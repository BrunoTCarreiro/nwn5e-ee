// nwn5e's own scripts run in C++ (script_hooks.h).

#include "script_hooks.h"

#include <string.h>

#include <string>
#include <vector>

#include "creature.h"
#include "hooks.h"
#include "log.h"
#include "rules/script_names.h"

namespace {

using RunScriptFn = int (*)(void* vm, void* name, uint32_t self, int self_valid, int event);
RunScriptFn g_run_script = nullptr;
constexpr const char* kRunScript = "?RunScript@CVirtualMachine@@QEAAHPEAVCExoString@@IHH@Z";

// Registered at install, read on the engine's thread: a name and its handler at the same index.
std::vector<std::string> g_names;
std::vector<ScriptHandler> g_runs;
bool g_hooked = false;
bool g_failed = false;

// A CExoString (char* +0, length +8) naming a registered script (rules::FindScriptName: case-blind). 0.69.2: any
// name - before, only "n5..." ones were looked at, so the game's scripts #124 replaces (x0_s3_acid ...) ran as NWN's.
ScriptHandler Find(void* name) {
  if (!name) return nullptr;
  const char* chars = Field<const char*>(name, 0);
  const uint32_t length = Field<uint32_t>(name, 8);
  const int at = rules::FindScriptName(g_names, chars, length);
  return at < 0 ? nullptr : g_runs[static_cast<size_t>(at)];
}

int RunScript(void* vm, void* name, uint32_t self, int self_valid, int event) {
  if (const ScriptHandler run = Find(name)) {
    run(self);
    return 1;
  }
  return g_run_script(vm, name, self, self_valid, event);
}

}  // namespace

bool AddScriptHandler(const char* name, ScriptHandler handler) {
  if (g_failed || !name || !handler) return false;
  if (!g_hooked) {
    if (!HookExport("CVirtualMachine::RunScript (nwn5e's own scripts: lights, feats)", kRunScript,
                    reinterpret_cast<void*>(&RunScript), reinterpret_cast<void**>(&g_run_script))) {
      g_failed = true;
      Log("SKIP nwn5e's own scripts: RunScript isn't hooked - %s and the rest do nothing", name);
      return false;
    }
    g_hooked = true;
  }
  g_names.push_back(name);
  g_runs.push_back(handler);
  // 0.69.2: the hook's own lookup must find it (a CExoString as the engine passes it) - else it would never run.
  alignas(8) uint8_t probe[16] = {};
  *reinterpret_cast<const char**>(probe) = name;
  *reinterpret_cast<uint32_t*>(probe + 8) = static_cast<uint32_t>(strlen(name));
  if (Find(probe) != handler) {
    Log("SKIP nwn5e's script %s: the RunScript hook wouldn't run it (%s)", name,
        Find(probe) ? "the name is registered twice" : "its lookup misses it");
    g_names.pop_back();
    g_runs.pop_back();
    return false;
  }
  return true;
}
