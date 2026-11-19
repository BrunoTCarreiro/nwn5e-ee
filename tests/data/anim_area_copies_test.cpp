#include "check.h"
#include "data/anim_area_copies.h"
#include "data/anim_area_engine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace data;

// #161 VA-5: group 1's copy 0, left at start 0 by the game's loader (never placed: it stays at the centre), gets a start
// of 1 ms so the update places it like the others - when group 1 has several copies.
TEST(AreaFirstCopyScattered) {
  CHECK_EQ(FirstCopyStart(5, 0, 0), kFirstCopyStartMs);  // Entangle's 5 copies: copy 0 placed too
  CHECK_EQ(FirstCopyStart(2, 0, 0), kFirstCopyStartMs);
  CHECK_EQ(kFirstCopyStartMs, 1);
}

// A lone copy keeps the centre (a flask's splash), and a copy the loader did give a time, or one already alive, is the
// game's own.
TEST(AreaFirstCopyOtherwiseUnchanged) {
  CHECK_EQ(FirstCopyStart(1, 0, 0), 0);
  CHECK_EQ(FirstCopyStart(0, 0, 0), 0);
  CHECK_EQ(FirstCopyStart(5, 300, 0), 300);
  CHECK_EQ(FirstCopyStart(5, 0, 2000), 0);
  CHECK_EQ(FirstCopyStart(-1, 0, 0), 0);
}

// AH: a copy is ours when its model's name starts "n5", any case - the bench's plays, nwn5e's shipped models - however
// the 16 chars end (a CResRef isn't always zero-terminated).
TEST(AreaOwnModels) {
  CHECK_EQ(IsOwnAreaModel("n5va0007", 16), true);  // !vfxarea's play of the Composer's dome (SHP-4)
  CHECK_EQ(IsOwnAreaModel("n5vx0001", 16), true);
  CHECK_EQ(IsOwnAreaModel("n5_detmag_h", 16), true);
  CHECK_EQ(IsOwnAreaModel("N5_ZOTRUTH", 16), true);
  CHECK_EQ(IsOwnAreaModel("n5v_puddle", 16), true);
  const char full[16] = {'n', '5', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'o'};
  CHECK_EQ(IsOwnAreaModel(full, sizeof(full)), true);  // all 16 used, no zero after them
  CHECK_EQ(IsOwnAreaModel("n5", 16), true);
}

// The game's models - in its own rows and in nwn5e's (Entangle 105, the fogs 107-115, the flasks 100-101) - keep the
// highlight they were made for; and nothing is read past `size`.
TEST(AreaGameModelsNotOurs) {
  CHECK_EQ(IsOwnAreaModel("vps_entangle", 16), false);
  CHECK_EQ(IsOwnAreaModel("vps_fogmind_L", 16), false);
  CHECK_EQ(IsOwnAreaModel("vps_sword_01", 16), false);
  CHECK_EQ(IsOwnAreaModel("", 16), false);
  CHECK_EQ(IsOwnAreaModel("n", 16), false);
  CHECK_EQ(IsOwnAreaModel("5n_x", 16), false);
  CHECK_EQ(IsOwnAreaModel("nw_x", 16), false);
  CHECK_EQ(IsOwnAreaModel("n6_x", 16), false);
  CHECK_EQ(IsOwnAreaModel("m5_x", 16), false);
  CHECK_EQ(IsOwnAreaModel("xn5", 16), false);
  CHECK_EQ(IsOwnAreaModel("n5va0007", 1), false);  // only "n" inside the size
  CHECK_EQ(IsOwnAreaModel("n5va0007", 0), false);
  CHECK_EQ(IsOwnAreaModel(nullptr, 16), false);
}

// AH: the lone copy - group 1 of one, never placed (start 0, not alive), never animated, ours - gets its own sequence.
TEST(AreaLoneCopyStarted) {
  CHECK_EQ(StartsLoneCopy(1, 0, 0, 0, true), true);  // !vfxarea n5_zotruth 0.6 1 0
}

