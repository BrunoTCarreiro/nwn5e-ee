#pragma once

#include <stddef.h>
#include <stdint.h>

// EE's 64-bit nwmain.exe exports its engine functions by their decorated C++
// names (docs/EE-RECON.md), so nwn5e finds them by name — not by fixed
// addresses — and a Beamdog patch that keeps a name keeps the hook.

// The address of an export of nwmain.exe (e.g.
// "?GetCriticalHitRoll@CNWSCreatureStats@@QEAAHH@Z"), or 0 if it has none.
uintptr_t EngineExport(const char* decorated_name);

// The decorated name of the export at or before `address` in nwmain.exe —
// the function that most likely contains it — with *offset = address minus
// that export; nullptr outside nwmain.exe's exported code. For naming callers
// (_ReturnAddress()) in logs.
const char* ExportContaining(uintptr_t address, uintptr_t* offset);

bool HooksBegin();
// Hooks the export `decorated_name`; `original` receives the trampoline.
// Logs and returns false when the export is missing (a different build).
bool HookExport(const char* what, const char* decorated_name, void* detour, void** original);
bool HooksCommit();
