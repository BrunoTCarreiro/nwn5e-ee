#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

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

// --- Reading engine code (for anchored checks, docs/EE-PORT.md principle 1).

constexpr uintptr_t kCallLength = 5;  // E8 rel32

// A function's code, from the executable's unwind table (.pdata).
struct CodeRange {
  uintptr_t begin = 0;
  uintptr_t end = 0;
  bool Valid() const { return end > begin; }
  bool Contains(uintptr_t address) const { return address >= begin && address < end; }
};

// The function whose code contains `address` (empty if none is listed).
CodeRange FunctionAt(uintptr_t address);

// Every direct call (E8 rel32) inside `range` that lands on `target`, in
// address order. Call sites are not changed by hooking their target.
std::vector<uintptr_t> FindCallsTo(CodeRange range, uintptr_t target);

// Whether the bytes `pattern` occur anywhere in [from, to).
bool ContainsBytes(uintptr_t from, uintptr_t to, const uint8_t* pattern, size_t size);
