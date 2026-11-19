// #161 VA-5 and AH (#39): see anim_area_scatter.h.
#include "anim_area_scatter.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "data/anim_area_copies.h"
#include "data/anim_area_engine.h"
#include "hooks.h"
#include "log.h"

namespace {

// The client area object's copy state and its functions: data/anim_area_engine.h (each offset checked by a pattern
// where the game uses it - VA-5's four below, AH's in FindAreaCopyCode).
namespace ae = data::area_engine;

template <typename T>
T Read(const void* object, size_t offset) {
  T value;
  memcpy(&value, static_cast<const uint8_t*>(object) + offset, sizeof(value));
  return value;
}

using LoadEffectFn = uint64_t (*)(void* self, uint32_t row);           // CNWCAreaOfEffectObject::LoadEffect(row): 1 loaded
using UnHiliteFn = int (*)(void* copy, uint8_t slot);                  // CNWCAnimBase::UnHilite(slot)
using SetAnimationFn = int (*)(void* self, uint16_t animation, uint8_t copy);  // CNWCAreaOfEffectObject::SetAnimation
LoadEffectFn g_load = nullptr;
// AH: set only when every check in FindAreaCopyCode passed (else VA-5 alone).
UnHiliteFn g_unhilite = nullptr;
SetAnimationFn g_set_animation = nullptr;
uintptr_t g_copy_vtable = 0;  // CNWCAnimBaseVisualEffect's vtable: every copy's first field
bool g_logged = false;
bool g_logged_unlit = false;
bool g_logged_lone = false;
bool g_logged_layout = false;

// VA-5: group 1's copy 0 placed like its siblings.
void ScatterFirstCopy(uint8_t* object, uint32_t row) {
  const int count = Read<int>(object, ae::kGroup1Count);
  int* start = Read<int*>(object, ae::kCopyStart);
  const int* alive = Read<int*>(object, ae::kCopyAlive);
  if (count <= 0 || !start || !alive) return;
  const int fixed = data::FirstCopyStart(count, start[0], alive[0]);
  if (fixed != start[0]) {
    start[0] = fixed;
    if (!g_logged) {
      g_logged = true;
      Log("area scatter: vfx_persistent row %u - group 1's first copy placed like its %d siblings (logged once)", row, count - 1);
    }
  }
}

// AH, after the loader (and VA-5): nwn5e's own copies lose the loader's white highlight (slot 3) - their material drawn
// as on a creature play: the tint, Glow, the area's light - and a lone group 1 copy of ours, which the update never
// places, starts the sequence the update gives a placed copy: SetAnimation(1) - the impact once, its timer the impact's
// length, then (AIUpdate) the duration looped. Its lifetime stays 0 (ResetTimer would give it the row's): a lifetime
// would end in the cessation and SetMoveTimer's start of 1 - re-placed at random in the radius, not where it stands.
// Nothing re-applies slot 3 later: the area code's only Hilite calls are LoadEffect's three; the hover's
// (UpdateMouseCursorAppearance) is slot 1 on copy 0 - the object's animation base - drawn over it while the pointer is
// there and cleared alone; RefreshHilite only reads the slots; graphics.hilite.* only set globals. (A self-illumination
// or alpha visual effect put on the area object itself would set copy 0's slot 3 again: nwn5e's areas never get one.)
// Any count or class not as the loader leaves them: logged once, the area left alone.
void OwnCopies(uint8_t* object, uint32_t row) {
  const int total = Read<int>(object, ae::kTotalCount);
  uint8_t* copies = Read<uint8_t*>(object, ae::kCopies);
  if (total == 0 || !copies) return;  // no copies: a row without models (Alarm's ward, a cantrip's patch)
  const int group1 = Read<int>(object, ae::kGroup1Count);
  bool laid_out = data::AreaCopiesLaidOut(total, group1);
  for (int i = 0; laid_out && i < total; ++i) {
    laid_out = Read<uintptr_t>(copies + static_cast<size_t>(i) * ae::kCopyStride, 0) == g_copy_vtable;
  }
  if (!laid_out) {
    if (!g_logged_layout) {
      g_logged_layout = true;
      Log("SKIP area copies: vfx_persistent row %u's copies aren't as the loader lays them out (%d in all, group 1's %d, "
          "or one of another class) - left as the game made them (logged once)",
          row, total, group1);
    }
    return;
  }
  const auto name_of = [](const uint8_t* copy) { return reinterpret_cast<const char*>(copy + ae::kCopyModelName); };
  int unlit = 0;
  const char* first = nullptr;
  for (int i = 0; i < total; ++i) {
    uint8_t* copy = copies + static_cast<size_t>(i) * ae::kCopyStride;
    if (!Read<void*>(copy, ae::kCopyModel) || !data::IsOwnAreaModel(name_of(copy), ae::kCopyModelNameSize)) continue;
    if (!Read<int>(copy, ae::kCopyHiliteOn + 4 * ae::kAreaHiliteSlot)) continue;  // graphics.hilite.enabled off: none
    if (!g_unhilite(copy, ae::kAreaHiliteSlot)) continue;
    ++unlit;
    if (!first) first = name_of(copy);
  }
  if (unlit && !g_logged_unlit) {
    g_logged_unlit = true;
    Log("area copies: vfx_persistent row %u - %d of its %d copies nwn5e's models (%.16s): the area's white highlight "
        "taken off, drawn as on a creature (logged once)",
        row, unlit, total, first);
  }
  const int* start = Read<int*>(object, ae::kCopyStart);
  const int* alive = Read<int*>(object, ae::kCopyAlive);
  const uint16_t* state = Read<uint16_t*>(object, ae::kCopyState);
  const int* timer = Read<int*>(object, ae::kCopyTimer);
  if (group1 < 1 || !start || !alive || !state || !timer || !Read<void*>(copies, ae::kCopyModel)) return;
  const bool own = data::IsOwnAreaModel(name_of(copies), ae::kCopyModelNameSize);
  if (!data::StartsLoneCopy(group1, start[0], alive[0], state[0], own)) return;
  g_set_animation(object, ae::kAreaImpact, 0);
  if (!g_logged_lone) {
    g_logged_lone = true;
    Log("area copies: vfx_persistent row %u - its lone copy (%.16s) plays its impact once, then its duration looped, "
        "where it stands (logged once)",
        row, name_of(copies));
  }
}

uint64_t LoadEffect(void* self, uint32_t row) {
  const uint64_t result = g_load(self, row);
  if (!self) return result;
  auto* object = static_cast<uint8_t*>(self);
  ScatterFirstCopy(object, row);
  if (g_unhilite && static_cast<uint32_t>(result) != 0) OwnCopies(object, row);
  return result;
}

// AH's code, each piece checked where the game uses it: Hilite and UnHilite (entries, one RefreshHilite), LoadEffect's
// three Hilite calls with slot 3, the copies' array and total (LoadEffect), a copy's model and name (its LoadModel, by
// its class's vtable), SetAnimation and the update's call of it for a placed copy. False (logged) unless all are.
bool FindAreaCopyCode(const CodeRange& load, uintptr_t update_skip) {
  const uintptr_t hilite = FindPattern("area copies: CNWCAnimBase::Hilite", ae::kHilitePattern);
  const uintptr_t unhilite = FindPattern("area copies: CNWCAnimBase::UnHilite", ae::kUnHilitePattern);
  const uintptr_t set_animation = FindPattern("area copies: the area's SetAnimation", ae::kSetAnimationPattern);
  const uintptr_t impact = FindPattern("area copies: the update's impact for a placed copy", ae::kImpactCallPattern);
  const uintptr_t vtable = FindVtable("area copies: a copy's class", ae::kCopyRtti);
  const uintptr_t load_model = VtableSlot(vtable, ae::kCopyLoadModelSlot);
  if (!hilite || !unhilite || !set_animation || !impact || !load_model) return false;
  const std::vector<uintptr_t> calls = FindCallsTo(load, hilite);
  bool slot3 = calls.size() == static_cast<size_t>(ae::kHiliteSlot3Calls);
  for (uintptr_t call : calls) {
    const auto* before = reinterpret_cast<const void*>(call - (sizeof(ae::kHiliteSlot3Call) - 1));
    slot3 = slot3 && memcmp(before, ae::kHiliteSlot3Call, sizeof(ae::kHiliteSlot3Call)) == 0;
  }
  const uintptr_t refresh = CallTarget(unhilite + ae::kUnHiliteRefreshCallAt);
  if (FunctionEntry(hilite) != hilite || FunctionEntry(unhilite) != unhilite ||
      FunctionEntry(set_animation) != set_animation || !refresh ||
      CallTarget(hilite + ae::kHiliteRefreshCallAt) != refresh || !slot3 ||
      CallTarget(impact + ae::kImpactCallAt) != set_animation || !FunctionAt(update_skip).Contains(impact)) {
    Log("SKIP area copies: the game's highlight and area calls aren't the ones checked (%zu Hilite calls in the loader)",
        calls.size());
    return false;
  }
  if (!FindPatternIn("area copies: the copies built", load, ae::kCopiesPattern) ||
      !FindPatternIn("area copies: their total", load, ae::kTotalCountPattern) ||
      !FindPatternIn("area copies: a copy's model and its name", FunctionAt(load_model), ae::kModelNamePattern)) {
    return false;
  }
  g_unhilite = reinterpret_cast<UnHiliteFn>(unhilite);
  g_set_animation = reinterpret_cast<SetAnimationFn>(set_animation);
  g_copy_vtable = vtable;
  return true;
}

}  // namespace

