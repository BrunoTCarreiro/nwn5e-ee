#include "hooks.h"

#include <windows.h>
#include <string.h>

#include <algorithm>
#include <mutex>
#include <string>
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

// The begin of the primary entry a .pdata entry belongs to (its chain's root).
uint32_t RootBegin(uintptr_t base, const RUNTIME_FUNCTION* entry) {
  for (int depth = 0; depth < 32 && entry; ++depth) {
    if (entry->UnwindData & 1) {  // an indirect entry: points at another RUNTIME_FUNCTION
      entry = reinterpret_cast<const RUNTIME_FUNCTION*>(base + (entry->UnwindData & ~1u));
      continue;
    }
    const auto* unwind = reinterpret_cast<const uint8_t*>(base + entry->UnwindData);
    if (!((unwind[0] >> 3) & UNW_FLAG_CHAININFO)) return entry->BeginAddress;
    const int codes = unwind[2];
    entry = reinterpret_cast<const RUNTIME_FUNCTION*>(unwind + 4 + ((codes + 1) & ~1) * 2);
  }
  return entry ? entry->BeginAddress : 0;
}

CodeRange FunctionAt(uintptr_t address) {
  DWORD64 image_base = 0;
  const RUNTIME_FUNCTION* entry = RtlLookupFunctionEntry(address, &image_base, nullptr);
  if (!entry) return {};
  const uintptr_t base = static_cast<uintptr_t>(image_base);
  const uint32_t root = RootBegin(base, entry);
  // Every entry of the module's exception directory with the same root.
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
  const auto* entries = reinterpret_cast<const RUNTIME_FUNCTION*>(base + directory.VirtualAddress);
  const size_t count = directory.Size / sizeof(RUNTIME_FUNCTION);
  CodeRange range;
  for (size_t i = 0; i < count; ++i) {
    if (RootBegin(base, &entries[i]) != root) continue;
    const uintptr_t begin = base + entries[i].BeginAddress;
    const uintptr_t end = base + entries[i].EndAddress;
    range.parts.push_back({begin, end});
    if (!range.begin || begin < range.begin) range.begin = begin;
    if (end > range.end) range.end = end;
  }
  if (range.parts.empty()) {
    range.begin = base + entry->BeginAddress;
    range.end = base + entry->EndAddress;
  }
  return range;
}

std::vector<uintptr_t> FindCallsTo(CodeRange range, uintptr_t target) {
  std::vector<uintptr_t> sites;
  if (!range.Valid()) return sites;
  std::vector<std::pair<uintptr_t, uintptr_t>> parts = range.parts;
  if (parts.empty()) parts.push_back({range.begin, range.end});
  std::sort(parts.begin(), parts.end());
  for (const auto& part : parts) {
    for (uintptr_t site = part.first; site + kCallLength <= part.second; ++site) {
      const auto* code = reinterpret_cast<const uint8_t*>(site);
      if (code[0] != 0xe8) continue;
      int32_t rel = 0;
      memcpy(&rel, code + 1, sizeof(rel));
      if (site + kCallLength + static_cast<intptr_t>(rel) == target) sites.push_back(site);
    }
  }
  return sites;
}

bool ContainsBytes(uintptr_t from, uintptr_t to, const uint8_t* pattern, size_t size) {
  for (uintptr_t at = from; at + size <= to; ++at) {
    if (memcmp(reinterpret_cast<const void*>(at), pattern, size) == 0) return true;
  }
  return false;
}

bool PatchAnchored(const char* what, const char* export_name, const uint8_t* anchor, size_t anchor_size,
                   size_t offset, const uint8_t* expected, const uint8_t* replacement, size_t size) {
  const CodeRange code = FunctionAt(EngineExport(export_name));
  if (!code.Valid() || offset + size > anchor_size) {
    Log("SKIP %s: %s not found", what, export_name);
    return false;
  }
  uintptr_t found = 0;
  int matches = 0;
  for (uintptr_t at = code.begin; at + anchor_size <= code.end; ++at) {
    if (memcmp(reinterpret_cast<const void*>(at), anchor, anchor_size) == 0) {
      found = at;
      ++matches;
    }
  }
  if (matches != 1) {
    Log("SKIP %s: the anchor occurs %d times in %s (expected once)", what, matches, export_name);
    return false;
  }
  const uintptr_t site = found + offset;
  if (memcmp(reinterpret_cast<const void*>(site), expected, size) != 0) {
    Log("SKIP %s: unexpected bytes at %s+0x%zx", what, export_name, site - code.begin);
    return false;
  }
  DWORD old_protect = 0;
  if (!VirtualProtect(reinterpret_cast<void*>(site), size, PAGE_EXECUTE_READWRITE, &old_protect)) {
    Log("SKIP %s: VirtualProtect failed (%lu)", what, GetLastError());
    return false;
  }
  memcpy(reinterpret_cast<void*>(site), replacement, size);
  DWORD ignored = 0;
  VirtualProtect(reinterpret_cast<void*>(site), size, old_protect, &ignored);
  FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(site), size);
  Log("patch %s at %s+0x%zx (%zu bytes)", what, export_name, site - code.begin, size);
  return true;
}

