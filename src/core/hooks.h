#pragma once

#include <stddef.h>
#include <stdint.h>

#include <utility>
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
// The compiler splits many EE functions into chained fragments (shrink-
// wrapped prologues, cold paths: 13,776 of 50,532 .pdata entries); a range
// holds every fragment of one function. begin / end span them all.
struct CodeRange {
  uintptr_t begin = 0;
  uintptr_t end = 0;
  std::vector<std::pair<uintptr_t, uintptr_t>> parts;  // [begin, end) of each fragment
  bool Valid() const { return end > begin; }
  bool Contains(uintptr_t address) const {
    if (parts.empty()) return address >= begin && address < end;
    for (const auto& part : parts) {
      if (address >= part.first && address < part.second) return true;
    }
    return false;
  }
};

// The function whose code contains `address`, every fragment of it (empty if
// none is listed).
CodeRange FunctionAt(uintptr_t address);

// Every direct call (E8 rel32) inside `range` that lands on `target`, in
// address order. Call sites are not changed by hooking their target.
std::vector<uintptr_t> FindCallsTo(CodeRange range, uintptr_t target);

// Whether the bytes `pattern` occur anywhere in [from, to).
bool ContainsBytes(uintptr_t from, uintptr_t to, const uint8_t* pattern, size_t size);

// An anchored patch (docs/EE-PORT.md principle 1, EP2: the last resort): the
// bytes `anchor` must occur exactly once inside the exported function
// `export_name`; at `offset` into the anchor, `size` bytes equal to `expected`
// are replaced by `replacement`. Skipped and logged otherwise (a different
// game build), never applied blind.
bool PatchAnchored(const char* what, const char* export_name, const uint8_t* anchor, size_t anchor_size,
                   size_t offset, const uint8_t* expected, const uint8_t* replacement, size_t size);

// The registers of the engine code that called into a hook, at that call: the
// stack is unwound from here up to the frame returning to `return_address`
// (the call site's return address, as _ReturnAddress() gave it to the hook),
// so its nonvolatile registers (rbx, rbp, rdi, rsi, r12-r15) hold what the
// engine had in them. False when that frame is not found.
struct _CONTEXT;
bool CallerContext(uintptr_t return_address, _CONTEXT* out);

// Patches found by byte pattern ("ff c0 ?? 75", ?? = any byte), each exactly
// once inside the exported function `within_export` — or, for code nothing
// exports (the client's), in the whole executable's .text (EP2, extended by
// EP15 for one all-or-nothing case). At `offset` into the match, `size`
// bytes equal to `expected` become `replacement`. All of `patches` are
// applied or none (logged).
struct PatternPatch {
  const char* what;
  const char* within_export;  // nullptr: the whole .text
  const char* pattern;
  size_t offset;
  const uint8_t* expected;
  const uint8_t* replacement;
  size_t size;
};
bool ApplyPatternPatches(const char* what, const PatternPatch* patches, size_t count);

// --- Code nothing exports (the client's GUI, docs/CHARGEN.md §10) ----------
// Found by anchors that survive a rebuild of the same code, checked for
// uniqueness, and skipped (logged) when they aren't: never hooked blind.

// The entry of the function containing `address` (its .pdata chain's root), 0 if none.
uintptr_t FunctionEntry(uintptr_t address);

// Every `lea r64, [rip + x]` in .text whose x is the string `text` (with its
// terminating zero) in .rdata / .data.
std::vector<uintptr_t> FindStringReferences(const char* text);

// The one function that references `text` (e.g. its own debug message); 0 and
// logged when none or several do.
uintptr_t FunctionByString(const char* what, const char* text);

// A class's vtable from its MSVC RTTI name (".?AVJsonWindow@JSON@Nui@@"):
// TypeDescriptor -> CompleteObjectLocator -> the vtable after a pointer to it.
// 0 and logged unless each step is unique.
uintptr_t FindVtable(const char* what, const char* rtti_name);
uintptr_t VtableSlot(uintptr_t vtable, int slot);

// HookExport for an address found by an anchor; skipped (logged) when 0.
bool HookAddress(const char* what, uintptr_t address, void* detour, void** original);
