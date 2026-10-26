// nwn5e's own scripts run in C++ (script_hooks.h).

#include "script_hooks.h"

#include <string.h>

#include <string>
#include <vector>

#include "creature.h"
#include "hooks.h"
#include "log.h"

namespace {

using RunScriptFn = int (*)(void* vm, void* name, uint32_t self, int self_valid, int event);
RunScriptFn g_run_script = nullptr;
constexpr const char* kRunScript = "?RunScript@CVirtualMachine@@QEAAHPEAVCExoString@@IHH@Z";

struct Handler {
  std::string name;
  ScriptHandler run;
};
std::vector<Handler> g_handlers;  // registered at install, read on the engine's thread
bool g_hooked = false;
bool g_failed = false;

// A CExoString (char* +0, length +8) naming a registered script, resrefs being case-blind.
const Handler* Find(void* name) {
  if (!name) return nullptr;
  const char* chars = Field<const char*>(name, 0);
  const uint32_t length = Field<uint32_t>(name, 8);
  if (!chars || length < 3 || (chars[0] != 'n' && chars[0] != 'N') || chars[1] != '5') return nullptr;
  for (const Handler& h : g_handlers) {
    if (h.name.size() == length && _strnicmp(chars, h.name.c_str(), length) == 0) return &h;
  }
  return nullptr;
}

int RunScript(void* vm, void* name, uint32_t self, int self_valid, int event) {
  if (const Handler* h = Find(name)) {
    h->run(self);
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
  g_handlers.push_back(Handler{name, handler});
  return true;
}
