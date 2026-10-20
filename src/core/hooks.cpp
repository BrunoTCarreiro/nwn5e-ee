#include "hooks.h"

#include <windows.h>

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

#include "MinHook.h"
#include "log.h"

uintptr_t EngineExport(const char* decorated_name) {
  return reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(nullptr), decorated_name));
}

const char* ExportContaining(uintptr_t address, uintptr_t* offset) {
  // nwmain.exe's exports sorted by address, read once from its export table.
  static std::vector<std::pair<uintptr_t, const char*>> exports;
  static std::once_flag once;
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  std::call_once(once, [base] {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return;
    const auto* table = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
    const auto* functions = reinterpret_cast<const DWORD*>(base + table->AddressOfFunctions);
    const auto* names = reinterpret_cast<const DWORD*>(base + table->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const WORD*>(base + table->AddressOfNameOrdinals);
    exports.reserve(table->NumberOfNames);
    for (DWORD i = 0; i < table->NumberOfNames; ++i) {
      exports.emplace_back(base + functions[ordinals[i]], reinterpret_cast<const char*>(base + names[i]));
    }
    std::sort(exports.begin(), exports.end());
  });
  auto next = std::upper_bound(exports.begin(), exports.end(), std::make_pair(address, static_cast<const char*>(nullptr)),
                               [](const auto& a, const auto& b) { return a.first < b.first; });
  if (next == exports.begin() || next == exports.end()) return nullptr;
  --next;
  *offset = address - next->first;
  return next->second;
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
