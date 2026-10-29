// The resource panel (rules/hud_layout.h; owner, 2026-10-29: "We might need
// some sort of custom hud element to display focus points", then "go with the
// recommendation" - above the quickbar's left end, a pip per point in rows of
// 10 and "left/total", a spent pip fading out; a general class-resources
// panel, the Monk's Focus first with Uncanny Metabolism as a dot; always shown
// while the character has a pool; a hover tooltip with the costs; `!hud` turns
// it off). One client NUI window (client_ui.cpp), as the action queue strip's
// tiles:
// - The server's side (a creature update, the single-player character, every
//   150 ms): the pools the class rules give (AddHudPool), the screen's GUI size
//   (GetPlayerDeviceProperty, every 2 s), handed over when they change.
// - The client's side (every frame): the window built for the pools' shape
//   (rules::LayoutHud: a draw list - a dark card, each pool's name, pips as a
//   filled circle under a ring, the count, the flags' names and dots), rebuilt
//   only when the shape changes (names, totals, flags); points spent or gained
//   recolour the pips - a spent one fades out over 0.45 s, a regained one in.
//   A new window shows nothing for three frames (NUI opens it at the screen's
//   centre until its geometry applies - the strip's lesson), then appears in
//   place. Not in a game (no in-game GUI - the main menu): closed.
// - Where (read from 89.8193.37): the quickbar is 800 x 36 GUI units, centred
//   on the bottom edge and scaled by the GUI scale s about its bottom centre
//   (CPanelQuickBar::Reposition 0x860010: SetPositionScreen((gui width - 800)
//   / 2, 0); Camera::UpscaleViewport's sceneGUI_PNL_QUICK_BAR branch;
//   CGuiInGame::GetGuiPosition keeps 36 for it). A NUI window's x / y are window
//   pixels, its w / h GUI units (x s). 0.57.1 (owner: "have the resource bar be
//   aligned with the right edge of the quickbar"): the card's right edge on the
//   bar's right edge (0.57.0 started it right of the chat window - EDIT_CHAT_MAIN,
//   720 GUI units from the left edge, over the bar's left part). 0.57.2
//   (owner: "move the box slightly up as it's just a bit over the Talk input
//   box"; "if the game window resolution is ... low enough that the resource
//   panel would overlap the chat window it should be ... placed underneath the
//   action queue panel"): the card's bottom over the Talk line - a band the
//   window's full width, 33-56 GUI units over the bottom (CGuiInGameChatPrompt)
//   - and, where its left edge would reach the chat log (720 GUI units wide,
//   fixed), centred under the action queue strip, below where its leaving
//   tiles slide (rules::PlaceHud).
// - The card: NUI draws with Nuklear - the window's padding 4 and the group's 4
//   (the queue strip's 108 tile left its text 92), the group clipping its
//   children's draw lists - so the label's draw list starts 8 in from the
//   window's edge and the card, 4 past the label on every side, fills the
//   group: 4 in from the window's edge.

#include "resource_panel.h"

#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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

bool ActionQueueStripOff(void* creature);  // action_queue.cpp (!queue)