bool CallerContext(uintptr_t return_address, CONTEXT* out) {
  CONTEXT context;
  RtlCaptureContext(&context);
  for (int frame = 0; frame < 32; ++frame) {
    if (context.Rip == return_address) {
      *out = context;
      return true;
    }
    DWORD64 image_base = 0;
    PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
    if (!function) {  // a leaf function: its return address is on top of the stack
      context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
      context.Rsp += 8;
      continue;
    }
    void* handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context, &handler_data, &establisher,
                     nullptr);
    if (!context.Rip) return false;
  }
  return false;
}

namespace {

struct ParsedPattern {
  std::vector<uint8_t> bytes;
  std::vector<bool> wild;
};

ParsedPattern ParsePattern(const char* text) {
  ParsedPattern pattern;
  for (const char* p = text; *p;) {
    while (*p == ' ') ++p;
    if (!*p) break;
    if (p[0] == '?') {
      pattern.bytes.push_back(0);
      pattern.wild.push_back(true);
      p += p[1] == '?' ? 2 : 1;
      continue;
    }
    pattern.bytes.push_back(static_cast<uint8_t>(strtoul(std::string(p, 2).c_str(), nullptr, 16)));
    pattern.wild.push_back(false);
    p += 2;
  }
  return pattern;
}

// A section of the executable by name (".text", ".rdata", ".data").
CodeRange Section(const char* name) {
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
  const size_t length = strlen(name);
  for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
    if (length <= 8 && memcmp(section->Name, name, length) == 0 && (length == 8 || section->Name[length] == 0)) {
      return {base + section->VirtualAddress, base + section->VirtualAddress + section->Misc.VirtualSize, {}};
    }
  }
  return {};
}

CodeRange TextSection() { return Section(".text"); }

// Every place the bytes occur in [range).
std::vector<uintptr_t> FindAll(const CodeRange& range, const void* bytes, size_t size) {
  std::vector<uintptr_t> found;
  if (!range.Valid() || !size) return found;
  const auto* first = static_cast<const uint8_t*>(bytes);
  for (uintptr_t at = range.begin; at + size <= range.end; ++at) {
    if (*reinterpret_cast<const uint8_t*>(at) == first[0] && memcmp(reinterpret_cast<const void*>(at), bytes, size) == 0) {
      found.push_back(at);
    }
  }
  return found;
}

// The one place `pattern` occurs in `range` (0 when none or more than one).
uintptr_t FindUnique(const CodeRange& range, const ParsedPattern& pattern, int* matches) {
  *matches = 0;
  uintptr_t found = 0;
  const size_t n = pattern.bytes.size();
  if (!n || !range.Valid()) return 0;
  for (uintptr_t at = range.begin; at + n <= range.end; ++at) {
    const auto* code = reinterpret_cast<const uint8_t*>(at);
    if (code[0] != pattern.bytes[0] && !pattern.wild[0]) continue;
    size_t i = 0;
    while (i < n && (pattern.wild[i] || code[i] == pattern.bytes[i])) ++i;
    if (i == n) {
      found = at;
      if (++*matches > 1) return 0;
    }
  }
  return *matches == 1 ? found : 0;
}

}  // namespace

bool ApplyPatternPatches(const char* what, const PatternPatch* patches, size_t count) {
  std::vector<uintptr_t> sites(count, 0);
  for (size_t i = 0; i < count; ++i) {
    const PatternPatch& patch = patches[i];
    const CodeRange range = patch.within_export ? FunctionAt(EngineExport(patch.within_export)) : TextSection();
    int matches = 0;
    const uintptr_t at = FindUnique(range, ParsePattern(patch.pattern), &matches);
    if (!at || memcmp(reinterpret_cast<const void*>(at + patch.offset), patch.expected, patch.size) != 0) {
      Log("SKIP %s: %s found %d times%s - none of its %zu patches applied", what, patch.what, matches,
          at ? " (other bytes than expected)" : "", count);
      return false;
    }
    sites[i] = at + patch.offset;
  }
  for (size_t i = 0; i < count; ++i) {
    DWORD old_protect = 0;
    VirtualProtect(reinterpret_cast<void*>(sites[i]), patches[i].size, PAGE_EXECUTE_READWRITE, &old_protect);
    memcpy(reinterpret_cast<void*>(sites[i]), patches[i].replacement, patches[i].size);
    DWORD ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(sites[i]), patches[i].size, old_protect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(sites[i]), patches[i].size);
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    Log("patch %s: %s at rva 0x%zx", what, patches[i].what, sites[i] - base);
  }
  return true;
}

