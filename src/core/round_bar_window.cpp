// The round bar (#127; docs/PLAN.md "Round bar"; the owner, 2026-11-03: "sometimes it's hard to figure out when a
// round ends and the next one begins, is there something that could be done here? An onscreen element, like a small
// loading bar that would show the state of the current round?", then "go with A"): a thin card under the action
// queue strip (rules::RoundBarTop; the resource panel goes under it when it's under the strip), shown in a fight -
// - the player character's combat round filling (rules::RoundFill: CNWSCombatRound's Timer +0x2F0C over its
//   RoundLength +0x2F10), gold an attack round, blue a cast round (SpellCastRound +0x2F08), dimmed while paused
//   (+0x2F1C - each swing pauses it), empty between rounds;
// - its swings as ticks (rules::RoundSwingMarks: the schedule's attack entries, +0x2F70, type 1 at +0x10, their due
//   time at +0, taken as the round starts), dark once made (fewer left in the schedule);
// - three pips - the Action (a round of its own running: taken), the Bonus Action (bonus_actions.cpp: free, spent,
//   or one waiting), the Reaction (reactions.h: ready or spent) - while the actions are faithful.
// The server's side samples the single-player character each update and publishes it (a lock); the client's draws
// a client window every frame, the fill moving between samples while the round runs. `!roundbar` (or Combat
// configuration's "Round bar") turns it off and on - the record nwn5e_roundbar_off. Both modes: it shows, it doesn't
// rule.
// 0.66.1 (owner: "the bar pauses for a split second, then the vfx plays, then the animation plays" - a flourish's
// 3.5 s pause came at a fresh round's start, its bar empty: dimmed and still, it looked like nothing): while the
// round is paused the whole track turns violet, so a pause is plain whatever the fill.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#include "bonus_actions.h"
#include "chat.h"
#include "client_ui.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "reactions.h"
#include "records.h"
#include "rules/round_bar.h"

bool ActionQueueStripOff(void* creature);  // action_queue.cpp (!queue)

namespace {

constexpr const char* kWindowId = "n5round";
constexpr const char* kVarOff = "nwn5e_roundbar_off";
constexpr size_t kRoundStarted = 0x2f04;  // CNWSCombatRound (#51's recon)
constexpr size_t kRoundCastRound = 0x2f08;
constexpr size_t kRoundTimer = 0x2f0c;
constexpr size_t kRoundLength = 0x2f10;
constexpr size_t kRoundPaused = 0x2f1c;
constexpr size_t kRoundActionList = 0x2f70;  // -> list wrapper -> CExoLinkedListInternal* (head +0)
constexpr size_t kNodeNext = 0x8;
constexpr size_t kNodeData = 0x10;
constexpr size_t kActionDue = 0x0;   // int32: when it's due in the round (ms)
constexpr size_t kActionType = 0x10;  // byte: 1 an attack
constexpr uint8_t kActionAttack = 1;
constexpr ULONGLONG kGuiEveryMs = 3000;
constexpr int kRevealFrames = 3;  // a new client window takes its geometry a frame or two late

// Layout (GUI units; the draw list's origin the label's top-left).
constexpr float kPipY = 6.0f, kPip = 10.0f;
constexpr float kPipX[3] = {4.0f, 36.0f, 68.0f};
constexpr float kBarX = 100.0f, kBarY = 7.0f, kBarW = 192.0f, kBarH = 8.0f;
constexpr float kPad = 4.0f;  // the window's and the group's

struct Sample {
  bool valid = false;
  bool shown = false;
  bool running = false;
  bool cast = false;
  bool paused = false;
  long long timer = 0, length = 0;
  ULONGLONG at = 0;
  std::vector<float> marks;
  int done = 0;
  bool pips = false;
  rules::RoundPip action = rules::RoundPip::kFree, bonus = rules::RoundPip::kFree, reaction = rules::RoundPip::kFree;
  float gui_w = 0.0f, gui_h = 0.0f, gui_s = 1.0f;
  bool strip_on = true;
};
std::mutex g_lock;
Sample g_published;  // g_lock

// The server's side.
uint32_t g_client_player = ee::kObjectInvalid;
long long g_last_timer = -1;
bool g_last_running = false;
int g_last_left = 0;  // the round's swings to come at the last sample
std::vector<long long> g_snapshot;  // the round's swings' due times, as it started
ULONGLONG g_next_gui = 0;
float g_gui_w = 0.0f, g_gui_h = 0.0f, g_gui_s = 1.0f;

uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }
bool Off(void* creature) { return creature && engine::GetLocalInt(creature, kVarOff) != 0; }