// Not when the game's area code gives it one already (several in group 1: VA-5 places copy 0; a start; alive; an
// animation), when there's no group 1 copy, or when the model is the game's (a flask's splash: as the game leaves it).
TEST(AreaLoneCopyOtherwiseLeft) {
  CHECK_EQ(StartsLoneCopy(1, 0, 0, 0, false), false);
  CHECK_EQ(StartsLoneCopy(2, 0, 0, 0, true), false);
  CHECK_EQ(StartsLoneCopy(5, 0, 0, 0, true), false);
  CHECK_EQ(StartsLoneCopy(0, 0, 0, 0, true), false);
  CHECK_EQ(StartsLoneCopy(-1, 0, 0, 0, true), false);
  CHECK_EQ(StartsLoneCopy(1, 300, 0, 0, true), false);
  CHECK_EQ(StartsLoneCopy(1, 1, 0, 0, true), false);
  CHECK_EQ(StartsLoneCopy(1, 0, 2000, 0, true), false);
  CHECK_EQ(StartsLoneCopy(1, 0, 0, 1, true), false);
  CHECK_EQ(StartsLoneCopy(1, 0, 0, 2, true), false);
  CHECK_EQ(StartsLoneCopy(1, 0, 0, 3, true), false);
}

// AH's layout guard: counts as the loader leaves them, or the area is left alone.
TEST(AreaCopiesLayoutGuard) {
  CHECK_EQ(AreaCopiesLaidOut(1, 1), true);
  CHECK_EQ(AreaCopiesLaidOut(2, 2), true);
  CHECK_EQ(AreaCopiesLaidOut(15, 5), true);  // Entangle: 5 + 5 + 5
  CHECK_EQ(AreaCopiesLaidOut(3, 0), true);   // group 1 empty: group 2's copies
  CHECK_EQ(AreaCopiesLaidOut(kMaxAreaCopies, 30), true);
  CHECK_EQ(kMaxAreaCopies, 255);
  CHECK_EQ(AreaCopiesLaidOut(0, 0), false);  // no copies (and none to read)
  CHECK_EQ(AreaCopiesLaidOut(-1, 0), false);
  CHECK_EQ(AreaCopiesLaidOut(kMaxAreaCopies + 1, 1), false);
  CHECK_EQ(AreaCopiesLaidOut(3, 4), false);
  CHECK_EQ(AreaCopiesLaidOut(3, -1), false);
}

// --- The hook's anchors in the game's own nwmain.exe ---

namespace {

// The game's 64-bit nwmain.exe, read whole for the checks below (nothing of it is kept).
struct GameExe {
  struct Section {
    std::string name;
    uint32_t rva = 0, vsize = 0, raw = 0, rsize = 0;
  };
  std::string bytes;
  uint64_t base = 0;
  std::vector<Section> sections;
  uint32_t pdata = 0, pdata_size = 0;
  std::vector<uint32_t> roots;  // each .pdata entry's root (its function's first entry's begin)

