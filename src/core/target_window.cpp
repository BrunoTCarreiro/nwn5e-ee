// #171, the target window (owner, 2026-11-18: Q3 B for Scorching Ray, "remember the order in which enemies are
// selected", "can we use right click to remove an assignment?", "I'd rather have a small window that shows the targets
// assigned to each ray and an ok and cancel button. This should also be the case for magic missile ... faithfully they
// should be able to be directed"; the window before the cast: "A"; "for style points, one should also be able to use
// scorching ray to light fire light sources"). The rows' rules: rules/target_rows.h.
//
// A player character's own cast (its input message: action_guard's from_input) of Magic Missile or Scorching Ray is
// held - the order guard refuses it, so nothing is spent - and, on the server's next frame, a window opens: one
// row a dart or ray (3, one more a slot level above the spell's), row 1 the creature it was cast at. EE's targeting
// mode (EnterTargetingMode, the module's OnPlayerTarget event: nwn5e's n5_target, the module's own script chained) picks
// the rest: a creature fills the next empty row (the same creature again is another dart or ray); for Scorching Ray an
// unlit fire light too (a lamp, a brazier, a torch bracket, a tile flame's stand-in: light_sources.cpp's). Each picked
// target glows (VFX_DUR_GLOW_LIGHT_RED, tagged). [x] empties a row; a right-click (the event with no object) takes back
// the latest dart of the target under the cursor, Esc cancels the spell (0.99.99). OK casts it again as it was ordered - ActionCastSpellAtObject at the first row's target
// with its metamagic, its domain level (#36's slot) and its class - and the rows ride with it to the spell's script
// (TakeWindowTargets: an empty row the last target picked's); Cancel (or the window's close) casts nothing. A click on
// the window can end the targeting mode first (the event with no object): a click within kClickAfterCancelMs of one
// undoes its take-back. Henchmen and NPC casters, and scripts' casts, never wait: every dart or ray at their target.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "action_guard.h"
#include "anim_target_counts.h"
#include "client_hover.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "nui.h"
#include "player_text.h"
#include "script_hooks.h"
#include "server_tick.h"
#include "target_window.h"
#include "rules/spell_slots.h"
#include "rules/spells2024.h"
#include "rules/target_rows.h"

namespace {

constexpr const char* kWindow = "n5tw";
constexpr const char* kTargetScript = "n5_target";  // the module's OnPlayerTarget while a window has been opened
constexpr ULONGLONG kSentForMs = 60000;             // OK to the spell's script: the cast must run within this
constexpr ULONGLONG kClickAfterCancelMs = 600;      // a window click this soon after a cancel event undoes it
constexpr ULONGLONG kQuietMs = 1000;                // a closed window's leftover targeting events, swallowed
constexpr ULONGLONG kEscapeWithinMs = 400;          // a targeting cancel this soon after Esc was down is Esc's
constexpr ULONGLONG kHoverLeftWithinMs = 250;       // a right-click's target: under the cursor, or just left
constexpr int kObjectTypeCreature = 1;              // NWScript OBJECT_TYPE_CREATURE
constexpr int kObjectTypePlaceable = 64;            // NWScript OBJECT_TYPE_PLACEABLE

struct Held {
  SpellOrder order;
  rules::TargetRows rows;
  std::vector<uint32_t> marked;  // the targets glowing
  std::vector<rules::TargetCount> counted;  // the count markers on (anim_target_counts.h)
  int undo_row = -1;             // the last take-back by a cancel event, for a window click right after it
  uint32_t undo_id = 0;
  ULONGLONG undo_at = 0;
  bool open = false;
};
std::unordered_map<uint32_t, Held> g_held;  // by player character

struct Sent {
  int spell = 0;
  std::vector<uint32_t> targets;
  ULONGLONG at = 0;
};
std::unordered_map<uint32_t, Sent> g_sent;          // OK'd, waiting for the cast's script
std::unordered_map<uint32_t, int> g_events;         // each player's targeting events seen (its n5tw_ev)
std::unordered_map<uint32_t, ULONGLONG> g_quiet;    // a window just closed: its targeting events swallowed until
bool g_from_input = false;                          // the order being guarded came from a player's input
bool g_module_event = false;                        // n5_target set as the module's OnPlayerTarget

// Esc and a right-click both end EE's targeting mode as the same event (no object). In single-player the game's window
// is this process's: Esc held while it's in front, noted each server frame and at the event, tells them apart.
ULONGLONG g_escape_at = 0;

void NoteEscape() {
  if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0) return;
  DWORD process = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &process);
  if (process == GetCurrentProcessId()) g_escape_at = GetTickCount64();
}

