#include "hooks.h"

#include <windows.h>

#include "MinHook.h"
#include "log.h"

uintptr_t EngineExport(const char* decorated_name) {
  return reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(nullptr), decorated_name));
}

bool HooksBegin() {
  const MH_STATUS status = MH_Initialize();
  if (status != MH_OK) Log("MinHook initialise failed: %s", MH_StatusToString(status));
  return status == MH_OK;
}

bool HookExport(const char* what, const char* decorated_name, void* detour, void** original) {
  void* target = reinterpret_cast<void*>(EngineExport(decorated_name));
  if (!target) {
    Log("SKIP %s: nwmain.exe has no export %s", what, decorated_name);
    return false;
  }
  MH_STATUS status = MH_CreateHook(target, detour, original);
  if (status == MH_OK) status = MH_QueueEnableHook(target);
  if (status != MH_OK) {
    Log("FAIL %s at %p: %s", what, target, MH_StatusToString(status));
    return false;
  }
  Log("hook %s at %p (%s)", what, target, decorated_name);
  return true;
}

bool HooksCommit() {
  const MH_STATUS status = MH_ApplyQueued();
  if (status != MH_OK) Log("MinHook apply failed: %s", MH_StatusToString(status));
  return status == MH_OK;
}