std::vector<long long> AttackDues(void* round) {
  std::vector<long long> dues;
  void* wrapper = Field<void*>(round, kRoundActionList);
  void* list = wrapper ? *static_cast<void**>(wrapper) : nullptr;
  void* node = list ? *static_cast<void**>(list) : nullptr;
  for (int guard = 0; node && guard < 64; ++guard, node = Field<void*>(node, kNodeNext)) {
    const void* action = Field<void*>(node, kNodeData);
    if (action && Field<uint8_t>(action, kActionType) == kActionAttack) dues.push_back(Field<int32_t>(action, kActionDue));
  }
  return dues;
}

void RoundBarUpdate(void* creature) {
  if (!creature || !ClientNuiReady()) return;
  const uint32_t id = IdOf(creature);
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature) || !ReadCreatureFacts(stats).is_player) return;
  // The game's own client: the single-player character (a new one after a load takes over).
  if (g_client_player != id && g_client_player != ee::kObjectInvalid && CreatureById(g_client_player)) return;
  g_client_player = id;
  EnsureRecords(creature);
  const ULONGLONG now = GetTickCount64();
  if (now >= g_next_gui) {
    g_next_gui = now + kGuiEveryMs;
    RunScriptChunk("object p = OBJECT_SELF; SetLocalInt(p, \"nwn5e_rb_gw\", GetPlayerDeviceProperty(p, \"gui_width\"));"
                   " SetLocalInt(p, \"nwn5e_rb_gh\", GetPlayerDeviceProperty(p, \"gui_height\"));"
                   " SetLocalInt(p, \"nwn5e_rb_gs\", GetPlayerDeviceProperty(p, \"gui_scale\"));",
                   id);
    g_gui_w = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_rb_gw"));
    g_gui_h = static_cast<float>(engine::GetLocalInt(creature, "nwn5e_rb_gh"));
    const int percent = engine::GetLocalInt(creature, "nwn5e_rb_gs");
    g_gui_s = percent >= 50 && percent <= 400 ? percent / 100.0f : 1.0f;
  }
  Sample s;
  s.valid = true;
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  s.running = round && Field<int32_t>(round, kRoundStarted) != 0;
  if (round) {
    s.cast = Field<int32_t>(round, kRoundCastRound) != 0;
    s.paused = Field<int32_t>(round, kRoundPaused) != 0;
    s.timer = Field<int32_t>(round, kRoundTimer);
    s.length = Field<int32_t>(round, kRoundLength);
  }
  const std::vector<long long> pending = s.running ? AttackDues(round) : std::vector<long long>();
  if (s.running && (!g_last_running || s.timer < g_last_timer)) {
    g_snapshot = pending;  // a new round
  } else if (s.running && static_cast<int>(pending.size()) > g_last_left) {
    // #53 (0.66.2): swings added to the running round (Action Surge) - the ticks made so far, then those to come.
    const size_t made = static_cast<size_t>(std::max(0, static_cast<int>(g_snapshot.size()) - g_last_left));
    if (g_snapshot.size() > made) g_snapshot.resize(made);
    g_snapshot.insert(g_snapshot.end(), pending.begin(), pending.end());
  }
  if (!s.running) g_snapshot.clear();
  g_last_running = s.running;
  g_last_timer = s.timer;
  const int left = static_cast<int>(pending.size());
  g_last_left = left;
  s.marks = rules::RoundSwingMarks(g_snapshot, s.length);
  s.done = static_cast<int>(g_snapshot.size()) - left;
  if (s.done < 0) s.done = 0;
  s.pips = g_config.actions == rules::RuleVariant::kFaithful;
  if (s.pips) {
    s.action = rules::ActionPip(s.running);
    const int bonus = BonusActionStatus(creature);
    s.bonus = rules::BonusPip(bonus == 0, bonus == 2);
    s.reaction = rules::ReactionPip(ReactionReady(creature));
  }
  s.gui_w = g_gui_w;
  s.gui_h = g_gui_h;
  s.gui_s = g_gui_s;
  s.strip_on = !ActionQueueStripOff(creature);
  s.shown = !Off(creature) && Field<int32_t>(creature, ee::kCreatureInCombat) != 0 && g_gui_w > 0.0f;
  s.at = now;
  std::lock_guard<std::mutex> hold(g_lock);
  g_published = std::move(s);
}

