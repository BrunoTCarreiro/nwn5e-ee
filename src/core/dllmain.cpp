// nwn5e.dll for NWN: Enhanced Edition (64-bit) — loaded into nwmain.exe by
// nwn5e.exe before the game's entry point. Engine functions are found by
// their exported names (docs/EE-RECON.md).
//
// Spike (route B): CNWSCreatureStats::GetCriticalHitRoll is hooked so every
// attack roll threatens a critical hit — the same proof nwn5e's 1.69 build
// started with. Each call is logged (the first 200).

#include <windows.h>
#include <shlwapi.h>

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
int g_calls = 0;

int GetCriticalHitRoll(void* stats, int offhand) {
  const int vanilla = g_get_critical_hit_roll(stats, offhand);
  if (g_calls < 200) {
    ++g_calls;
    Log("spike: GetCriticalHitRoll(stats %p, offhand %d): EE %d -> 1 (every roll threatens)", stats, offhand, vanilla);
  }
  return 1;
}

void Start(HMODULE self) {
  wchar_t directory[MAX_PATH];
  GetModuleFileNameW(self, directory, MAX_PATH);
  PathRemoveFileSpecW(directory);
  LogOpen(directory);
  Log("nwn5e-ee 0.0.1 loaded (spike: every attack roll threatens a critical)");
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
