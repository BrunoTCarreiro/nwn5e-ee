// The server's frame (server_tick.h): CServerExoApp::MainLoop hooked, its listeners called after each pass.
// #171 (0.99.87; owner, 2026-11-19, on TW-1: "this only works if the game is not paused", then on 0.99.86: "doesn't
// work. now unpaused also only allows to pick one target"): the target window's opening and its cursor need "a moment
// after the player's order", outside the order's own handling, and a paused game runs neither DelayCommand nor
// creature updates - the main loop runs every frame regardless.
#include "server_tick.h"

#include <vector>

#include "hooks.h"
#include "log.h"

namespace {

using MainLoopFn = int (*)(void* app);
constexpr const char* kServerMainLoop = "?MainLoop@CServerExoApp@@QEAAHXZ";
MainLoopFn g_main_loop = nullptr;
std::vector<ServerTickListener> g_listeners;
bool g_hooked = false;
bool g_failed = false;

int MainLoop(void* app) {
  const int result = g_main_loop(app);
  for (ServerTickListener listener : g_listeners) listener();
  return result;
}

}  // namespace

bool AddServerTickListener(ServerTickListener listener) {
  if (!listener) return false;
  if (!g_hooked && !g_failed) {
    g_hooked = HookExport("CServerExoApp::MainLoop (the server's frame, paused or not)", kServerMainLoop,
                          reinterpret_cast<void*>(&MainLoop), reinterpret_cast<void**>(&g_main_loop));
    g_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_listeners.push_back(listener);
  return true;
}