// --- Anchors for code nothing exports --------------------------------------

namespace {

uintptr_t ImageBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)); }

}  // namespace

uintptr_t FunctionEntry(uintptr_t address) {
  DWORD64 image_base = 0;
  const RUNTIME_FUNCTION* entry = RtlLookupFunctionEntry(address, &image_base, nullptr);
  return entry ? static_cast<uintptr_t>(image_base) + RootBegin(static_cast<uintptr_t>(image_base), entry) : 0;
}

namespace {

// Every `lea r64, [rip + disp32]` in .text (REX.W 48 / 4c, 8d, ModRM mod 00
// r/m 101) whose address is one of `targets`.
std::vector<uintptr_t> LeaReferences(const std::vector<uintptr_t>& targets) {
  std::vector<uintptr_t> references;
  if (targets.empty()) return references;
  const CodeRange text_section = TextSection();
  for (uintptr_t at = text_section.begin; at + 7 <= text_section.end; ++at) {
    const auto* code = reinterpret_cast<const uint8_t*>(at);
    if ((code[0] != 0x48 && code[0] != 0x4c) || code[1] != 0x8d || (code[2] & 0xc7) != 0x05) continue;
    int32_t displacement = 0;
    memcpy(&displacement, code + 3, sizeof(displacement));
    const uintptr_t target = at + 7 + displacement;
    if (std::find(targets.begin(), targets.end(), target) != targets.end()) references.push_back(at);
  }
  return references;
}

}  // namespace

std::vector<uintptr_t> FindStringReferences(const char* text) {
  const size_t size = strlen(text) + 1;  // with its terminating zero
  std::vector<uintptr_t> strings;
  for (const char* name : {".rdata", ".data"}) {
    const std::vector<uintptr_t> found = FindAll(Section(name), text, size);
    strings.insert(strings.end(), found.begin(), found.end());
  }
  return LeaReferences(strings);
}

std::vector<uintptr_t> FindRipReferences(uintptr_t target) {
  return target ? LeaReferences({target}) : std::vector<uintptr_t>();
}

uintptr_t HandlerByTag(const char* what, uintptr_t function, const char* tag) {
  const CodeRange range = FunctionAt(function);
  std::vector<uintptr_t> tags;
  for (const char* name : {".rdata", ".data"}) {
    const std::vector<uintptr_t> found = FindAll(Section(name), tag, strlen(tag) + 1);
    tags.insert(tags.end(), found.begin(), found.end());
  }
  const CodeRange text_section = TextSection();
  std::vector<std::pair<uintptr_t, uintptr_t>> parts = range.parts;
  if (parts.empty() && range.Valid()) parts.push_back({range.begin, range.end});
  std::sort(parts.begin(), parts.end());
  bool armed = false;
  for (const auto& part : parts) {
    for (uintptr_t at = part.first; at + 7 <= part.second; ++at) {
      const auto* code = reinterpret_cast<const uint8_t*>(at);
      if ((code[0] != 0x48 && code[0] != 0x4c) || code[1] != 0x8d || (code[2] & 0xc7) != 0x05) continue;
      int32_t displacement = 0;
      memcpy(&displacement, code + 3, sizeof(displacement));
      const uintptr_t target = at + 7 + displacement;
      if (std::find(tags.begin(), tags.end(), target) != tags.end()) {
        armed = true;
      } else if (armed && text_section.Contains(target)) {
        if (FunctionEntry(target) == target) return target;
        armed = false;
      }
    }
  }
  Log("SKIP %s: no handler stored after the tag \"%s\"", what, tag);
  return 0;
}

uintptr_t FindPatternIn(const char* what, const CodeRange& range, const char* pattern) {
  int matches = 0;
  uintptr_t found = 0;
  std::vector<std::pair<uintptr_t, uintptr_t>> parts = range.parts;
  if (parts.empty() && range.Valid()) parts.push_back({range.begin, range.end});
  const ParsedPattern parsed = ParsePattern(pattern);
  for (const auto& part : parts) {
    int here = 0;
    CodeRange piece;
    piece.begin = part.first;
    piece.end = part.second;
    const uintptr_t at = FindUnique(piece, parsed, &here);
    matches += here;
    if (at) found = at;
  }
  if (matches != 1) {
    Log("SKIP %s: its byte pattern matches %d times in the function (expected 1)", what, matches);
    return 0;
  }
  return found;
}

