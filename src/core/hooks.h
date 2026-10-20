#pragma once

#include <stddef.h>
#include <stdint.h>

// EE's 64-bit nwmain.exe exports its engine functions by their decorated C++
// names (docs/EE-RECON.md), so nwn5e finds them by name — not by fixed
// addresses — and a Beamdog patch that keeps a name keeps the hook.

// The address of an export of nwmain.exe (e.g.
// "?GetCriticalHitRoll@CNWSCreatureStats@@QEAAHH@Z"), or 0 if it has none.
uintptr_t EngineExport(const char* decorated_name);

bool HooksBegin();
// Hooks the export `decorated_name`; `original` receives the trampoline.
// Logs and returns false when the export is missing (a different build).
bool HookExport(const char* what, const char* decorated_name, void* detour, void** original);
bool HooksCommit();
