// The HUD clock (rules/hud_clock.h; #105; owner, 2026-11-01: "there should be
// a clock on top of the compass on the bottom right ... the sun and moon icons
// are a nice touch"). One client NUI window, as the resource panel's:
// - The server's side (a creature update, the single-player character, once a
//   second): NWN's clock - GetTimeHour / Minute / Second, HoursToSeconds(1),
//   GetIsDawn / Day / Dusk - and the screen's GUI size (every 2 s), through
//   the script bridge; handed over when the shown time or phase changes.
// - The client's side (every frame): a transparent window (input passes
//   through) with a draw list - a dark card, the sun (a disc in a ring of
//   dots; orange at dawn and dusk) or the moon (a pale disc, a shadow over
//   part of it), each turned on by a bind, and the time - placed with
//   rules::PlaceClock above the compass (bottom right); shown three frames
//   after it opens (NUI opens a window at the screen's centre). Closed outside
//   a game (no in-game GUI) and with `!clock` off.
// The compass's place isn't read from the engine yet: the spot is a first
// guess the owner tunes (clock_x / clock_y, `!test clock`; 0.59.9: `!clock
// right / bottom <n>` live, `!clock where`).

#include "clock_panel.h"

#include <windows.h>

#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "chat.h"
#include "client_ui.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "rules/hud_clock.h"

namespace {

constexpr const char* kVarOff = "nwn5e_clock_off";
constexpr const char* kWindowId = "n5clock";
constexpr ULONGLONG kCheckEveryMs = 1000;
constexpr ULONGLONG kGuiEveryMs = 2000;
constexpr int kRevealFrames = 3;
constexpr float kWindowPad = 4.0f;  // the window's edge -> the group (as the resource panel's)
constexpr float kGroupPad = 4.0f;   // the group -> the label (the draw list's origin)
constexpr float kWindowSlack = 2.0f * (kWindowPad + kGroupPad);
constexpr size_t kClientAppInternal = 0x8;  // the resource panel's chain to the in-game GUI
constexpr size_t kAppGui = 0x90;
constexpr size_t kGuiQuickbar = 0x60;

ClockOffsets g_offsets{rules::kClockFromRight, rules::kClockFromBottom};

// Server -> client.
struct Reading {
  std::string time;
  rules::ClockPhase phase = rules::ClockPhase::kDay;
  float gui_w = 0.0f, gui_h = 0.0f, gui_s = 1.0f;
  bool on = false;
};
std::mutex g_lock;
Reading g_published;
int g_version = 0;

struct PlayerState {
  ULONGLONG next = 0;
  ULONGLONG next_gui = 0;
  Reading reading;
};
std::unordered_map<uint32_t, PlayerState> g_players;
uint32_t g_client_player = ee::kObjectInvalid;
int g_logged = 0;

uint32_t IdOf(const void* creature) { return creature ? Field<uint32_t>(creature, ee::kObjectId) : ee::kObjectInvalid; }
bool Off(void* creature) { return engine::GetLocalInt(creature, kVarOff) != 0; }

void Publish(const Reading& reading) {
  std::lock_guard<std::mutex> hold(g_lock);
  g_published = reading;
  ++g_version;
}

void ClockCreatureUpdate(void* creature) {
  const uint32_t id = IdOf(creature);
  if (id == ee::kObjectInvalid) return;
  const ULONGLONG now = GetTickCount64();
  PlayerState& state = g_players[id];
  if (now < state.next) return;
  state.next = now + kCheckEveryMs;
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature) || !ReadCreatureFacts(stats).is_player || !ClientNuiReady()) return;
  // The game's own client: the single-player character (a new one after a load takes over).
  if (g_client_player != id && g_client_player != ee::kObjectInvalid && CreatureById(g_client_player)) return;
  g_client_player = id;
  EnsureRecords(creature);
  Reading reading = state.reading;
  reading.on = !Off(creature);
  if (reading.on) {
    const bool gui = now >= state.next_gui;
    if (gui) state.next_gui = now + kGuiEveryMs;
    std::string script =
        "object p = OBJECT_SELF; SetLocalInt(p, \"nwn5e_clk_h\", GetTimeHour());"
        " SetLocalInt(p, \"nwn5e_clk_m\", GetTimeMinute()); SetLocalInt(p, \"nwn5e_clk_s\", GetTimeSecond());"
        " SetLocalInt(p, \"nwn5e_clk_sph\", FloatToInt(HoursToSeconds(1)));"
        " int f = 3; if (GetIsDawn()) f = 0; else if (GetIsDusk()) f = 2; else if (GetIsDay()) f = 1;"
        " SetLocalInt(p, \"nwn5e_clk_ph\", f);";
    if (gui) {
      script += " SetLocalInt(p, \"nwn5e_clk_gw\", GetPlayerDeviceProperty(p, \"gui_width\"));"
                " SetLocalInt(p, \"nwn5e_clk_gh\", GetPlayerDeviceProperty(p, \"gui_height\"));"
                " SetLocalInt(p, \"nwn5e_clk_gs\", GetPlayerDeviceProperty(p, \"gui_scale\"));";
    }
    RunScriptChunk(script.c_str(), id);
    const int minute = rules::ClockGameMinute(engine::GetLocalInt(creature, "nwn5e_clk_m"),
                                              engine::GetLocalInt(creature, "nwn5e_clk_s"),
                                              engine::GetLocalInt(creature, "nwn5e_clk_sph"));
    reading.time = rules::ClockText(engine::GetLocalInt(creature, "nwn5e_clk_h"), minute);
    const int phase = engine::GetLocalInt(creature, "nwn5e_clk_ph");
    reading.phase = rules::ClockPhaseOf(phase == 0, phase == 1, phase == 2);
    if (gui) {
      reading.gui_w = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_clk_gw"));
      reading.gui_h = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_clk_gh"));
      const int percent = engine::GetLocalInt(creature, "nwn5e_clk_gs");  // 140 = 1.4
      reading.gui_s = percent >= 50 && percent <= 400 ? percent / 100.0f : 1.0f;
    }
  }
  const Reading& old = state.reading;
  const bool changed = reading.on != old.on || reading.time != old.time || reading.phase != old.phase ||
                       reading.gui_w != old.gui_w || reading.gui_h != old.gui_h || reading.gui_s != old.gui_s;
  state.reading = reading;
  if (!changed) return;
  if (g_logged < 4 && reading.on && reading.gui_w > 0.0f) {
    ++g_logged;
    Log("hud: the clock %s (window %.0f x %.0f, GUI scale %.2f, %d / %d GUI units from the right / bottom)",
        reading.time.c_str(), reading.gui_w, reading.gui_h, reading.gui_s, g_offsets.from_right,
        g_offsets.from_bottom);
  }
  Publish(reading);
}