bool EscapeRecently() {
  NoteEscape();
  return g_escape_at != 0 && GetTickCount64() - g_escape_at <= kEscapeWithinMs;
}

std::string Hex(uint32_t id) {
  char text[16];
  snprintf(text, sizeof text, "%x", id);
  return text;
}

const char* SpellName(int spell) {
  const rules::Spell2024* found = rules::Spell2024Of(spell);
  return found ? found->name : "";
}

std::string NameOf(uint32_t player, uint32_t id) {
  void* pc = CreatureById(player);
  if (!pc) return "";
  RunScriptChunk(("SetLocalString(OBJECT_SELF, \"n5tw_n\", GetName(StringToObject(\"" + Hex(id) + "\")));").c_str(),
                 player);
  return engine::GetLocalString(pc, "n5tw_n");
}

// The module's OnPlayerTarget: ours, the module's own kept on it (n5tw_chain) and run for any other targeting.
void SetModuleEvent(uint32_t player) {
  if (g_module_event) return;
  RunScriptChunk("object m = GetModule(); string s = GetEventScript(m, EVENT_SCRIPT_MODULE_ON_PLAYER_TARGET);"
                 "if (s != \"n5_target\") { SetLocalString(m, \"n5tw_chain\", s);"
                 " SetEventScript(m, EVENT_SCRIPT_MODULE_ON_PLAYER_TARGET, \"n5_target\"); }",
                 player);
  g_module_event = true;
}

// The cursor asked for a moment after (kEnterAfterMs, wall-clock), from the server's frame (server_tick.h) - as
// 0.99.85's DelayCommand(0.05) did, which worked unpaused, but a paused game runs no DelayCommand. 0.99.87 (owner,
// 2026-11-19, on 0.99.86's ask at once, inside the order's own handling: "now unpaused also only allows to pick one
// target"): never at once - the client was still ending its own targeting and dropped it.
constexpr ULONGLONG kEnterAfterMs = 60;
struct EnterAsk {
  int spell = 0;
  ULONGLONG due = 0;
};
std::unordered_map<uint32_t, EnterAsk> g_enter;  // by player character

void EnterTargeting(uint32_t player, int spell) { g_enter[player] = EnterAsk{spell, GetTickCount64() + kEnterAfterMs}; }

void EnterTargetingNow(uint32_t player, int spell) {
  const std::string types =
      spell == rules::kScorchingRay ? "OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE" : "OBJECT_TYPE_CREATURE";
  RunScriptChunk(("SetEnterTargetingModeData(OBJECT_SELF, SPELL_TARGETING_SHAPE_NONE, 0.0, 0.0, "
                  "SPELL_TARGETING_FLAGS_NONE, 36.6); EnterTargetingMode(OBJECT_SELF, " +
                  types + ", MOUSECURSOR_MAGIC, MOUSECURSOR_NOMAGIC);")
                     .c_str(),
                 player);
}

void StopTargeting(uint32_t player) {
  g_quiet[player] = GetTickCount64() + kQuietMs;
  RunScriptChunk("EnterTargetingMode(OBJECT_SELF, 0);", player);
}

void Mark(uint32_t player, uint32_t id) {
  RunScriptChunk(("object t = StringToObject(\"" + Hex(id) + "\"); if (GetIsObjectValid(t)) ApplyEffectToObject("
                  "DURATION_TYPE_TEMPORARY, TagEffect(SupernaturalEffect(EffectVisualEffect(VFX_DUR_GLOW_LIGHT_RED)), "
                  "\"n5tw_mark\"), t, 300.0);")
                     .c_str(),
                 player);
}

