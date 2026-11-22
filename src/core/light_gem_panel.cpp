// The light gem (rules/light_gem.h; #9 a; owner, 2026-11-15: "Sure, let's trial a light gem" - the light level where
// the hiding character stands, bright / dim / dark, on the HUD, a trial). One client NUI window, as the HUD clock's
// (clock_panel.cpp):
// - The server's side (a creature update, the single-player character, twice a second): in stealth mode or not
//   (the creature's stealth flag), and its light - the stealth watch's estimate, the one the 2024 light rule uses
//   (local nwn5e_light_level, worked out twice a second while it hides: stealth_watch.cpp) - and the screen's GUI
//   size (every 2 s while shown), handed over when any of it changes. The words: the SRD 5.2.1's terms, from
//   data/text/en.txt (light_gem.*).
// - The client's side (every frame): a transparent window (input passes through) with a draw list - a dark card,
//   the gem (a disc in the light's colour; a glow round it in bright light, a pale ring round it in darkness) and
//   the words - right above the clock, over the compass (bottom right): its card's right edge on the clock card's,
//   4 GUI units over it, so it follows the clock (`!clock right / bottom <n>`). Shown three frames after it opens
//   (NUI opens a window at the screen's centre); closed out of stealth mode, outside a game and with light_gem=0.

#include "light_gem_panel.h"

#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <mutex>
#include <string>

#include "client_ui.h"
#include "clock_panel.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "player_text.h"
#include "rules/hud_clock.h"
#include "rules/light_gem.h"

namespace {

constexpr const char* kWindowId = "n5lightgem";
constexpr ULONGLONG kCheckEveryMs = 500;
constexpr ULONGLONG kGuiEveryMs = 2000;
constexpr int kRevealFrames = 3;
constexpr float kWindowPad = 4.0f;  // the window's edge -> the group (the card), as the clock's
constexpr float kGroupPad = 4.0f;   // the group -> the label (the draw list's origin)
constexpr float kWindowSlack = 2.0f * (kWindowPad + kGroupPad);
constexpr float kGap = 4.0f;        // the gem's card -> the clock's, GUI units
constexpr size_t kClientAppInternal = 0x8;  // the clock's chain to the in-game GUI
constexpr size_t kAppGui = 0x90;
constexpr size_t kGuiQuickbar = 0x60;

// Server -> client.
struct Reading {
  bool on = false;
  rules::GemLight light = rules::GemLight::kNone;
  std::string words;
  float gui_w = 0.0f, gui_h = 0.0f, gui_s = 1.0f;
};
std::mutex g_lock;
Reading g_published;
int g_version = 0;

Reading g_server;  // the server's last reading
ULONGLONG g_next = 0;
ULONGLONG g_next_gui = 0;
uint32_t g_client_player = ee::kObjectInvalid;
int g_logged = 0;

void Publish(const Reading& reading) {
  std::lock_guard<std::mutex> hold(g_lock);
  g_published = reading;
  ++g_version;
}

void GemCreatureUpdate(void* creature) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  // The game's own client: the single-player character (a new one after a load takes over).
  if (g_client_player != id && g_client_player != ee::kObjectInvalid && CreatureById(g_client_player)) return;
  const ULONGLONG now = GetTickCount64();
  if (now < g_next) return;
  g_next = now + kCheckEveryMs;
  if (!engine::AreaOf(creature) || !ClientNuiReady()) return;
  g_client_player = id;
  Reading reading = g_server;
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  reading.light = rules::GemLightOf(engine::GetLocalInt(creature, "nwn5e_light_level"));
  reading.on = rules::LightGemShown(true, hiding, reading.light);
  if (reading.on) {
    reading.words = PlayerText(rules::LightGemTextKey(reading.light));
    if (now >= g_next_gui || reading.gui_w <= 0.0f) {
      g_next_gui = now + kGuiEveryMs;
      RunScriptChunk("object p = OBJECT_SELF;"
                     " SetLocalInt(p, \"nwn5e_gem_gw\", GetPlayerDeviceProperty(p, \"gui_width\"));"
                     " SetLocalInt(p, \"nwn5e_gem_gh\", GetPlayerDeviceProperty(p, \"gui_height\"));"
                     " SetLocalInt(p, \"nwn5e_gem_gs\", GetPlayerDeviceProperty(p, \"gui_scale\"));",
                     id);
      reading.gui_w = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_gem_gw"));
      reading.gui_h = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_gem_gh"));
      const int percent = engine::GetLocalInt(creature, "nwn5e_gem_gs");  // 140 = 1.4
      reading.gui_s = percent >= 50 && percent <= 400 ? percent / 100.0f : 1.0f;
    }
  }
  const Reading& old = g_server;
  const bool changed = reading.on != old.on || reading.light != old.light || reading.words != old.words ||
                       reading.gui_w != old.gui_w || reading.gui_h != old.gui_h || reading.gui_s != old.gui_s;
  if (!changed) return;
  if (g_logged < 40 && (reading.on != old.on || reading.light != old.light)) {
    ++g_logged;
    if (reading.on) {
      Log("hud: the light gem shows %s (0x%08X; window %.0f x %.0f, GUI scale %.2f)%s", reading.words.c_str(), id,
          reading.gui_w, reading.gui_h, reading.gui_s, g_logged == 40 ? " - further changes not logged" : "");
    } else {
      Log("hud: the light gem hidden (0x%08X %s)%s", id, hiding ? "hides, its light not worked out yet" : "not hiding",
          g_logged == 40 ? " - further changes not logged" : "");
    }
  }
  g_server = reading;
  Publish(reading);
}