namespace {

constexpr const char* kVarOff = "nwn5e_hud_off";
constexpr const char* kWindowId = "n5hud";
constexpr ULONGLONG kCheckEveryMs = 150;
constexpr ULONGLONG kGuiEveryMs = 2000;
constexpr int kRevealFrames = 3;
constexpr float kSpendSeconds = 0.45f;  // a spent pip fades out
constexpr float kGainSeconds = 0.25f;   // a regained one fades in
constexpr float kWindowPad = 4.0f;      // the window's edge -> the group (the card's edge)
constexpr float kGroupPad = 4.0f;       // the group -> the label (the draw list's origin)
constexpr float kWindowSlack = 2.0f * (kWindowPad + kGroupPad);
constexpr float kChatWidth = 720.0f;  // EDIT_CHAT_MAIN, from the left edge (GUI units)
constexpr size_t kQuickbarShown = 0xa0;  // CPanelQuickBar: non-null while the bar shows (GetGuiPosition's test)

// The client's in-game GUI (rules_quickbar.cpp's chain): CAppManager +0 CClientExoApp
// +0x8 CClientExoAppInternal +0x90 the in-game GUI +0x60 CPanelQuickBar.
constexpr size_t kClientAppInternal = 0x8;
constexpr size_t kAppGui = 0x90;
constexpr size_t kGuiQuickbar = 0x60;

std::vector<HudPoolProvider> g_providers;

// Server -> client.
std::mutex g_lock;
std::vector<rules::HudPool> g_published;
float g_published_gui_w = 0.0f;
float g_published_gui_h = 0.0f;
float g_published_gui_s = 1.0f;
bool g_published_strip_on = true;
int g_version = 0;

// The server's side.
struct PlayerState {
  std::string signature;
  std::string shape;
  ULONGLONG next = 0;
  ULONGLONG next_gui = 0;
  float gui_w = 0.0f;
  float gui_h = 0.0f;
  float gui_s = 1.0f;
  bool strip_on = true;
};
std::unordered_map<uint32_t, PlayerState> g_players;
uint32_t g_client_player = ee::kObjectInvalid;

uint32_t IdOf(const void* creature) { return creature ? Field<uint32_t>(creature, ee::kObjectId) : ee::kObjectInvalid; }

bool Off(void* creature) { return engine::GetLocalInt(creature, kVarOff) != 0; }

std::string Signature(const std::vector<rules::HudPool>& pools, float gui_w, float gui_h, float gui_s,
                      bool strip_on) {
  std::string s = rules::HudShape(pools);
  for (const rules::HudPool& pool : pools) {
    s += "|" + std::to_string(pool.left);
    for (const rules::HudFlag& flag : pool.flags) s += flag.ready ? "1" : "0";
    s += pool.tooltip;
  }
  char size[64];
  sprintf_s(size, "|%.0fx%.0fx%.2f%s", gui_w, gui_h, gui_s, strip_on ? "" : "|no strip");
  return s + size;
}

void PanelCreatureUpdate(void* creature) {
  const uint32_t id = IdOf(creature);
  if (id == ee::kObjectInvalid || g_providers.empty()) return;
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
  std::vector<rules::HudPool> pools;
  if (!Off(creature)) {
    for (HudPoolProvider provider : g_providers) {
      rules::HudPool pool;
      if (provider(creature, &pool)) pools.push_back(pool);
    }
  }
  if (!pools.empty() && now >= state.next_gui) {
    state.next_gui = now + kGuiEveryMs;
    RunScriptChunk("object p = OBJECT_SELF; SetLocalInt(p, \"nwn5e_hud_gw\", GetPlayerDeviceProperty(p, \"gui_width\"));"
                   " SetLocalInt(p, \"nwn5e_hud_gh\", GetPlayerDeviceProperty(p, \"gui_height\"));"
                   " SetLocalInt(p, \"nwn5e_hud_gs\", GetPlayerDeviceProperty(p, \"gui_scale\"));",
                   id);
    state.gui_w = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_hud_gw"));
    state.gui_h = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_hud_gh"));
    const int percent = engine::GetLocalInt(creature, "nwn5e_hud_gs");  // 140 = 1.4
    state.gui_s = percent >= 50 && percent <= 400 ? percent / 100.0f : 1.0f;
  }
  state.strip_on = !ActionQueueStripOff(creature);
  const std::string signature = Signature(pools, state.gui_w, state.gui_h, state.gui_s, state.strip_on);
  if (signature == state.signature) return;
  const std::string shape = rules::HudShape(pools);
  static int logged = 0;
  if (shape != state.shape && logged < 8) {
    ++logged;
    Log("hud: 0x%08X the resource panel: %s (window %.0f x %.0f, GUI scale %.2f)", id,
        pools.empty() ? "none" : shape.c_str(), state.gui_w, state.gui_h, state.gui_s);
  }
  state.shape = shape;
  state.signature = signature;
  std::lock_guard<std::mutex> hold(g_lock);
  g_published = std::move(pools);
  g_published_gui_w = state.gui_w;
  g_published_gui_h = state.gui_h;
  g_published_gui_s = state.gui_s;
  g_published_strip_on = state.strip_on;
  ++g_version;
}

// --- The client's side -------------------------------------------------------------------

struct Panel {
  int token = 0;
  std::string shape;
  rules::HudLayout layout;
  std::vector<rules::HudPool> pools;
  std::vector<std::vector<float>> alpha;     // each pip's fill, 0..1
  std::vector<std::vector<int>> sent_alpha;  // as last sent, 0..255
  std::vector<std::string> sent_count;
  std::vector<std::vector<int>> sent_flag;   // -1 unsent, 0 used, 1 ready
  std::string sent_tip;
  int frames = 0;
  bool revealed = false;
  float sent_x = -1.0e6f, sent_y = -1.0e6f;
};
Panel g_panel;
int g_seen_version = 0;
float g_gui_w = 0.0f;
float g_gui_h = 0.0f;
float g_gui_s = 1.0f;
bool g_strip_on = true;
LARGE_INTEGER g_last_frame{};
void** g_app_manager = nullptr;
bool g_in_game_logged = false;
int g_bar_shown = -1;  // the quickbar's +0xa0 as last logged (diagnostic: hide with it later?)
int g_bar_logs = 0;

void* InGameQuickbar() {
  if (!g_app_manager || !*g_app_manager) return nullptr;
  void* client = *static_cast<void**>(*g_app_manager);
  void* app = client ? Field<void*>(client, kClientAppInternal) : nullptr;
  void* gui = app ? Field<void*>(app, kAppGui) : nullptr;
  return gui ? Field<void*>(gui, kGuiQuickbar) : nullptr;
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

std::string Quoted(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    if (static_cast<unsigned char>(c) >= 0x20) out += c;
  }
  return out + "\"";
}

std::string Bind(const std::string& name) { return "{\"bind\":\"" + name + "\"}"; }

std::string PipBind(int pool, int pip) { return "p" + std::to_string(pool) + "_" + std::to_string(pip); }
std::string FlagBind(const char* part, int pool, int flag) {
  return std::string(part) + std::to_string(pool) + "_" + std::to_string(flag);
}

// Draw-list items (nw_inc_nui.nss: NuiDrawListItem; 2 circle, 4 text, 7 rect).
std::string Shape(int type, const std::string& colour, bool fill, float thickness, const rules::HudBox& box) {
  char head[160];
  sprintf_s(head, "{\"type\":%d,\"enabled\":{\"bind\":\"on\"},\"fill\":%s,\"line_thickness\":%.1f,\"order\":1,"
            "\"render\":0,\"arrayBinds\":false,\"color\":", type, fill ? "true" : "false", thickness);
  return head + colour + ",\"rect\":" + Rect(box.x, box.y, box.w, box.h) + "}";
}

std::string Text(const std::string& colour, const rules::HudBox& box, const std::string& text) {
  return "{\"type\":4,\"enabled\":{\"bind\":\"on\"},\"fill\":null,\"line_thickness\":null,\"order\":1,\"render\":0,"
         "\"arrayBinds\":false,\"color\":" + colour + ",\"rect\":" + Rect(box.x, box.y, box.w, box.h) +
         ",\"text\":" + text + ",\"font\":\"\"}";
}

const std::string kNameColour = Colour(225, 225, 225, 255);
const std::string kCountColour = Colour(255, 255, 255, 255);
const std::string kRingColour = Colour(150, 160, 170, 200);

std::string PanelLayout(const rules::HudLayout& layout, const std::vector<rules::HudPool>& pools) {
  std::string items = Shape(7, Colour(0, 0, 0, 150), true, 1.0f,
                            {-kGroupPad, -kGroupPad, layout.width + 2.0f * kGroupPad, layout.height + 2.0f * kGroupPad});
  for (size_t k = 0; k < layout.pools.size(); ++k) {
    const rules::HudPoolLayout& p = layout.pools[k];
    const int pool = static_cast<int>(k);
    items += "," + Text(kNameColour, p.name, Quoted(pools[k].name));
    for (size_t i = 0; i < p.pips.size(); ++i) {
      items += "," + Shape(2, Bind(PipBind(pool, static_cast<int>(i))), true, 1.0f, p.pips[i]);
      items += "," + Shape(2, kRingColour, false, 1.5f, p.pips[i]);
    }
    items += "," + Text(kCountColour, p.count, Bind("c" + std::to_string(pool)));
    for (size_t j = 0; j < p.flag_names.size(); ++j) {
      const int flag = static_cast<int>(j);
      items += "," + Text(Bind(FlagBind("fc", pool, flag)), p.flag_names[j], Quoted(pools[k].flags[j].name));
      items += "," + Shape(2, Bind(FlagBind("fd", pool, flag)), true, 1.0f, p.flag_dots[j]);
      items += "," + Shape(2, kRingColour, false, 1.5f, p.flag_dots[j]);
    }
  }
  char size[96];
  sprintf_s(size, "\"width\":%.1f,\"height\":%.1f,", layout.width, layout.height);
  const std::string element = std::string("{\"type\":\"label\",\"label\":null,\"value\":\"\",") + size +
                              "\"tooltip\":{\"bind\":\"tip\"},\"draw_list\":[" + items + "],\"draw_list_scissor\":false}";
  const std::string root = "{\"type\":\"group\",\"label\":null,\"value\":null,\"border\":false,\"scrollbars\":0,"
                           "\"children\":[" + element + "]}";
  return "{\"version\":1,\"title\":false,\"root\":" + root +
         ",\"geometry\":{\"bind\":\"geometry\"},\"resizable\":false,\"collapsed\":false,\"closable\":false,"
         "\"transparent\":true,\"border\":false,\"accepts_input\":true,\"size_constraint\":null,"
         "\"edge_constraint\":null,\"font\":\"\"}";
}

// Where the panel's window goes (window pixels): the card where rules::PlaceHud
// puts it (on the quickbar's right edge over the Talk line, or under the action
// queue strip when that would be over the chat), the window kWindowPad around it.
int g_place_logged = 0;
int g_place_logs = 0;

void PanelPosition(float window_w, float window_h, float* x, float* y) {
  rules::HudScreen screen;
  screen.width = g_gui_w;
  screen.height = g_gui_h;
  screen.scale = g_gui_s;
  screen.chat_right = kChatWidth * g_gui_s;
  screen.strip_on = g_strip_on;
  const rules::HudSpot spot = rules::PlaceHud(screen, window_w - 2.0f * kWindowPad, window_h - 2.0f * kWindowPad);
  *x = spot.x - kWindowPad * g_gui_s;
  *y = spot.y - kWindowPad * g_gui_s;
  const int place = spot.under_strip ? (g_strip_on ? 1 : 2) : 0;
  if (place != g_place_logged && g_place_logs < 12) {
    ++g_place_logs;
    Log("hud: the resource panel %s (window %.0f x %.0f, GUI scale %.2f)",
        spot.under_strip ? (g_strip_on ? "under the action queue strip - over the quickbar it would cover the chat"
                                       : "at the top (the strip is off) - over the quickbar it would cover the chat")
                         : "over the quickbar's right end",
        g_gui_w, g_gui_h, g_gui_s);
  }
  g_place_logged = place;
}

void ClosePanel() {
  if (g_panel.token) ClientNuiClose(g_panel.token);
  g_panel = Panel();
}

void OpenPanel(const std::vector<rules::HudPool>& pools) {
  if (g_panel.token) ClientNuiClose(g_panel.token);
  g_panel = Panel();
  g_panel.shape = rules::HudShape(pools);
  g_panel.layout = rules::LayoutHud(pools);
  g_panel.token = ClientNuiOpen(kWindowId, PanelLayout(g_panel.layout, pools), nullptr);
  if (!g_panel.token) {
    g_panel = Panel();
    return;
  }
  for (size_t k = 0; k < pools.size(); ++k) {
    const size_t pips = g_panel.layout.pools[k].pips.size();
    std::vector<float> alpha(pips);
    for (size_t i = 0; i < pips; ++i) alpha[i] = static_cast<int>(i) < pools[k].left ? 1.0f : 0.0f;  // no fade at first
    g_panel.alpha.push_back(alpha);
    g_panel.sent_alpha.push_back(std::vector<int>(pips, -1));
    g_panel.sent_count.push_back("");
    g_panel.sent_flag.push_back(std::vector<int>(pools[k].flags.size(), -1));
  }
  ClientNuiSetJson(g_panel.token, "on", "false");
}

void PanelTick() {
  LARGE_INTEGER now, frequency;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&frequency);
  float dt = g_last_frame.QuadPart ? static_cast<float>(now.QuadPart - g_last_frame.QuadPart) / frequency.QuadPart
                                   : 0.0f;
  g_last_frame = now;
  if (dt > 0.1f) dt = 0.1f;
  void* bar = InGameQuickbar();
  const bool in_game = bar != nullptr;
  if (!g_in_game_logged && in_game) {
    g_in_game_logged = true;
    Log("hud: the in-game GUI is up - the resource panel can show");
  }
  const int shown = bar ? (Field<void*>(bar, kQuickbarShown) ? 1 : 0) : -1;
  if (in_game && shown != g_bar_shown && g_bar_logs < 12) {
    ++g_bar_logs;
    Log("hud: the quickbar's +0xa0 %s (the resource panel stays either way)", shown ? "set - the bar shows" : "null");
  }
  if (in_game) g_bar_shown = shown;
  {
    std::lock_guard<std::mutex> hold(g_lock);
    if (g_version != g_seen_version) {
      g_seen_version = g_version;
      g_panel.pools = g_published;
      g_gui_w = g_published_gui_w;
      g_gui_h = g_published_gui_h;
      g_gui_s = g_published_gui_s;
      g_strip_on = g_published_strip_on;
    }
  }
  const std::vector<rules::HudPool> pools = g_panel.pools;
  if (!in_game || pools.empty() || g_gui_w <= 0.0f || g_gui_h <= 0.0f) {
    if (g_panel.token) ClosePanel();
    g_panel.pools = pools;
    return;
  }
  if (!g_panel.token || g_panel.shape != rules::HudShape(pools)) {
    OpenPanel(pools);
    g_panel.pools = pools;
    if (!g_panel.token) return;
  }
  Panel& panel = g_panel;
  float x = 0.0f, y = 0.0f;
  const float window_w = panel.layout.width + kWindowSlack, window_h = panel.layout.height + kWindowSlack;
  PanelPosition(window_w, window_h, &x, &y);
  if (fabsf(x - panel.sent_x) > 0.4f || fabsf(y - panel.sent_y) > 0.4f) {
    ClientNuiSetJson(panel.token, "geometry", Rect(x, y, window_w, window_h));
    panel.sent_x = x;
    panel.sent_y = y;
  }
  if (!panel.revealed && ++panel.frames >= kRevealFrames) {
    panel.revealed = true;
    ClientNuiSetJson(panel.token, "on", "true");
  }
  std::string tip;
  for (size_t k = 0; k < pools.size() && k < panel.alpha.size(); ++k) {
    const rules::HudPool& pool = pools[k];
    const int index = static_cast<int>(k);
    for (size_t i = 0; i < panel.alpha[k].size(); ++i) {
      const float target = static_cast<int>(i) < pool.left ? 1.0f : 0.0f;
      float& alpha = panel.alpha[k][i];
      alpha = rules::HudFade(alpha, target, dt, target > alpha ? kGainSeconds : kSpendSeconds);
      const int a = static_cast<int>(alpha * 255.0f + 0.5f);
      if (a != panel.sent_alpha[k][i]) {
        ClientNuiSetJson(panel.token, PipBind(index, static_cast<int>(i)).c_str(), Colour(110, 200, 255, a));
        panel.sent_alpha[k][i] = a;
      }
    }
    const std::string count = rules::HudCount(pool);
    if (count != panel.sent_count[k]) {
      ClientNuiSetText(panel.token, ("c" + std::to_string(index)).c_str(), count);
      panel.sent_count[k] = count;
    }
    for (size_t j = 0; j < pool.flags.size() && j < panel.sent_flag[k].size(); ++j) {
      const int ready = pool.flags[j].ready ? 1 : 0;
      if (ready == panel.sent_flag[k][j]) continue;
      const int flag = static_cast<int>(j);
      ClientNuiSetJson(panel.token, FlagBind("fc", index, flag).c_str(),
                       ready ? Colour(235, 235, 235, 255) : Colour(140, 140, 140, 255));
      ClientNuiSetJson(panel.token, FlagBind("fd", index, flag).c_str(),
                       ready ? Colour(255, 210, 60, 255) : Colour(0, 0, 0, 0));
      panel.sent_flag[k][j] = ready;
    }
    if (!tip.empty()) tip += "\n\n";
    tip += pool.tooltip;
  }
  if (tip != panel.sent_tip) {
    ClientNuiSetText(panel.token, "tip", tip);
    panel.sent_tip = tip;
  }
}

