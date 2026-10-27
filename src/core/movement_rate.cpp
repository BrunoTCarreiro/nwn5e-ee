#include "movement_rate.h"

#include <vector>

#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using MovementRateFn = float (*)(void* creature);

MovementRateFn g_movement_rate = nullptr;
std::vector<MovementRateAdjuster> g_adjusters;  // filled at install, then only read
thread_local void* t_moving = nullptr;
bool g_hooked = false;
bool g_hook_failed = false;

float GetMovementRateFactor(void* creature) {
  void* outer = t_moving;
  t_moving = creature;
  float factor = g_movement_rate(creature);
  t_moving = outer;
  for (MovementRateAdjuster adjuster : g_adjusters) factor = adjuster(creature, factor);
  return factor;
}

}  // namespace

bool HookMovementRate() {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreature::GetMovementRateFactor (Fast Movement: whose rate; the grapple's drag)",
                          ee::kGetMovementRateFactor, reinterpret_cast<void*>(&GetMovementRateFactor),
                          reinterpret_cast<void**>(&g_movement_rate));
    g_hook_failed = !g_hooked;
  }
  return g_hooked;
}

bool AddMovementRateAdjuster(MovementRateAdjuster adjuster) {
  if (!HookMovementRate()) return false;
  g_adjusters.push_back(adjuster);
  return true;
}

void* MovingCreature() { return t_moving; }
