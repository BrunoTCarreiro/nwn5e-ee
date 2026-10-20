// nwn5e.dll for NWN: Enhanced Edition (64-bit) — loaded into nwmain.exe by
// nwn5e.exe before the game's entry point. Engine functions are found by
// their exported names (docs/EE-RECON.md).
//
// Spike (route B): CNWSCreatureStats::GetCriticalHitRoll is hooked so every
// attack roll threatens a critical hit — the same proof nwn5e's 1.69 build
// started with. It has three callers in 89.8193.37-17: ResolveAttackRoll (the
// attack itself; each call logged, the first 200) and UpdateCombatInformation
// twice (main hand and off hand for the character sheet, on load and on
// changes; counted, logged once). The sheet shows the changed range too.

#include <windows.h>
#include <intrin.h>
#include <shlwapi.h>
#include <string.h>

#include "hooks.h"
#include "log.h"

#pragma comment(lib, "shlwapi.lib")

#ifdef NWN5E_PRIVATE
void InstallPrivateRules();  // private/src (docs/PRIVATE-LAYER.md)
#endif

namespace {

constexpr const char* kGetCriticalHitRoll = "?GetCriticalHitRoll@CNWSCreatureStats@@QEAAHH@Z";

// x64: one calling convention — `this` is the first argument.
using GetCriticalHitRollFn = int (*)(void* stats, int offhand);
GetCriticalHitRollFn g_get_critical_hit_roll = nullptr;
int g_attack_calls = 0;
int g_other_calls = 0;

int GetCriticalHitRoll(void* stats, int offhand) {
  const int vanilla = g_get_critical_hit_roll(stats, offhand);
  uintptr_t offset = 0;
  const char* caller = ExportContaining(reinterpret_cast<uintptr_t>(_ReturnAddress()), &offset);
  if (caller && strstr(caller, "ResolveAttackRoll@CNWSCreature@@")) {
    if (++g_attack_calls <= 200) {
      Log("spike: attack roll #%d (stats %p, %s hand): threat from %d+ in EE -> 1 (every roll threatens)", g_attack_calls,
          stats, offhand ? "off" : "main", vanilla);
    }
  } else if (++g_other_calls == 1) {
    Log("spike: GetCriticalHitRoll also called from %s +0x%llx (sheet / combat info; not logged further)",
        caller ? caller : "outside nwmain's exports", static_cast<unsigned long long>(offset));
  }
  return 1;
}

void Start(HMODULE self) {
  wchar_t directory[MAX_PATH];
  GetModuleFileNameW(self, directory, MAX_PATH);
  PathRemoveFileSpecW(directory);
  LogOpen(directory);
  Log("nwn5e-ee 0.0.2 loaded (spike: every attack roll threatens a critical)");
  wchar_t host[MAX_PATH];
  GetModuleFileNameW(nullptr, host, MAX_PATH);
  Log("host: %ls", host);
  if (!HooksBegin()) return;
  HookExport("CNWSCreatureStats::GetCriticalHitRoll (spike)", kGetCriticalHitRoll,
             reinterpret_cast<void*>(&GetCriticalHitRoll), reinterpret_cast<void**>(&g_get_critical_hit_roll));
#ifdef NWN5E_PRIVATE
  Log("private layer: compiled in");
  InstallPrivateRules();
#endif
  Log(HooksCommit() ? "hooks active" : "hooks NOT active");
}

}  // namespace

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(module);
    Start(module);
  }
  return TRUE;
}