// Removes the effects tagged `tag` (the glow's n5tw_mark, the count marker's n5tw_count) from the target; how many it
// found and removed (0.99.95: TC-1's evidence - the numbers stayed on screen after their take-off).
int RemoveTagged(uint32_t player, uint32_t id, const char* tag) {
  RunScriptChunk(("object t = StringToObject(\"" + Hex(id) + "\"); int n = 0; effect e = GetFirstEffect(t);"
                  "while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"" + std::string(tag) +
                  "\") { RemoveEffect(t, e); n++; } e = GetNextEffect(t); }"
                  "SetLocalInt(OBJECT_SELF, \"n5tw_removed\", n);")
                     .c_str(),
                 player);
  void* pc = CreatureById(player);
  return pc ? engine::GetLocalInt(pc, "n5tw_removed") : -1;
}

void Unmark(uint32_t player, uint32_t id) { RemoveTagged(player, id, "n5tw_mark"); }

// The glow on exactly the targets in the rows.
void SyncMarks(uint32_t player, Held* held) {
  std::vector<uint32_t> want;
  for (uint32_t id : held->rows.rows) {
    if (id != 0 && std::find(want.begin(), want.end(), id) == want.end()) want.push_back(id);
  }
  for (uint32_t id : held->marked) {
    if (std::find(want.begin(), want.end(), id) == want.end()) Unmark(player, id);
  }
  for (uint32_t id : want) {
    if (std::find(held->marked.begin(), held->marked.end(), id) == held->marked.end()) Mark(player, id);
  }
  held->marked = want;
}

void SetRowTexts(uint32_t player, const Held& held) {
  std::vector<std::pair<std::string, std::string>> binds;
  for (size_t i = 0; i < held.rows.rows.size(); ++i) {
    const uint32_t id = held.rows.rows[i];
    const std::string n = std::to_string(i + 1);
    binds.emplace_back("r" + n, id != 0 ? PlayerText("target_window.row", {{"n", n}, {"name", NameOf(player, id)}})
                                        : PlayerText("target_window.row_empty", {{"n", n}}));
  }
  NuiSetTexts(player, kWindow, binds);
}

// Each target's count (owner, 2026-11-19: "it should be a permanently visible indicator on top of each enemy saying how
// many darts/bolts are assigned to them, not how many are left"; rules::CountsByTarget) shows as the animation
// session's numbered marker (anim_target_counts.h, the owner's picks: the red glow kept, a white number on a small
// dark-red disc over the head) - an effect, so it shows paused. 0.99.88-0.99.90 floated it as text instead; 0.99.91
// took that out (owner: "after a while it stops showing up until the spell is cast and then it just streams in all the
// buffered values"): the client holds floating text while the game is paused - the usual way to pick.
// 0.99.100 (the animation session's TF; the owner's per-spell tallies): Magic Missile's blue, Scorching Ray's orange
// (Eldritch Blast's violet waits on the Warlock).
TargetCountStyle StyleOf(uint32_t spell) {
  return spell == static_cast<uint32_t>(rules::kScorchingRay) ? TargetCountStyle::ScorchingRay
                                                               : TargetCountStyle::MagicMissile;
}

void SyncCounts(uint32_t player, Held* held, const std::vector<rules::TargetCount>& now) {
  const rules::CountMarkerChanges changes = rules::CountMarkerChangesFor(held->counted, now);
  std::string off;
  for (uint32_t id : changes.remove) {
    const int removed = RemoveTagged(player, id, "n5tw_count");
    off += (off.empty() ? "" : ", ") + Hex(id) + " (" + std::to_string(removed) + " effect" +
           (removed == 1 ? "" : "s") + " removed)";
  }
  for (const rules::TargetCount& one : changes.apply) {
    const int row = TargetCountMarkerVfx(StyleOf(held->order.spell), one.count);  // -1 past 11: the glow alone
    if (row < 0) continue;
    RunScriptChunk(("object t = StringToObject(\"" + Hex(one.id) + "\"); if (GetIsObjectValid(t)) ApplyEffectToObject("
                    "DURATION_TYPE_TEMPORARY, TagEffect(SupernaturalEffect(EffectVisualEffect(" +
                    std::to_string(row) + ")), \"n5tw_count\"), t, 300.0);")
                       .c_str(),
                   player);
  }
  held->counted = now;
  static int logged = 0;  // 0.99.95: every sync, up to 200 a session (TC-1's evidence; 5 hid the later ones)
  if (logged < 200 && (!changes.remove.empty() || !changes.apply.empty())) {
    ++logged;
    std::string said;
    for (const rules::TargetCount& one : now) {
      said += (said.empty() ? "" : ", ") + Hex(one.id) + " " + std::to_string(one.count);
    }
    Log("target window: count markers for 0x%08X - %s (%zu put on, %zu taken off%s%s)", player,
        said.empty() ? "none" : said.c_str(), changes.apply.size(), changes.remove.size(), off.empty() ? "" : ": ",
        off.c_str());
  }
}