void InstallAreaScatter() {
  const uintptr_t load = FunctionByString("area scatter: CNWCAreaOfEffectObject::LoadEffect", ae::kLoadEffectText);
  if (!load) {
    Log("SKIP area scatter #161: the client's area loader not found - an area's first copy stays at its centre");
    return;
  }
  const CodeRange range = FunctionAt(load);
  const uintptr_t update_skip = FindPattern("area scatter: a start of 0 skipped", ae::kStartSkippedPattern);
  const bool laid_out = FindPatternIn("area scatter: group 1's count", range, ae::kGroup1CountPattern) &&
                        FindPatternIn("area scatter: group 1's start times", range, ae::kGroup1StartPattern) &&
                        update_skip && FindPattern("area scatter: the copies' time alive", ae::kAliveTestPattern);
  if (!laid_out) {
    Log("SKIP area scatter #161: the game's area code isn't the one checked - an area's first copy stays at its centre");
    return;
  }
  const bool own_copies = FindAreaCopyCode(range, update_skip);
  if (!own_copies) {
    Log("SKIP area copies #39 (AH): nwn5e's area copies keep the game's white highlight (a tint drawn white), a lone "
        "one unanimated");
  }
  if (!HookAddress("client CNWCAreaOfEffectObject::LoadEffect (#161: an area's first copy scattered)", load,
                   reinterpret_cast<void*>(&LoadEffect), reinterpret_cast<void**>(&g_load))) {
    g_load = nullptr;
    g_unhilite = nullptr;
    return;
  }
  Log("area scatter: an area's first copy is placed like the others when its group has several (a lone one keeps the "
      "centre; #161)");
  if (own_copies) {
    Log("area copies: nwn5e's own models in an area drawn as on a creature (no white highlight), a lone one fading in "
        "once where it stands (#39 AH)");
  }
}
