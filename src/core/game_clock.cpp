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

bool g_was_paused = false;
int g_pause_lines = 0;

void Tick(void*) {
  const uint64_t now = GetTickCount64();
  if (now == g_last_tick) return;
  const uint64_t step = g_last_tick ? now - g_last_tick : 0;
  g_last_tick = now;
  const bool paused = Paused();
  // 0.50.4: the pauses, so the rules timed by this clock (Rage, Withdraw, the
  // repeat saves) can be read against them in the log.
  if (paused != g_was_paused) {
    g_was_paused = paused;
    if (g_pause_lines < 400 && ++g_pause_lines) {
      Log("game clock: %s at game %.1f s", paused ? "paused" : "running", g_game_ms / 1000.0);
    }
  }
  if (step <= kLongestStepMs && !paused) g_game_ms += step;
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