  uint32_t U32(size_t at) const {
    uint32_t v = 0;
    if (at + 4 <= bytes.size()) memcpy(&v, bytes.data() + at, 4);
    return v;
  }
  // The bytes at `rva` (nullptr unless `size` of them are in one section's file data).
  const uint8_t* At(uint32_t rva, size_t size) const {
    for (const Section& s : sections) {
      if (rva >= s.rva && rva - s.rva + size <= s.rsize) {
        return reinterpret_cast<const uint8_t*>(bytes.data()) + s.raw + (rva - s.rva);
      }
    }
    return nullptr;
  }
  uint32_t Rva32(uint32_t rva) const {
    const uint8_t* p = At(rva, 4);
    uint32_t v = 0;
    if (p) memcpy(&v, p, 4);
    return v;
  }
  const Section* Named(const char* name) const {
    for (const Section& s : sections) {
      if (s.name == name) return &s;
    }
    return nullptr;
  }
  uint32_t RootOf(uint32_t function) const {  // `function`: a RUNTIME_FUNCTION's rva (hooks.cpp's RootBegin)
    for (int depth = 0; depth < 32; ++depth) {
      const uint32_t begin = Rva32(function), unwind = Rva32(function + 8);
      if (unwind & 1) {
        function = unwind & ~1u;
        continue;
      }
      const uint8_t* info = At(unwind, 4);
      if (!info) return 0;
      if (!((info[0] >> 3) & 4)) return begin;  // no UNW_FLAG_CHAININFO
      function = unwind + 4 + ((info[2] + 1) & ~1) * 2;
    }
    return 0;
  }
  bool Load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    const uint32_t pe = U32(0x3c);
    if (bytes.size() < 0x400 || pe + 0x200 > bytes.size() || memcmp(bytes.data() + pe, "PE\0\0", 4) != 0) return false;
    const uint16_t count = static_cast<uint16_t>(U32(pe + 6) & 0xffff);
    const uint16_t optional = static_cast<uint16_t>(U32(pe + 20) & 0xffff);
    memcpy(&base, bytes.data() + pe + 24 + 24, 8);
    pdata = U32(pe + 24 + 112 + 3 * 8);
    pdata_size = U32(pe + 24 + 112 + 3 * 8 + 4);
    for (uint16_t i = 0; i < count; ++i) {
      const size_t at = pe + 24 + optional + i * 40u;
      Section s;
      s.name.assign(bytes.data() + at, strnlen(bytes.data() + at, 8));
      s.vsize = U32(at + 8);
      s.rva = U32(at + 12);
      s.rsize = U32(at + 16);
      s.raw = U32(at + 20);
      sections.push_back(s);
    }
    for (uint32_t e = 0; e + 12 <= pdata_size; e += 12) roots.push_back(RootOf(pdata + e));
    return Named(".text") != nullptr && !roots.empty();
  }
  // Every fragment [begin, end) of the function containing `rva` (hooks.cpp's FunctionAt); empty if none.
  std::vector<std::pair<uint32_t, uint32_t>> FunctionParts(uint32_t rva) const {
    std::vector<std::pair<uint32_t, uint32_t>> parts;
    uint32_t root = 0;
    for (size_t i = 0; i < roots.size() && !root; ++i) {
      const uint32_t begin = Rva32(pdata + static_cast<uint32_t>(i) * 12), end = Rva32(pdata + static_cast<uint32_t>(i) * 12 + 4);
      if (rva >= begin && rva < end) root = roots[i];
    }
    for (size_t i = 0; root && i < roots.size(); ++i) {
      if (roots[i] == root) {
        parts.push_back({Rva32(pdata + static_cast<uint32_t>(i) * 12), Rva32(pdata + static_cast<uint32_t>(i) * 12 + 4)});
      }
    }
    return parts;
  }
  uint32_t Entry(uint32_t rva) const {  // hooks.cpp's FunctionEntry
    for (size_t i = 0; i < roots.size(); ++i) {
      const uint32_t begin = Rva32(pdata + static_cast<uint32_t>(i) * 12), end = Rva32(pdata + static_cast<uint32_t>(i) * 12 + 4);
      if (rva >= begin && rva < end) return roots[i];
    }
    return 0;
  }
  uint32_t CallTarget(uint32_t rva) const {
    const uint8_t* code = At(rva, 5);
    if (!code || code[0] != 0xe8) return 0;
    int32_t rel = 0;
    memcpy(&rel, code + 1, 4);
    return rva + 5 + rel;
  }
};

std::vector<int> PatternBytes(const char* text) {  // hooks.cpp's ParsePattern: "48 8B ?? 78", ?? any byte
  std::vector<int> pattern;
  for (const char* p = text; *p;) {
    while (*p == ' ') ++p;
    if (!*p) break;
    if (*p == '?') {
      pattern.push_back(-1);
      p += p[1] == '?' ? 2 : 1;
      continue;
    }
    pattern.push_back(static_cast<int>(strtoul(std::string(p, 2).c_str(), nullptr, 16)));
    p += 2;
  }
  return pattern;
}

// Where `pattern` occurs within [begin, end) (rvas).
std::vector<uint32_t> Matches(const GameExe& exe, uint32_t begin, uint32_t end, const std::vector<int>& pattern) {
  std::vector<uint32_t> found;
  const uint8_t* code = exe.At(begin, end - begin);
  if (!code || pattern.empty()) return found;
  for (uint32_t i = 0; i + pattern.size() <= end - begin; ++i) {
    size_t k = 0;
    while (k < pattern.size() && (pattern[k] < 0 || code[i + k] == pattern[k])) ++k;
    if (k == pattern.size()) found.push_back(begin + i);
  }
  return found;
}

std::vector<uint32_t> InText(const GameExe& exe, const std::vector<int>& pattern) {
  const GameExe::Section* text = exe.Named(".text");
  return Matches(exe, text->rva, text->rva + text->vsize, pattern);
}

