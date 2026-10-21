// The NUI test (owner's go, 2026-10-21: Fighting Styles are chosen in a NUI
// window, option A; first a spike): proves nwn5e can open its own window and
// receive its clicks. `!nui` opens it; [debug] nui_test=1 also opens it 3 s
// after a player character arrives (once per session). A style button answers
// in the window (a bind) and in chat — nothing is chosen; Close closes it.

#include <windows.h>

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "log.h"
#include "nui.h"

namespace {

constexpr const char* kWindow = "nwn5etest";
constexpr DWORD kArrivalDelayMs = 3000;

struct Choice {
  const char* id;
  const char* label;
};
const Choice kStyles[] = {
    {"archery", "Archery"}, {"defense", "Defense"}, {"greatweapon", "Great Weapon"}, {"twoweapon", "Two-Weapon"}};

std::unordered_map<uint32_t, DWORD> g_arrived;  // player character -> first seen in an area (0 = opened)

std::string Layout() {
  using namespace nui;
  return Window(
      Col(List({
          Label(Str("nwn5e can open its own windows. Click a style: the DLL answers below and in chat."), 40.0f),
          Label(Bind("status"), 30.0f),
          Row(List({Button(kStyles[0].id, Str(kStyles[0].label)), Button(kStyles[1].id, Str(kStyles[1].label)),
                    Button(kStyles[2].id, Str(kStyles[2].label)), Button(kStyles[3].id, Str(kStyles[3].label))})),
          Row(List({Spacer(), Button("close", Str("Close"), 120.0f), Spacer()})),
      })),
      Str("nwn5e - NUI test"), 200.0f, 150.0f, 560.0f, 240.0f);
}

int Open(uint32_t player) { return NuiOpen(player, kWindow, Layout(), {{"status", "Nothing clicked yet."}}); }

std::vector<std::string> NuiCommand(void* creature, const std::string& /*args*/) {
  const int token = Open(Field<uint32_t>(creature, ee::kObjectId));
  if (!token) return {};  // the script already told the player why
  return {"nwn5e: NUI test window opened (token " + std::to_string(token) + ")."};
}

void OnEvent(const NuiEvent& event) {
  if (event.type != "click") return;
  if (event.element == "close") {
    NuiClose(event.player, kWindow);
    return;
  }
  for (const Choice& style : kStyles) {
    if (event.element != style.id) continue;
    NuiSetText(event.player, kWindow, "status", std::string("You clicked ") + style.label + " - the DLL got it.");
    TellPlayer(event.player, std::string("nwn5e: NUI test - the DLL got your click on ") + style.label +
                                 " (nothing is chosen; this is only the test).");
    return;
  }
}

void OnArrival(void* creature) {
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature) || !ReadCreatureFacts(stats).is_player) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto seen = g_arrived.find(id);
  if (seen == g_arrived.end()) {
    g_arrived[id] = GetTickCount() | 1;  // never 0
    return;
  }
  if (seen->second == 0 || GetTickCount() - seen->second < kArrivalDelayMs) return;
  seen->second = 0;
  Open(id);
}

}  // namespace

void InstallNuiTest() {
  if (!AddNuiWindow(kWindow, &OnEvent)) return;
  AddChatCommand("!nui", &NuiCommand);
  if (g_config.debug_nui_test) AddCreatureUpdateListener(&OnArrival);
}