// --- The client's side ---------------------------------------------------------------------------

struct Window {
  int token = 0;
  int frames = 0;
  bool revealed = false;
  std::string sent_time;
  int sent_phase = -1;
  float sent_x = -1.0e6f, sent_y = -1.0e6f;
};
Window g_window;
int g_seen_version = 0;
Reading g_reading;
void** g_app_manager = nullptr;

bool InGame() {
  if (!g_app_manager || !*g_app_manager) return false;
  void* client = *static_cast<void**>(*g_app_manager);
  void* app = client ? Field<void*>(client, kClientAppInternal) : nullptr;
  void* gui = app ? Field<void*>(app, kAppGui) : nullptr;
  return gui && Field<void*>(gui, kGuiQuickbar);
}

std::string Rect(float x, float y, float w, float h) {
  char text[128];
  sprintf_s(text, "{\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f}", x, y, w, h);
  return text;
}

std::string Colour(int r, int g, int b, int a) {
  char text[96];
  sprintf_s(text, "{\"r\":%d,\"g\":%d,\"b\":%d,\"a\":%d}", r, g, b, a);
  return text;
}

std::string Bind(const char* name) { return std::string("{\"bind\":\"") + name + "\"}"; }

// Draw-list items (nw_inc_nui.nss: NuiDrawListItem; 2 circle, 4 text, 7 rect), turned on by `enabled`'s bind.
std::string Shape(int type, const char* enabled, const std::string& colour, bool fill, float thickness, float x,
                  float y, float w, float h) {
  char head[200];
  sprintf_s(head, "{\"type\":%d,\"enabled\":{\"bind\":\"%s\"},\"fill\":%s,\"line_thickness\":%.1f,\"order\":1,"
            "\"render\":0,\"arrayBinds\":false,\"color\":", type, enabled, fill ? "true" : "false", thickness);
  return head + colour + ",\"rect\":" + Rect(x, y, w, h) + "}";
}