std::vector<uint32_t> InFunction(const GameExe& exe, const std::vector<std::pair<uint32_t, uint32_t>>& parts,
                                 const std::vector<int>& pattern) {
  std::vector<uint32_t> found;
  for (const auto& part : parts) {
    for (uint32_t at : Matches(exe, part.first, part.second, pattern)) found.push_back(at);
  }
  return found;
}

std::vector<uint32_t> InData(const GameExe& exe, const std::string& needle) {  // in .rdata and .data
  std::vector<uint32_t> found;
  for (const char* name : {".rdata", ".data"}) {
    const GameExe::Section* s = exe.Named(name);
    if (!s) continue;
    const std::string data = exe.bytes.substr(s->raw, s->rsize);
    for (size_t at = data.find(needle); at != std::string::npos; at = data.find(needle, at + 1)) {
      found.push_back(s->rva + static_cast<uint32_t>(at));
    }
  }
  return found;
}

// The one function referencing `text` (hooks.cpp's FunctionByString: a `lea r64, [rip + x]` of it), 0 unless one.
uint32_t FunctionUsing(const GameExe& exe, const char* text) {
  const std::vector<uint32_t> strings = InData(exe, std::string(text, strlen(text) + 1));
  const GameExe::Section* code = exe.Named(".text");
  const uint8_t* b = exe.At(code->rva, code->vsize);
  std::vector<uint32_t> functions;
  for (uint32_t i = 0; b && i + 7 <= code->vsize; ++i) {
    if ((b[i] != 0x48 && b[i] != 0x4c) || b[i + 1] != 0x8d || (b[i + 2] & 0xc7) != 0x05) continue;
    int32_t disp = 0;
    memcpy(&disp, b + i + 3, 4);
    const uint32_t target = code->rva + i + 7 + disp;
    for (uint32_t s : strings) {
      const uint32_t entry = target == s ? exe.Entry(code->rva + i) : 0;
      bool known = false;
      for (uint32_t f : functions) known = known || f == entry;
      if (entry && !known) functions.push_back(entry);
    }
  }
  return functions.size() == 1 ? functions[0] : 0;
}

// A class's vtable from its RTTI name (hooks.cpp's FindVtable), 0 unless each step is unique.
uint32_t VtableOf(const GameExe& exe, const char* rtti) {
  const std::vector<uint32_t> names = InData(exe, std::string(rtti, strlen(rtti) + 1));
  const GameExe::Section* rdata = exe.Named(".rdata");
  if (names.size() != 1 || !rdata) return 0;
  const uint32_t descriptor = names[0] - 0x10;
  std::vector<uint32_t> locators;
  for (uint32_t at = rdata->rva; at + 24 <= rdata->rva + rdata->rsize; at += 4) {
    if (exe.Rva32(at) == 1 && exe.Rva32(at + 4) == 0 && exe.Rva32(at + 12) == descriptor && exe.Rva32(at + 20) == at) {
      locators.push_back(at);
    }
  }
  if (locators.size() != 1) return 0;
  const uint64_t pointer = exe.base + locators[0];
  const std::vector<uint32_t> slots = InData(exe, std::string(reinterpret_cast<const char*>(&pointer), 8));
  return slots.size() == 1 ? slots[0] + 8 : 0;
}

}  // namespace