// --- The client's side ---------------------------------------------------------------------------

struct Window {
  int token = 0;
  int frames = 0;
  bool revealed = false;
  std::string sent_words;
  int sent_light = -1;
  float sent_x = -1.0e6f, sent_y = -1.0e6f;
};
Window g_window;
int g_seen_version = 0;
Reading g_reading;
void** g_app_manager = nullptr;
int g_open_logs = 0;

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

constexpr float kGemX = 10.0f;  // the gem's centre in the content (GUI units)
constexpr float kGemY = 9.0f;

std::string GemLayout() {
  const float w = rules::kLightGemWidth, h = rules::kLightGemHeight;
  // The card: the group (the label sits kGroupPad in from it), as the clock's.
  std::string items = Shape(7, "on", Colour(0, 0, 0, 150), true, 1.0f, -kGroupPad, -kGroupPad,
                            w + 2.0f * kGroupPad, h + 2.0f * kGroupPad);
  items += "," + Shape(7, "on", Colour(150, 160, 170, 160), false, 1.0f, -kGroupPad, -kGroupPad,
                       w + 2.0f * kGroupPad, h + 2.0f * kGroupPad);
  // The glow round it in bright light, the gem in the light's colour, a pale ring round it in darkness.
  items += "," + Circle("glow", Colour(255, 226, 140, 90), true, kGemX, kGemY, 8.6f);
  items += "," + Circle("on", Bind("gem_c"), true, kGemX, kGemY, 5.6f);
  items += "," + Circle("ring", Colour(150, 160, 190, 210), false, kGemX, kGemY, 6.4f);
  // The words.
  items += ",{\"type\":4,\"enabled\":{\"bind\":\"on\"},\"fill\":null,\"line_thickness\":null,\"order\":1,\"render\":0,"
           "\"arrayBinds\":false,\"color\":" + Colour(240, 240, 240, 255) + ",\"rect\":" + Rect(22.0f, 0.0f, w - 22.0f, h) +
           ",\"text\":" + Bind("t") + ",\"font\":\"\"}";
  char size[96];
  sprintf_s(size, "\"width\":%.1f,\"height\":%.1f,", w, h);
  const std::string element = std::string("{\"type\":\"label\",\"label\":null,\"value\":\"\",") + size +
                              "\"draw_list\":[" + items + "],\"draw_list_scissor\":false}";
  const std::string root = "{\"type\":\"group\",\"label\":null,\"value\":null,\"border\":false,\"scrollbars\":0,"
                           "\"children\":[" + element + "]}";
  // The clock's edge_constraint (0 all round: the window may reach the screen's edges).
  return "{\"version\":1,\"title\":false,\"root\":" + root +
         ",\"geometry\":{\"bind\":\"geometry\"},\"resizable\":false,\"collapsed\":false,\"closable\":false,"
         "\"transparent\":true,\"border\":false,\"accepts_input\":false,\"size_constraint\":null,"
         "\"edge_constraint\":{\"x\":0,\"y\":0,\"w\":0,\"h\":0},\"font\":\"\"}";
}