std::string Circle(const char* enabled, const std::string& colour, bool fill, float cx, float cy, float r) {
  return Shape(2, enabled, colour, fill, 1.5f, cx - r, cy - r, 2.0f * r, 2.0f * r);
}

constexpr float kIconX = 10.0f;  // the icon's centre in the content (GUI units)
constexpr float kIconY = 9.0f;

std::string ClockLayout() {
  const float w = rules::kClockWidth, h = rules::kClockHeight;
  std::string items = Shape(7, "on", Colour(0, 0, 0, 150), true, 1.0f, -kGroupPad, -kGroupPad, w + 2.0f * kGroupPad,
                            h + 2.0f * kGroupPad);
  items += "," + Shape(7, "on", Colour(150, 160, 170, 160), false, 1.0f, -kGroupPad, -kGroupPad, w + 2.0f * kGroupPad,
                       h + 2.0f * kGroupPad);
  // The sun: a disc in a ring of eight dots, its colour a bind (yellow; orange at dawn and dusk).
  items += "," + Circle("sun", Bind("sun_c"), true, kIconX, kIconY, 4.2f);
  for (int k = 0; k < 8; ++k) {
    const float a = static_cast<float>(k) * 3.14159265f / 4.0f;
    items += "," + Circle("sun", Bind("sun_c"), true, kIconX + 7.2f * cosf(a), kIconY + 7.2f * sinf(a), 1.2f);
  }
  // The moon: a pale disc with a shadow over its upper right.
  items += "," + Circle("moon", Colour(215, 225, 255, 255), true, kIconX, kIconY, 6.2f);
  items += "," + Circle("moon", Colour(10, 12, 24, 215), true, kIconX + 3.2f, kIconY - 2.2f, 5.4f);
  // The time.
  items += ",{\"type\":4,\"enabled\":{\"bind\":\"on\"},\"fill\":null,\"line_thickness\":null,\"order\":1,\"render\":0,"
           "\"arrayBinds\":false,\"color\":" + Colour(240, 240, 240, 255) + ",\"rect\":" + Rect(24.0f, 0.0f, w - 24.0f, h) +
           ",\"text\":" + Bind("t") + ",\"font\":\"\"}";
  char size[96];
  sprintf_s(size, "\"width\":%.1f,\"height\":%.1f,", w, h);
  const std::string element = std::string("{\"type\":\"label\",\"label\":null,\"value\":\"\",") + size +
                              "\"draw_list\":[" + items + "],\"draw_list_scissor\":false}";
  const std::string root = "{\"type\":\"group\",\"label\":null,\"value\":null,\"border\":false,\"scrollbars\":0,"
                           "\"children\":[" + element + "]}";
  return "{\"version\":1,\"title\":false,\"root\":" + root +
         ",\"geometry\":{\"bind\":\"geometry\"},\"resizable\":false,\"collapsed\":false,\"closable\":false,"
         "\"transparent\":true,\"border\":false,\"accepts_input\":false,\"size_constraint\":null,"
         "\"edge_constraint\":null,\"font\":\"\"}";
}

void CloseClock() {
  if (g_window.token) ClientNuiClose(g_window.token);
  g_window = Window();
}