std::vector<std::string> HudCommand(void* creature, const std::string& /*args*/) {
  const uint32_t id = IdOf(creature);
  EnsureRecords(creature);
  const bool off = !Off(creature);
  SetRecord(creature, kVarOff, off ? 1 : 0);
  g_players[id].signature = "-";  // redrawn (or closed) at the next check
  g_players[id].next = 0;
  Log("hud: 0x%08X the resource panel turned %s", id, off ? "off" : "on");
  return {off ? "nwn5e: the resource panel is off (!hud turns it back on)."
              : "nwn5e: the resource panel is on - your class's points above the quickbar; hover it for what they pay "
                "for (!hud turns it off)."};
}

}  // namespace

void AddHudPool(HudPoolProvider provider) { g_providers.push_back(provider); }

void InstallResourcePanel() {
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  RegisterRecords({kVarOff});
  AddChatCommand("!hud", &HudCommand);
  AddClientTickListener(&PanelTick);
  if (!AddCreatureUpdateListener(&PanelCreatureUpdate)) {
    Log("SKIP hud: the resource panel - no creature updates");
    return;
  }
  Log("hud: the resource panel above the quickbar (%zu pool%s; !hud on / off)%s", g_providers.size(),
      g_providers.size() == 1 ? "" : "s", g_app_manager ? "" : " - NO app manager: it never shows");
}
