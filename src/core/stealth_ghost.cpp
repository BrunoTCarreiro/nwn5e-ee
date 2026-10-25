// The hiding character's see-through look by the light (owner, 2026-10-25:
// "change the stealth vfx, make it dependent on the light level, 90% visible on
// light, 70% in dim and 50% in dark" - "do not make the ghostly effect pulse,
// have it maintain the transparency level"). While a player character hides,
// stealth_watch.cpp puts EE's steady ghost effect on it (VFX_DUR_GHOST_TRANSPARENT,
// 424: programmatic effect 420 - not the pulsing 401 / 404) and tells this file the
// opacity its light calls for. An object's transparency is its Gob's alpha
// (+0x108), set by Gob::SetColorShifting(colour, alpha, propagate) - read from
// nwmain: the only writer of that field besides the sky and the area loader. While
// an opacity is set, a see-through request (alpha under 1) gets it instead; the
// watch re-applies the ghost effect when the light changes, so the new value is
// set. The first requests are logged (caller, asked, set) to check what the ghost
// effect and the game's own stealth look send. `stealth_ghost` (2024 perception,
// stealth_light).

#include <windows.h>

#include <intrin.h>
#include <stdint.h>

#include <atomic>

#include "config.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using ColorShiftFn = void (*)(void* gob, const float* color, float alpha, int propagate);
ColorShiftFn g_color_shift = nullptr;
std::atomic<float> g_opacity{0.0f};  // 0: no override
std::atomic<int> g_logged{0};

void ColorShift(void* gob, const float* color, float alpha, int propagate) {
  const float want = g_opacity.load();
  const bool see_through = alpha < 0.999f;
  const float set = see_through && want > 0.0f ? want : alpha;
  if (see_through && g_logged.load() < 40) {
    ++g_logged;
    const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    Log("stealth ghost: see-through request from nwmain+0x%llx: alpha %.2f -> %.2f (gob %p)",
        static_cast<unsigned long long>(caller - base), alpha, set, gob);
  }
  g_color_shift(gob, color, set, propagate);
}

}  // namespace

// stealth_watch.cpp: the opacity the hiding character's light calls for (0.9 light,
// 0.7 dim, 0.5 dark), or 0 when no one hides.
void SetStealthOpacity(float opacity) { g_opacity.store(opacity); }

bool StealthGhostOn() {
  return g_config.stealth_ghost && g_config.stealth_light && g_config.perception == rules::RuleVariant::kFaithful;
}

void InstallStealthGhost() {
  if (!StealthGhostOn()) return;
  if (HookExport("Gob::SetColorShifting (a hiding character's see-through look by its light)",
                 "?SetColorShifting@Gob@@UEAAXVVector@@MH@Z", reinterpret_cast<void*>(&ColorShift),
                 reinterpret_cast<void**>(&g_color_shift))) {
    Log("stealth ghost: a hiding character is 90%% visible in light, 70%% in dim, 50%% in dark (stealth_ghost)");
  }
}