void Refresh(uint32_t player, Held* held) {
  SetRowTexts(player, *held);
  SyncMarks(player, held);
  SyncCounts(player, held, rules::CountsByTarget(held->rows));
}

std::string Layout(const Held& held) {
  const bool rays = held.order.spell == rules::kScorchingRay;
  std::string children;
  for (size_t i = 0; i < held.rows.rows.size(); ++i) {
    const std::string n = std::to_string(i + 1);
    const std::string bind = "r" + n;
    const std::string clear = "x" + n;
    children += nui::Row(nui::List({nui::Label(nui::Bind(bind.c_str()), 26.0f),
                                    nui::Button(clear.c_str(), nui::Str(PlayerText("target_window.clear")), 32.0f)})) +
                ",";
  }
  children += nui::Text(nui::Str(PlayerText(rays ? "target_window.help_rays" : "target_window.help_darts")), 64.0f);
  children += "," + nui::Row(nui::List({nui::Button("ok", nui::Str(PlayerText("target_window.ok")), 110.0f),
                                        nui::Spacer(),
                                        nui::Button("cancel", nui::Str(PlayerText("target_window.cancel")), 110.0f)}));
  const std::string title = PlayerText(rays ? "target_window.title_rays" : "target_window.title_darts",
                                       {{"spell", SpellName(held.order.spell)},
                                        {"count", std::to_string(held.rows.rows.size())}});
  const float height = 110.0f + 34.0f * static_cast<float>(held.rows.rows.size()) + 120.0f;
  return nui::Window(nui::Col(children), nui::Str(title), 60.0f, 120.0f, 400.0f, height);
}

void Reissue(uint32_t player, const SpellOrder& order, uint32_t target) {
  char code[512];
  snprintf(code, sizeof code,
           "ActionCastSpellAtObject(%u, StringToObject(\"%s\"), %d, FALSE, %d, PROJECTILE_PATH_TYPE_DEFAULT, FALSE, "
           "GetClassByPosition(%d, OBJECT_SELF), FALSE);",
           order.spell, Hex(target).c_str(), order.metamagic, order.domain_level, order.class_slot + 1);
  RunScriptChunk(code, player);
}

void Close(uint32_t player, Held* held) {
  for (uint32_t id : held->marked) Unmark(player, id);
  held->marked.clear();
  SyncCounts(player, held, {});  // every count marker off
  g_enter.erase(player);
  NuiClose(player, kWindow);
  StopTargeting(player);
}

void Fire(uint32_t player) {
  const auto found = g_held.find(player);
  if (found == g_held.end()) return;
  Held held = found->second;
  g_held.erase(found);
  Close(player, &held);
  const std::vector<uint32_t> targets = rules::ResolveTargets(held.rows);
  if (targets.empty()) {
    TellPlayer(player, PlayerText("target_window.no_target", {{"spell", SpellName(held.order.spell)}}));
    Log("target window: 0x%08X %s - no target, nothing cast", player, SpellName(held.order.spell));
    return;
  }
  g_sent[player] = Sent{static_cast<int>(held.order.spell), targets, GetTickCount64()};
  Reissue(player, held.order, targets[0]);
  std::string list;
  for (uint32_t id : targets) list += " 0x" + Hex(id);
  Log("target window: 0x%08X OK - %s cast again (class slot %d, domain level %d, metamagic %d), %zu at:%s", player,
      SpellName(held.order.spell), held.order.class_slot, held.order.domain_level, held.order.metamagic, targets.size(),
      list.c_str());
}