void ClockTick() {
  {
    std::lock_guard<std::mutex> hold(g_lock);
    if (g_version != g_seen_version) {
      g_seen_version = g_version;
      g_reading = g_published;
    }
  }
  if (!InGame() || !g_reading.on || g_reading.time.empty() || g_reading.gui_w <= 0.0f || g_reading.gui_h <= 0.0f) {
    if (g_window.token) CloseClock();
    return;
  }
  if (!g_window.token) {
    g_window.token = ClientNuiOpen(kWindowId, ClockLayout(), nullptr);
    if (!g_window.token) return;
    ClientNuiSetJson(g_window.token, "on", "false");
    ClientNuiSetJson(g_window.token, "sun", "false");
    ClientNuiSetJson(g_window.token, "moon", "false");
  }
  const float window_w = rules::kClockWidth + kWindowSlack, window_h = rules::kClockHeight + kWindowSlack;
  const rules::HudBox spot =
      rules::PlaceClock(g_reading.gui_w, g_reading.gui_h, g_reading.gui_s, window_w, window_h,
                        static_cast<float>(g_offsets.from_right), static_cast<float>(g_offsets.from_bottom),
                        kWindowPad);
  if (fabsf(spot.x - g_window.sent_x) > 0.4f || fabsf(spot.y - g_window.sent_y) > 0.4f) {
    ClientNuiSetJson(g_window.token, "geometry", Rect(spot.x, spot.y, spot.w, spot.h));
    g_window.sent_x = spot.x;
    g_window.sent_y = spot.y;
  }
  if (g_reading.time != g_window.sent_time) {
    ClientNuiSetText(g_window.token, "t", g_reading.time);
    g_window.sent_time = g_reading.time;
  }
  if (!g_window.revealed) {
    if (++g_window.frames < kRevealFrames) return;
    g_window.revealed = true;
    ClientNuiSetJson(g_window.token, "on", "true");
    g_window.sent_phase = -1;
  }
  const int phase = static_cast<int>(g_reading.phase);
  if (phase == g_window.sent_phase) return;
  const bool night = g_reading.phase == rules::ClockPhase::kNight;
  const bool low_sun = g_reading.phase == rules::ClockPhase::kDawn || g_reading.phase == rules::ClockPhase::kDusk;
  ClientNuiSetJson(g_window.token, "sun", night ? "false" : "true");
  ClientNuiSetJson(g_window.token, "moon", night ? "true" : "false");
  ClientNuiSetJson(g_window.token, "sun_c", low_sun ? Colour(255, 150, 60, 255) : Colour(255, 214, 70, 255));
  g_window.sent_phase = phase;
}

// "!clock right <n>" / "!clock bottom <n>" (GUI units, 0..2000) / "!clock where": the spot, while it's fitted.
std::string Where() {
  char text[200];
  sprintf_s(text, "nwn5e: the clock - right %d, bottom %d GUI units (!clock right <n> / !clock bottom <n> moves it; "
            "tell the game session the numbers that fit).", g_offsets.from_right, g_offsets.from_bottom);
  return text;
}

std::vector<std::string> ClockCommand(void* creature, const std::string& args) {
  const uint32_t id = IdOf(creature);
  std::string word = args;
  for (char& c : word) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  const size_t space = word.find(' ');
  const std::string what = word.substr(0, space);
  if (what == "where") return {Where()};
  if (what == "right" || what == "bottom") {
    const int value = space == std::string::npos ? -1 : atoi(word.substr(space + 1).c_str());
    if (value < 0 || value > 2000) return {"nwn5e: !clock right <0..2000> or !clock bottom <0..2000> (GUI units)"};
    (what == "right" ? g_offsets.from_right : g_offsets.from_bottom) = value;
    Log("hud: 0x%08X the clock moved: right %d, bottom %d", id, g_offsets.from_right, g_offsets.from_bottom);
    return {Where()};
  }
  EnsureRecords(creature);
  const bool off = !Off(creature);
  SetRecord(creature, kVarOff, off ? 1 : 0);
  g_players[id].next = 0;
  Log("hud: 0x%08X the clock turned %s", id, off ? "off" : "on");
  return {off ? "nwn5e: the clock is off (!clock turns it back on)."
              : "nwn5e: the clock is on - the game time over the compass (!clock turns it off)."};
}

}  // namespace

ClockOffsets& ClockOffsetsForTests() { return g_offsets; }

void InstallHudClock() {
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  RegisterRecords({kVarOff});
  AddChatCommand("!clock", &ClockCommand);
  AddClientTickListener(&ClockTick);
  if (!AddCreatureUpdateListener(&ClockCreatureUpdate)) {
    Log("SKIP hud: the clock - no creature updates");
    return;
  }
  Log("hud: the clock over the compass (!clock on / off)%s", g_app_manager ? "" : " - NO app manager: it never shows");
}
