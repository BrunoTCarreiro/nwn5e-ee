#include "game_clock.h"

#include <windows.h>

#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using PauseStateFn = uint8_t (*)(void* app);

PauseStateFn g_pause_state = nullptr;
void** g_app_manager = nullptr;
uint64_t g_game_ms = 0;
uint64_t g_last_tick = 0;
bool g_started = false;
bool g_start_failed = false;
constexpr uint64_t kLongestStepMs = 1000;

bool Paused() {
  if (!g_pause_state || !g_app_manager || !*g_app_manager) return false;
  void* app = Field<void*>(*g_app_manager, ee::kAppManagerServerApp);
  return app && g_pause_state(app) != 0;
}

void Tick(void*) {
  const uint64_t now = GetTickCount64();
  if (now == g_last_tick) return;
  const uint64_t step = g_last_tick ? now - g_last_tick : 0;
  g_last_tick = now;
  if (step <= kLongestStepMs && !Paused()) g_game_ms += step;
}

}  // namespace

uint64_t GameMs() { return g_game_ms; }

bool StartGameClock() {
  if (g_started || g_start_failed) return g_started;
  g_pause_state = reinterpret_cast<PauseStateFn>(EngineExport(ee::kGetActivePauseState));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!g_pause_state || !g_app_manager) Log("game clock: the pause state is unknown - it runs through a pause");
  g_started = AddCreatureUpdateListener(&Tick);
  g_start_failed = !g_started;
  if (!g_started) Log("SKIP game clock: no creature update - game time stands still");
  return g_started;
}