void Cancel(uint32_t player) {
  const auto found = g_held.find(player);
  if (found == g_held.end()) return;
  Held held = found->second;
  g_held.erase(found);
  Close(player, &held);
  TellPlayer(player, PlayerText("target_window.cancelled", {{"spell", SpellName(held.order.spell)}}));
  Log("target window: 0x%08X %s cancelled - nothing cast", player, SpellName(held.order.spell));
}

void OnWindowEvent(const NuiEvent& event) {
  const auto found = g_held.find(event.player);
  if (found == g_held.end()) return;
  Held& held = found->second;
  if (event.type == "close") {
    Cancel(event.player);
    return;
  }
  if (event.type != "click") return;
  if (held.undo_row >= 0 && GetTickCount64() - held.undo_at < kClickAfterCancelMs) {
    rules::RestoreRow(&held.rows, held.undo_row, held.undo_id);  // the click ended the targeting mode: undone
  }
  held.undo_row = -1;
  if (event.element == "ok") {
    Fire(event.player);
    return;
  }
  if (event.element == "cancel") {
    Cancel(event.player);
    return;
  }
  if (event.element.size() > 1 && event.element[0] == 'x') {
    rules::ClearRow(&held.rows, atoi(event.element.c_str() + 1) - 1);
    Refresh(event.player, &held);
    EnterTargeting(event.player, static_cast<int>(held.order.spell));
  }
}

// Whether `id` is an unlit fire light nwn5e handles (its scripts or a tile flame's stand-in).
bool UnlitLight(uint32_t player, uint32_t id) {
  void* pc = CreatureById(player);
  if (!pc) return false;
  RunScriptChunk(("object o = StringToObject(\"" + Hex(id) + "\");"
                  "int f = GetStringLeft(GetTag(o), 11) == \"nwn5e_flame\";"
                  "int lamp = GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_DAMAGED) == \"n5_lamp_hit\" ||"
                  " GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_USED) == \"n5_lamp_use\";"
                  "int out = f ? GetLocalInt(o, \"nwn5e_torch_out\") : !GetPlaceableIllumination(o);"
                  "SetLocalInt(OBJECT_SELF, \"n5tw_l\", (f || lamp) && out);")
                     .c_str(),
                 player);
  return engine::GetLocalInt(pc, "n5tw_l") != 0;
}

void Pick(uint32_t player, Held* held) {
  void* pc = CreatureById(player);
  if (!pc) return;
  const uint32_t id =
      static_cast<uint32_t>(strtoul(engine::GetLocalString(pc, "n5tw_o").c_str(), nullptr, 16));
  const int spell = static_cast<int>(held->order.spell);
  if (id == 0 || id == ee::kObjectInvalid) {  // the targeting mode ended with no object: Esc or a right-click
    // 0.99.99 (owner, 2026-11-20: "Esc cancels, right-click A"): Esc cancels the spell, nothing spent; a right-click
    // takes back the latest dart of the target under the cursor (client_hover.h), whatever was picked after it.
    if (EscapeRecently()) {
      Log("target window: 0x%08X Esc - the spell cancelled", player);
      Cancel(player);
      return;
    }
    const uint32_t hovered = HoveredObject(kHoverLeftWithinMs);
    const int row = rules::TakeBackTarget(&held->rows, hovered);
    if (row >= 0) {
      held->undo_id = hovered;
      held->undo_row = row;
      held->undo_at = GetTickCount64();
      Refresh(player, held);
    }
    Log("target window: 0x%08X right-click over 0x%x - %s", player, hovered,
        row >= 0 ? ("row " + std::to_string(row + 1) + " taken back").c_str()
                 : (hovered ? "it holds no dart or ray" : "nothing under the cursor"));
    EnterTargeting(player, spell);
    return;
  }
  const int kind = engine::GetLocalInt(pc, "n5tw_k");
  if (engine::GetLocalInt(pc, "n5tw_g") != 0 || (kind != kObjectTypeCreature && kind != kObjectTypePlaceable)) {
    EnterTargeting(player, spell);  // the ground, or nothing a dart or ray takes
    return;
  }
  if (kind == kObjectTypePlaceable && (spell != rules::kScorchingRay || !UnlitLight(player, id))) {
    TellPlayer(player, PlayerText("target_window.not_light", {{"name", NameOf(player, id)}}));
    EnterTargeting(player, spell);
    return;
  }
  if (!rules::AddTarget(&held->rows, id)) {
    TellPlayer(player, PlayerText("target_window.full", {{"spell", SpellName(spell)}}));
  } else {
    Refresh(player, held);
  }
  EnterTargeting(player, spell);
}