// Every byte pattern core/anim_area_scatter checks before hooking, in the game's own nwmain.exe, as the hook checks it -
// a missing or repeated one would make the hook skip in game (VA-5 and AH alike). Only where the game is (NWN_GAME_DIR,
// or Steam's folder); nothing of it is kept.
TEST(AreaEnginePatternsInTheGame) {
  const char* given = getenv("NWN_GAME_DIR");
  const std::string game = given && *given ? given : "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
  GameExe exe;
  if (!exe.Load(game + "\\bin\\win32\\nwmain.exe")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  namespace ae = data::area_engine;
  const uint32_t load = FunctionUsing(exe, ae::kLoadEffectText);
  CHECK_EQ(load != 0, true);
  const auto load_parts = exe.FunctionParts(load);
  CHECK_EQ(load_parts.empty(), false);
  // VA-5's four.
  CHECK_EQ(InFunction(exe, load_parts, PatternBytes(ae::kGroup1CountPattern)).size(), 1u);
  CHECK_EQ(InFunction(exe, load_parts, PatternBytes(ae::kGroup1StartPattern)).size(), 1u);
  const std::vector<uint32_t> skipped = InText(exe, PatternBytes(ae::kStartSkippedPattern));
  CHECK_EQ(skipped.size(), 1u);
  CHECK_EQ(InText(exe, PatternBytes(ae::kAliveTestPattern)).size(), 1u);
  // AH: the copies' layout in LoadEffect.
  CHECK_EQ(InFunction(exe, load_parts, PatternBytes(ae::kCopiesPattern)).size(), 1u);
  CHECK_EQ(InFunction(exe, load_parts, PatternBytes(ae::kTotalCountPattern)).size(), 1u);
  // Hilite and UnHilite: function entries, the same RefreshHilite; LoadEffect's three slot-3 calls land on Hilite.
  const std::vector<uint32_t> hilite = InText(exe, PatternBytes(ae::kHilitePattern));
  const std::vector<uint32_t> unhilite = InText(exe, PatternBytes(ae::kUnHilitePattern));
  CHECK_EQ(hilite.size(), 1u);
  CHECK_EQ(unhilite.size(), 1u);
  if (hilite.size() == 1 && unhilite.size() == 1) {
    CHECK_EQ(exe.Entry(hilite[0]), hilite[0]);
    CHECK_EQ(exe.Entry(unhilite[0]), unhilite[0]);
    const uint32_t refresh = exe.CallTarget(unhilite[0] + static_cast<uint32_t>(ae::kUnHiliteRefreshCallAt));
    CHECK_EQ(refresh != 0, true);
    CHECK_EQ(exe.CallTarget(hilite[0] + static_cast<uint32_t>(ae::kHiliteRefreshCallAt)), refresh);
    const std::vector<int> slot3(ae::kHiliteSlot3Call, ae::kHiliteSlot3Call + sizeof(ae::kHiliteSlot3Call));
    const std::vector<uint32_t> calls = InFunction(exe, load_parts, slot3);
    CHECK_EQ(calls.size(), static_cast<size_t>(ae::kHiliteSlot3Calls));
    for (uint32_t call : calls) CHECK_EQ(exe.CallTarget(call + sizeof(ae::kHiliteSlot3Call) - 1), hilite[0]);
  }
  // The copy's class: its LoadModel keeps the model's Gob and name where the hook reads them.
  const uint32_t vtable = VtableOf(exe, ae::kCopyRtti);
  CHECK_EQ(vtable != 0, true);
  uint64_t load_model = 0;
  const uint8_t* slot = vtable ? exe.At(vtable + 8 * ae::kCopyLoadModelSlot, 8) : nullptr;
  if (slot) memcpy(&load_model, slot, 8);
  const uint32_t load_model_rva = load_model > exe.base ? static_cast<uint32_t>(load_model - exe.base) : 0;
  CHECK_EQ(load_model_rva != 0 && exe.Entry(load_model_rva) == load_model_rva, true);
  CHECK_EQ(InFunction(exe, exe.FunctionParts(load_model_rva), PatternBytes(ae::kModelNamePattern)).size(), 1u);
  // SetAnimation, and the update's call of it for a placed copy (in the function with VA-5's skip).
  const std::vector<uint32_t> set_animation = InText(exe, PatternBytes(ae::kSetAnimationPattern));
  const std::vector<uint32_t> impact = InText(exe, PatternBytes(ae::kImpactCallPattern));
  CHECK_EQ(set_animation.size(), 1u);
  CHECK_EQ(impact.size(), 1u);
  if (set_animation.size() == 1 && impact.size() == 1 && skipped.size() == 1) {
    CHECK_EQ(exe.Entry(set_animation[0]), set_animation[0]);
    CHECK_EQ(exe.CallTarget(impact[0] + static_cast<uint32_t>(ae::kImpactCallAt)), set_animation[0]);
    CHECK_EQ(exe.Entry(impact[0]), exe.Entry(skipped[0]));
  }
  printf("    nwmain.exe: LoadEffect rva 0x%x (%zu parts), Hilite 0x%x, UnHilite 0x%x, SetAnimation 0x%x, copy vtable 0x%x\n",
         load, load_parts.size(), hilite.empty() ? 0 : hilite[0], unhilite.empty() ? 0 : unhilite[0],
         set_animation.empty() ? 0 : set_animation[0], vtable);
}