// --- The client's side --------------------------------------------------------------------------------------------

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

std::string Bind(const std::string& name) { return "{\"bind\":\"" + name + "\"}"; }

// Draw-list items (nw_inc_nui.nss: 2 circle, 4 text, 7 rect).
std::string Shape(int type, const std::string& colour, bool fill, float thickness, const std::string& rect) {
  char head[160];
  sprintf_s(head, "{\"type\":%d,\"enabled\":{\"bind\":\"on\"},\"fill\":%s,\"line_thickness\":%.1f,\"order\":1,"
            "\"render\":0,\"arrayBinds\":false,\"color\":", type, fill ? "true" : "false", thickness);
  return head + colour + ",\"rect\":" + rect + "}";
}

std::string Text(const std::string& colour, const std::string& rect, const char* text) {
  return "{\"type\":4,\"enabled\":{\"bind\":\"on\"},\"fill\":null,\"line_thickness\":null,\"order\":1,\"render\":0,"
         "\"arrayBinds\":false,\"color\":" + colour + ",\"rect\":" + rect + ",\"text\":\"" + text + "\",\"font\":\"\"}";
}

std::string Layout() {
  const std::string ring = Colour(150, 160, 170, 200);
  std::string items = Shape(7, Colour(0, 0, 0, 150), true, 1.0f,
                            Rect(-kPad, -kPad, rules::kRoundBarWidth + 2.0f * kPad, rules::kRoundBarHeight + 2.0f * kPad));
  static const char* const kLetters[3] = {"A", "B", "R"};
  static const char* const kPipBinds[3] = {"pa", "pb", "pr"};
  for (int i = 0; i < 3; ++i) {
    const std::string box = Rect(kPipX[i], kPipY, kPip, kPip);
    items += "," + Shape(2, Bind(kPipBinds[i]), true, 1.0f, box) + "," + Shape(2, ring, false, 1.5f, box);
    items += "," + Text(Bind("lc"), Rect(kPipX[i] + kPip + 3.0f, 1.0f, 14.0f, 18.0f), kLetters[i]);
  }
  items += "," + Shape(7, Bind("bc"), true, 1.0f, Rect(kBarX, kBarY, kBarW, kBarH));
  items += "," + Shape(7, Bind("fc"), true, 1.0f, Bind("fill"));
  for (int i = 0; i < rules::kRoundBarMaxSwings; ++i) {
    items += "," + Shape(7, Bind("tc" + std::to_string(i)), true, 1.0f, Bind("t" + std::to_string(i)));
  }
  char size[96];
  sprintf_s(size, "\"width\":%.1f,\"height\":%.1f,", rules::kRoundBarWidth, rules::kRoundBarHeight);
  const std::string element = std::string("{\"type\":\"label\",\"label\":null,\"value\":\"\",") + size +
                              "\"draw_list\":[" + items + "],\"draw_list_scissor\":false}";
  const std::string root = "{\"type\":\"group\",\"label\":null,\"value\":null,\"border\":false,\"scrollbars\":0,"
                           "\"children\":[" + element + "]}";
  return "{\"version\":1,\"title\":false,\"root\":" + root +
         ",\"geometry\":{\"bind\":\"geometry\"},\"resizable\":false,\"collapsed\":false,\"closable\":false,"
         "\"transparent\":true,\"border\":false,\"accepts_input\":false,\"size_constraint\":null,"
         "\"edge_constraint\":null,\"font\":\"\"}";
}

struct Window {
  int token = 0;
  int frames = 0;
  std::vector<std::pair<std::string, std::string>> sent;  // bind -> last value
};
Window g_window;

void Set(const std::string& bind, const std::string& value) {
  for (auto& entry : g_window.sent) {
    if (entry.first != bind) continue;
    if (entry.second == value) return;
    entry.second = value;
    ClientNuiSetJson(g_window.token, bind.c_str(), value);
    return;
  }
  g_window.sent.emplace_back(bind, value);
  ClientNuiSetJson(g_window.token, bind.c_str(), value);
}