// The module's OnPlayerTarget (OBJECT_SELF the module): a held window's pick, or the module's own script.
void OnTargetScript(uint32_t self) {
  RunScriptChunk("object p = GetLastPlayerToSelectTarget(); object o = GetTargetingModeSelectedObject();"
                 "SetLocalString(p, \"n5tw_o\", GetIsObjectValid(o) ? ObjectToString(o) : \"\");"
                 "SetLocalInt(p, \"n5tw_g\", GetIsObjectValid(o) && o == GetArea(p));"
                 "SetLocalInt(p, \"n5tw_k\", GetIsObjectValid(o) ? GetObjectType(o) : 0);"
                 "SetLocalInt(p, \"n5tw_ev\", GetLocalInt(p, \"n5tw_ev\") + 1);",
                 self);
  const ULONGLONG now = GetTickCount64();
  for (auto& seen : g_events) {
    void* pc = CreatureById(seen.first);
    if (!pc) continue;
    const int events = engine::GetLocalInt(pc, "n5tw_ev");
    if (events == seen.second) continue;
    seen.second = events;
    const auto held = g_held.find(seen.first);
    if (held != g_held.end() && held->second.open) {
      Pick(seen.first, &held->second);
      return;
    }
    const auto quiet = g_quiet.find(seen.first);
    if (quiet != g_quiet.end() && now < quiet->second) return;  // the window's own targeting ending
    break;
  }
  RunScriptChunk("string s = GetLocalString(GetModule(), \"n5tw_chain\"); if (s != \"\") ExecuteScript(s, GetModule());",
                 self);
}

void Open(uint32_t player, Held* held) {
  SetModuleEvent(player);
  void* pc = CreatureById(player);
  if (pc) g_events[player] = engine::GetLocalInt(pc, "n5tw_ev");
  if (!NuiOpen(player, kWindow, Layout(*held))) {
    // No window: the cast goes ahead at its target, every dart or ray there.
    const Held kept = *held;
    g_held.erase(player);
    Reissue(player, kept.order, kept.order.target);
    Log("target window: 0x%08X %s - the window didn't open, cast at its target", player, SpellName(kept.order.spell));
    return;
  }
  Refresh(player, held);
  EnterTargeting(player, static_cast<int>(held->order.spell));
  Log("target window: 0x%08X %s held - %zu rows, row 1 0x%s", player, SpellName(held->order.spell),
      held->rows.rows.size(), Hex(held->order.target).c_str());
}

