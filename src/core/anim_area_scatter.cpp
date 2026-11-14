// #161 VA-5: see anim_area_scatter.h.
#include "anim_area_scatter.h"

#include <stddef.h>
#include <stdint.h>

#include "data/anim_area_copies.h"
#include "hooks.h"
#include "log.h"

namespace {

// The client area object's copy state (EE 8193.x; the patterns below check each offset where the game uses it):
constexpr size_t kGroup1Count = 0x194;  // int: NUMACT01 ("cmp dword [rsi+0x194], 0" opens the loader's group 1 loop)
constexpr size_t kCopyAlive = 0x170;    // int*: each copy's time left alive, ms (0: not placed or between lives)
constexpr size_t kCopyStart = 0x178;    // int*: each copy's start time, ms (0: the update skips it - nothing pending)

// The loader's group 1 loop: its step (mov r12d, 300 - each copy's start k x 300 ms, copy 0's 0) and its count test.
// (Its store into kCopyStart's array is the same bytes as group 3's, so the update's read below checks that offset.)
constexpr const char* kGroup1CountPattern = "83 BE 94 01 00 00 00";
constexpr const char* kGroup1StartPattern = "41 BC 2C 01 00 00";
// The update: a copy not alive, its start 0 skipped (je); a copy alive (its kCopyAlive entry) handled first.
constexpr const char* kStartSkippedPattern = "48 8B 93 78 01 00 00 8B 04 BA 85 C0 0F 84";
constexpr const char* kAliveTestPattern = "48 8B 8B 70 01 00 00 48 63 FE 8B 04 B9 85 C0 0F 85";

using LoadEffectFn = uint64_t (*)(void* self, uint32_t row);  // CNWCAreaOfEffectObject::LoadEffect(row)
LoadEffectFn g_load = nullptr;
bool g_logged = false;

uint64_t LoadEffect(void* self, uint32_t row) {
  const uint64_t result = g_load(self, row);
  if (!self) return result;
  auto* object = static_cast<uint8_t*>(self);
  const int count = *reinterpret_cast<int*>(object + kGroup1Count);
  int* start = *reinterpret_cast<int**>(object + kCopyStart);
  const int* alive = *reinterpret_cast<int**>(object + kCopyAlive);
  if (count <= 0 || !start || !alive) return result;
  const int fixed = data::FirstCopyStart(count, start[0], alive[0]);
  if (fixed != start[0]) {
    start[0] = fixed;
    if (!g_logged) {
      g_logged = true;
      Log("area scatter: vfx_persistent row %u - group 1's first copy placed like its %d siblings (logged once)", row, count - 1);
    }
  }
  return result;
}

}  // namespace

void InstallAreaScatter() {
  const uintptr_t load = FunctionByString("area scatter: CNWCAreaOfEffectObject::LoadEffect",
                                          "CNWCAreaOfEffectObject::LoadEffect(): Failed to load area effect model 1.");
  if (!load) {
    Log("SKIP area scatter #161: the client's area loader not found - an area's first copy stays at its centre");
    return;
  }
  const CodeRange range = FunctionAt(load);
  const bool laid_out = FindPatternIn("area scatter: group 1's count", range, kGroup1CountPattern) &&
                        FindPatternIn("area scatter: group 1's start times", range, kGroup1StartPattern) &&
                        FindPattern("area scatter: a start of 0 skipped", kStartSkippedPattern) &&
                        FindPattern("area scatter: the copies' time alive", kAliveTestPattern);
  if (!laid_out) {
    Log("SKIP area scatter #161: the game's area code isn't the one checked - an area's first copy stays at its centre");
    return;
  }
  if (!HookAddress("client CNWCAreaOfEffectObject::LoadEffect (#161: an area's first copy scattered)", load,
                   reinterpret_cast<void*>(&LoadEffect), reinterpret_cast<void**>(&g_load))) {
    g_load = nullptr;
    return;
  }
  Log("area scatter: an area's first copy is placed like the others when its group has several (a lone one keeps the "
      "centre; #161)");
}