std::string PipColour(rules::RoundPip pip, bool shown) {
  if (!shown) return Colour(0, 0, 0, 0);
  switch (pip) {
    case rules::RoundPip::kFree: return Colour(110, 220, 110, 255);     // left
    case rules::RoundPip::kWaiting: return Colour(235, 170, 60, 255);   // spent, one waiting
    case rules::RoundPip::kSpent: return Colour(70, 70, 70, 255);       // spent
  }
  return Colour(70, 70, 70, 255);
}

void Close() {
  if (g_window.token) ClientNuiClose(g_window.token);
  g_window = Window();
}

void RoundBarTick() {
  Sample s;
  {
    std::lock_guard<std::mutex> hold(g_lock);
    s = g_published;
  }
  if (!s.valid || !s.shown) {
    if (g_window.token) Close();
    return;
  }
  if (!g_window.token) {
    g_window.token = ClientNuiOpen(kWindowId, Layout(), nullptr);
    if (!g_window.token) return;
  }
  ++g_window.frames;
  const float window_w = rules::kRoundBarWidth + rules::kRoundBarWindowExtra;
  const float window_h = rules::kRoundBarHeight + rules::kRoundBarWindowExtra;
  const float x = (s.gui_w - window_w * s.gui_s) / 2.0f;
  const float y = rules::RoundBarTop(s.strip_on, s.gui_s);
  Set("geometry", Rect(x, y, window_w, window_h));
  const bool revealed = g_window.frames > kRevealFrames;
  Set("on", revealed ? "true" : "false");
  const rules::RoundKind kind = rules::RoundKindOf(s.running, s.cast);
  const long long moving = s.running && !s.paused ? static_cast<long long>(GetTickCount64() - s.at) : 0;
  const float fill = kind == rules::RoundKind::kNone ? 0.0f : rules::RoundFill(s.timer + moving, s.length);
  char width[32];
  sprintf_s(width, "%.0f", kBarW * fill);  // whole units: no bind sent for a change too small to see
  Set("fill", Rect(kBarX, kBarY, static_cast<float>(atof(width)), kBarH));
  const bool held = s.running && s.paused;  // 0.66.1: the round paused - the track says so
  Set("bc", held ? Colour(130, 85, 185, 235) : Colour(70, 70, 70, 210));
  const int alpha = held ? 130 : 255;
  Set("fc", kind == rules::RoundKind::kCast ? Colour(90, 150, 255, alpha) : Colour(230, 185, 60, alpha));
  for (int i = 0; i < rules::kRoundBarMaxSwings; ++i) {
    const std::string n = std::to_string(i);
    if (i < static_cast<int>(s.marks.size())) {
      Set("t" + n, Rect(kBarX + kBarW * s.marks[i] - 1.0f, kBarY - 3.0f, 2.0f, kBarH + 6.0f));
      Set("tc" + n, i < s.done ? Colour(40, 40, 40, 255) : Colour(245, 245, 245, 255));
    } else {
      Set("tc" + n, Colour(0, 0, 0, 0));
    }
  }
  Set("pa", PipColour(s.action, s.pips));
  Set("pb", PipColour(s.bonus, s.pips));
  Set("pr", PipColour(s.reaction, s.pips));
  Set("lc", s.pips ? Colour(225, 225, 225, 255) : Colour(0, 0, 0, 0));
}

std::vector<std::string> RoundBarCommand(void* creature, const std::string&) {
  const bool on = !RoundBarEnabled(creature);
  SetRoundBar(creature, on);
  return {std::string("nwn5e: the round bar is ") +
          (on ? "on - in a fight, under the action queue strip: your round, its swings, and what's left (Action, "
                "Bonus Action, Reaction)."
              : "off (!roundbar or Combat configuration turns it on).")};
}

}  // namespace

bool RoundBarEnabled(void* creature) {
  if (!creature) return false;
  EnsureRecords(creature);
  return !Off(creature);
}

void SetRoundBar(void* creature, bool on) {
  if (!creature) return;
  EnsureRecords(creature);
  SetRecord(creature, kVarOff, on ? 0 : 1);
  Log("round bar: 0x%08X %s", IdOf(creature), on ? "on" : "off");
}

void InstallRoundBar() {
  RegisterRecords({kVarOff});
  AddChatCommand("!roundbar", &RoundBarCommand);
  const bool updates = AddCreatureUpdateListener(&RoundBarUpdate);
  AddClientTickListener(&RoundBarTick);
  Log("round bar: %s", updates ? "under the action queue strip, in a fight (!roundbar turns it off and on)"
                               : "NOT shown (no creature updates)");
}