uintptr_t CallTarget(uintptr_t call) {
  const auto* code = reinterpret_cast<const uint8_t*>(call);
  if (!call || code[0] != 0xe8) return 0;
  int32_t displacement = 0;
  memcpy(&displacement, code + 1, sizeof(displacement));
  return call + kCallLength + displacement;
}

uintptr_t FirstCallIn(uintptr_t function) {
  const CodeRange range = FunctionAt(function);
  const CodeRange text_section = TextSection();
  for (uintptr_t at = function; range.Contains(at) && at + kCallLength <= range.end; ++at) {
    const uintptr_t target = CallTarget(at);
    if (target && text_section.Contains(target) && FunctionEntry(target) == target) return target;
  }
  return 0;
}

uintptr_t FindPattern(const char* what, const char* pattern) {
  int matches = 0;
  const uintptr_t at = FindUnique(TextSection(), ParsePattern(pattern), &matches);
  if (!at) Log("SKIP %s: its byte pattern matches %d times (expected 1)", what, matches);
  return at;
}

uintptr_t FunctionByString(const char* what, const char* text) {
  std::vector<uintptr_t> entries;
  for (uintptr_t reference : FindStringReferences(text)) {
    const uintptr_t entry = FunctionEntry(reference);
    if (entry && std::find(entries.begin(), entries.end(), entry) == entries.end()) entries.push_back(entry);
  }
  if (entries.size() != 1) {
    Log("SKIP %s: \"%s\" is referenced by %zu functions (expected 1)", what, text, entries.size());
    return 0;
  }
  return entries[0];
}

uintptr_t FindVtable(const char* what, const char* rtti_name) {
  const uintptr_t base = ImageBase();
  // The TypeDescriptor: {vftable pointer, spare, name}, the name at +0x10.
  std::vector<uintptr_t> descriptors;
  for (const char* name : {".data", ".rdata"}) {
    for (uintptr_t at : FindAll(Section(name), rtti_name, strlen(rtti_name) + 1)) descriptors.push_back(at - 0x10);
  }
  if (descriptors.size() != 1) {
    Log("SKIP %s: type %s found %zu times", what, rtti_name, descriptors.size());
    return 0;
  }
  // The CompleteObjectLocator (x64): signature 1, offset 0, cdOffset, then the
  // type descriptor, class descriptor and itself as image-relative offsets.
  const auto descriptor_rva = static_cast<uint32_t>(descriptors[0] - base);
  const CodeRange rdata = Section(".rdata");
  uintptr_t locator = 0;
  int locators = 0;
  for (uintptr_t at = rdata.begin; at + 24 <= rdata.end; at += 4) {
    const auto* col = reinterpret_cast<const uint32_t*>(at);
    if (col[0] == 1 && col[1] == 0 && col[3] == descriptor_rva && col[5] == static_cast<uint32_t>(at - base)) {
      locator = at;
      ++locators;
    }
  }
  if (locators != 1) {
    Log("SKIP %s: %d complete object locators for %s (expected 1)", what, locators, rtti_name);
    return 0;
  }
  // The vtable follows a pointer to its locator.
  const uint64_t pointer = locator;
  const std::vector<uintptr_t> slots = FindAll(rdata, &pointer, sizeof(pointer));
  if (slots.size() != 1) {
    Log("SKIP %s: %zu vtables point at %s's locator (expected 1)", what, slots.size(), rtti_name);
    return 0;
  }
  return slots[0] + sizeof(pointer);
}

uintptr_t VtableSlot(uintptr_t vtable, int slot) {
  return vtable ? reinterpret_cast<const uintptr_t*>(vtable)[slot] : 0;
}

bool HookAddress(const char* what, uintptr_t address, void* detour, void** original) {
  if (!address) {
    Log("SKIP %s: its code was not found", what);
    return false;
  }
  void* target = reinterpret_cast<void*>(address);
  MH_STATUS status = MH_CreateHook(target, detour, original);
  if (status == MH_OK) status = MH_QueueEnableHook(target);
  if (status != MH_OK) {
    Log("FAIL %s at %p: %s", what, target, MH_StatusToString(status));
    return false;
  }
  Log("hook %s at rva 0x%zx", what, address - ImageBase());
  return true;
}