void CloseGem() {
  if (g_window.token) ClientNuiClose(g_window.token);
  g_window = Window();
}

void GemTick() {
  {
    std::lock_guard<std::mutex> hold(g_lock);
    if (g_version != g_seen_version) {
      g_seen_version = g_version;
      g_reading = g_published;
    }
  }
  if (!InGame() || !g_reading.on || g_reading.words.empty() || g_reading.gui_w <= 0.0f || g_reading.gui_h <= 0.0f) {
    if (g_window.token) CloseGem();
    return;
  }
  if (!g_window.token) {
    g_window.token = ClientNuiOpen(kWindowId, GemLayout(), nullptr);
    if (g_open_logs < 6) {
      ++g_open_logs;
      Log("hud: the light gem window %s", g_window.token ? "opened" : "NOT opened");
    }
    if (!g_window.token) return;
    ClientNuiSetJson(g_window.token, "on", "false");
    ClientNuiSetJson(g_window.token, "glow", "false");
    ClientNuiSetJson(g_window.token, "ring", "false");
  }
  const float gem_w = rules::kLightGemWidth + kWindowSlack, gem_h = rules::kLightGemHeight + kWindowSlack;
  const ClockOffsets& clock = ClockOffsetsForTests();  // the clock's spot, as the owner set it
  const rules::HudBox spot = rules::PlaceLightGem(
      g_reading.gui_w, g_reading.gui_h, g_reading.gui_s, gem_w, gem_h, static_cast<float>(clock.from_right),
      static_cast<float>(clock.from_bottom), rules::kClockWidth + kWindowSlack, rules::kClockHeight + kWindowSlack,
      kWindowPad, kGap, kWindowPad);
  if (fabsf(spot.x - g_window.sent_x) > 0.4f || fabsf(spot.y - g_window.sent_y) > 0.4f) {
    ClientNuiSetJson(g_window.token, "geometry", Rect(spot.x, spot.y, spot.w, spot.h));
    g_window.sent_x = spot.x;
    g_window.sent_y = spot.y;
  }
  if (g_reading.words != g_window.sent_words) {
    ClientNuiSetText(g_window.token, "t", g_reading.words);
    g_window.sent_words = g_reading.words;
  }
  if (!g_window.revealed) {
    if (++g_window.frames < kRevealFrames) return;
    g_window.revealed = true;
    ClientNuiSetJson(g_window.token, "on", "true");
    g_window.sent_light = -1;
  }
  const int light = static_cast<int>(g_reading.light);
  if (light == g_window.sent_light) return;
  const rules::GemColour c = rules::LightGemColour(g_reading.light);
  ClientNuiSetJson(g_window.token, "gem_c", Colour(c.r, c.g, c.b, c.a));
  ClientNuiSetJson(g_window.token, "glow", g_reading.light == rules::GemLight::kBright ? "true" : "false");
  ClientNuiSetJson(g_window.token, "ring", g_reading.light == rules::GemLight::kDark ? "true" : "false");
  g_window.sent_light = light;
}

}  // namespace

void InstallLightGem() {
  // #9 a's launch line: its key, and why it doesn't show when it can't.
  if (!g_config.light_gem) {
    Log("hud: the light gem off (light_gem=0)");
    return;
  }
  if (!g_config.stealth_light || g_config.perception != rules::RuleVariant::kFaithful) {
    Log("hud: the light gem - light_gem=1 but off: no light worked out (stealth_light=%d, perception %s)",
        g_config.stealth_light ? 1 : 0, g_config.perception == rules::RuleVariant::kFaithful ? "faithful" : "not faithful");
    return;
  }
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  AddClientTickListener(&GemTick);
  if (!AddCreatureUpdateListener(&GemCreatureUpdate)) {
    Log("SKIP hud: the light gem - no creature updates");
    return;
  }
  Log("hud: the light gem (a trial) - light_gem=1: while you're in stealth mode, the light you stand in (the SRD's "
      "Bright Light / Dim Light / Darkness: light_gem.*) over the clock (bottom right), twice a second%s",
      g_app_manager ? "" : " - NO app manager: it never shows");
}