void Update(void* creature) {
  if (g_held.empty() || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto found = g_held.find(id);
  if (found == g_held.end() || found->second.open) return;
  found->second.open = true;
  Open(id, &found->second);
}

void Adjust(void* /*creature*/, SpellOrder* /*order*/, bool from_input) { g_from_input = from_input; }

// 0.99.100 (owner, 2026-11-20: "if the player keeps clicking an enemy after all targets have been assigned the game
// assigns an attack action. make it so that can't happen while the player is assigning spell targets"): each pick ends
// EE's targeting mode and the window asks for it again kEnterAfterMs later, so a click in between is a plain click -
// an attack order (action 0xC). While the player's window is open, its attack orders are refused.
constexpr uint32_t kActionAttack = 0xC;

bool NoAttackWhilePicking(void* object, uint32_t action, uint32_t /*type1*/, const void* /*param1*/) {
  if (action != kActionAttack || !object) return false;
  const auto held = g_held.find(Field<uint32_t>(object, ee::kObjectId));
  if (held == g_held.end() || !held->second.open) return false;
  static int logged = 0;
  if (logged < 5) {
    ++logged;
    Log("target window: 0x%08X's attack order refused - it's picking %s's targets", held->first,
        SpellName(held->second.order.spell));
  }
  return true;
}

bool Guard(void* creature, const SpellOrder& order) {
  const bool input = g_from_input;
  g_from_input = false;
  if (!input || order.fake || order.spontaneous || order.class_slot < 0 || order.class_slot > 7) return false;
  if (order.spell != static_cast<uint32_t>(rules::kMagicMissile) &&
      order.spell != static_cast<uint32_t>(rules::kScorchingRay)) {
    return false;
  }
  if (!creature) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto old = g_held.find(id);
  if (old != g_held.end() && old->second.open) return true;  // a window already open: this order waits for it
  const int own = order.spell == static_cast<uint32_t>(rules::kMagicMissile) ? 1 : 2;
  const int slot = std::max(own, rules::ChosenSlotLevel(own, order.domain_level));
  Held held;
  held.order = order;
  held.rows = rules::MakeTargetRows(rules::TargetRowsFor(static_cast<int>(order.spell), slot),
                                    order.target != ee::kObjectInvalid ? order.target : 0u);
  g_held[id] = held;  // opened on the server's next frame (ServerTick), outside this order's own handling
  g_sent.erase(id);
  return true;
}

// #171 (0.99.87; owner, 2026-11-19, on TW-1: "this only works if the game is not paused"): the server's frame, paused
// or not - a held order's window opens (0.99.85 waited for the character's update, which a paused game doesn't run;
// 0.99.86 opened it inside the order's handling and asked for the cursor at once, which the client dropped), and an
// asked-for cursor comes kEnterAfterMs later while its window is open.
void ServerTick() {
  if (g_held.empty()) {
    g_enter.clear();
    return;
  }
  NoteEscape();
  for (auto& one : g_held) {
    if (one.second.open) continue;
    one.second.open = true;
    const uint32_t id = one.first;
    Open(id, &one.second);
    break;  // Open may erase from g_held: one a frame
  }
  if (g_enter.empty()) return;
  const ULONGLONG now = GetTickCount64();
  for (auto it = g_enter.begin(); it != g_enter.end();) {
    if (now < it->second.due) {
      ++it;
      continue;
    }
    const uint32_t player = it->first;
    const int spell = it->second.spell;
    it = g_enter.erase(it);
    const auto held = g_held.find(player);
    if (held != g_held.end() && held->second.open) EnterTargetingNow(player, spell);
  }
}

}  // namespace

std::vector<uint32_t> TakeWindowTargets(uint32_t caster, int spell) {
  const auto found = g_sent.find(caster);
  if (found == g_sent.end()) return {};
  if (GetTickCount64() - found->second.at > kSentForMs) {
    g_sent.erase(found);
    return {};
  }
  if (found->second.spell != spell) return {};
  std::vector<uint32_t> targets = found->second.targets;
  g_sent.erase(found);
  return targets;
}

bool InstallTargetWindow() {
  if (g_config.spellcasting != rules::RuleVariant::kFaithful) return false;
  const bool ok = AddSpellOrderAdjuster(&Adjust) && AddSpellOrderGuard(&Guard) && AddNuiWindow(kWindow, &OnWindowEvent) &&
                  AddScriptHandler(kTargetScript, &OnTargetScript) && AddCreatureUpdateListener(&Update) &&
                  AddServerTickListener(&ServerTick);
  InstallClientHover();  // a right-click's target (missing: logged; a right-click then takes nothing back)
  AddActionGuard(&NoAttackWhilePicking);  // no attack orders while a window is open (logged if it can't hook)
  Log("target window: Magic Missile's darts and Scorching Ray's rays picked one a row before the cast (#171)%s",
      ok ? "" : " - SOME hooks missing");
  return ok;
}
