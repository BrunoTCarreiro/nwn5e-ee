// A test tool (owner, 2026-10-23: "an NUI window with a clickable list with
// all the effect animations so that I can browse them"): `!vfx` opens a window
// listing visualeffects.2da's named rows (the served table: nwn5e's rows too,
// e.g. the Short Rest's 1550). A click plays one on the player character -
// fire-and-forget and projectile rows once, duration rows for 8 s, beams from
// the character to the nearest creature for 3 s - and the window shows its
// row, kind and model; "Clear effects" removes the visuals on the character.
// The search field (owner, 2026-10-23: "can you add a search bar to the vfx
// window?") narrows the list as you type, by label or row number: the full
// list is kept on the player (a local json), a watch on the field filters it
// in NWScript, and the shown rows' indices map a click back to its effect.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "chat.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "nui.h"

namespace {

constexpr const char* kWindow = "nwn5evfx";
constexpr const char* kVarAll = "nwn5e_vfx_all";    // every label (json array)
constexpr const char* kVarShown = "nwn5e_vfx_idx";  // the shown rows' indices into it (json array)
constexpr const char* kVarPick = "nwn5e_vfx_pick";

struct VisualRow {
  int row;
  std::string label;
  char kind;          // F fire-and-forget, D duration, B beam, P projectile
  std::string model;  // the first model the row names, for the status line
};
std::vector<VisualRow> g_rows;

// An NWScript string literal (quotes and backslashes escaped).
std::string ScriptLiteral(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

const char* KindName(char kind) {
  switch (kind) {
    case 'F': return "instant";
    case 'D': return "duration, 8 s";
    case 'B': return "beam to the nearest creature";
    case 'P': return "projectile";
    default: return "?";
  }
}

bool LoadRows() {
  if (!g_rows.empty()) return true;
  const std::vector<std::string> labels = GameTableColumn("visualeffects", "Label");
  const std::vector<std::string> kinds = GameTableColumn("visualeffects", "Type_FD");
  if (labels.empty()) return false;
  static const char* const kModelColumns[] = {"Imp_Impact_Node", "Imp_HeadCon_Node", "Imp_Root_S_Node",
                                              "Imp_Root_M_Node", "Imp_Root_L_Node",  "Imp_Root_H_Node",
                                              "ProgFX_Impact",   "Ces_Impact_Node",  "Ces_HeadCon_Node"};
  std::vector<std::vector<std::string>> models;
  for (const char* column : kModelColumns) models.push_back(GameTableColumn("visualeffects", column));
  for (size_t row = 0; row < labels.size(); ++row) {
    const std::string& label = labels[row];
    if (label.empty() || label == "OS_RESERVED" || label == "RESERVED" || label == "DELETED" || label == "PADDING") {
      continue;
    }
    VisualRow visual{static_cast<int>(row), label, row < kinds.size() && !kinds[row].empty() ? kinds[row][0] : 'F', ""};
    for (const auto& column : models) {
      if (row < column.size() && !column[row].empty()) {
        visual.model = column[row];
        break;
      }
    }
    g_rows.push_back(visual);
  }
  Log("vfx browser: %zu named visual effects", g_rows.size());
  return true;
}

std::string Layout() {
  using namespace nui;
  return Window(Col(List({
                    Label(Bind("status"), 44.0f),
                    TextEdit("search", "Search - a name or a row number", "search", 40, 32.0f),
                    ListView(Cell(Button("vfx", Bind("labels")), 0.0f, true), Bind("count"), 26.0f),
                    Row(List({Button("clear", Str("Clear effects"), 140.0f), Spacer(),
                              Button("close", Str("Close"), 100.0f)})),
                })),
                Str("nwn5e - visual effects"), 60.0f, 60.0f, 480.0f, 660.0f);
}

void Open(uint32_t player) {
  if (!LoadRows()) {
    TellPlayer(player, "nwn5e: visualeffects.2da could not be read.");
    return;
  }
  if (!NuiOpen(player, kWindow, Layout(),
               {{"status", std::to_string(g_rows.size()) + " visual effects - click one to play it on you."},
                {"search", ""}})) {
    return;
  }
  std::string labels = "[";
  for (size_t i = 0; i < g_rows.size(); ++i) {
    char line[160];
    sprintf_s(line, "%d  %s", g_rows[i].row, g_rows[i].label.c_str());
    labels += (i ? "," : "") + nui::Str(line);
  }
  labels += "]";
  NuiSetJson(player, kWindow, "labels", labels);
  NuiSetJson(player, kWindow, "count", std::to_string(g_rows.size()));
  // The full list for the search; no filter yet (a click's index is the row's).
  const std::string keep = "SetLocalJson(OBJECT_SELF, \"" + std::string(kVarAll) + "\", JsonParse(" +
                           ScriptLiteral(labels) + ")); DeleteLocalJson(OBJECT_SELF, \"" + kVarShown +
                           "\"); NuiSetBindWatch(OBJECT_SELF, " + std::to_string(NuiToken(player, kWindow)) +
                           ", \"search\", TRUE);";
  RunScriptChunk(keep.c_str(), player);
}

// The list narrowed to the labels holding the search text (any case).
void Filter(uint32_t player) {
  const int token = NuiToken(player, kWindow);
  if (!token) return;
  char code[1400];
  sprintf_s(code,
            "int t = %d; json all = GetLocalJson(OBJECT_SELF, \"%s\");"
            "string q = GetStringLowerCase(JsonGetString(NuiGetBind(OBJECT_SELF, t, \"search\")));"
            "json shown = JsonArray(); json idx = JsonArray(); int n = JsonGetLength(all); int i;"
            "for (i = 0; i < n; i++) { string s = JsonGetString(JsonArrayGet(all, i));"
            " if (q == \"\" || FindSubString(GetStringLowerCase(s), q) >= 0) {"
            " shown = JsonArrayInsert(shown, JsonString(s)); idx = JsonArrayInsert(idx, JsonInt(i)); } }"
            "SetLocalJson(OBJECT_SELF, \"%s\", idx);"
            "NuiSetBind(OBJECT_SELF, t, \"labels\", shown);"
            "NuiSetBind(OBJECT_SELF, t, \"count\", JsonInt(JsonGetLength(shown)));"
            "NuiSetBind(OBJECT_SELF, t, \"status\", JsonString(q == \"\" ? IntToString(n) + \" visual effects - "
            "click one to play it on you.\" : IntToString(JsonGetLength(shown)) + \" of \" + IntToString(n) + "
            "\" match - click one to play it on you.\"));",
            token, kVarAll, kVarShown);
  RunScriptChunk(code, player);
}

// The effect a click on shown row `index` means (-1 if none).
int Picked(uint32_t player, int index) {
  void* creature = CreatureById(player);
  if (!creature) return -1;
  char code[400];
  sprintf_s(code,
            "json idx = GetLocalJson(OBJECT_SELF, \"%s\"); int pick = %d;"
            "if (JsonGetType(idx) == JSON_TYPE_ARRAY) pick = %d < JsonGetLength(idx) ? "
            "JsonGetInt(JsonArrayGet(idx, %d)) : -1;"
            "SetLocalInt(OBJECT_SELF, \"%s\", pick + 1);",
            kVarShown, index, index, index, kVarPick);
  RunScriptChunk(code, player);
  return engine::GetLocalInt(creature, kVarPick) - 1;
}

void Play(uint32_t player, const VisualRow& visual) {
  char code[512];
  switch (visual.kind) {
    case 'D':
      sprintf_s(code, "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect(%d), OBJECT_SELF, 8.0);",
                visual.row);
      break;
    case 'B':
      sprintf_s(code,
                "object t = GetNearestObject(OBJECT_TYPE_CREATURE, OBJECT_SELF); if (GetIsObjectValid(t)) "
                "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectBeam(%d, OBJECT_SELF, BODY_NODE_HAND), t, 3.0); "
                "else SendMessageToPC(OBJECT_SELF, \"nwn5e: a beam needs a creature nearby to aim at.\");",
                visual.row);
      break;
    default:
      sprintf_s(code, "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(%d), OBJECT_SELF);", visual.row);
      break;
  }
  RunScriptChunk(code, player);
  NuiSetText(player, kWindow, "status",
             std::to_string(visual.row) + "  " + visual.label + " (" + KindName(visual.kind) + ")" +
                 (visual.model.empty() ? std::string() : " - model " + visual.model));
}

void OnEvent(const NuiEvent& event) {
  if (event.type == "watch" && event.element == "search") {
    Filter(event.player);
    return;
  }
  if (event.type != "click") return;
  if (event.element == "close") {
    NuiClose(event.player, kWindow);
  } else if (event.element == "clear") {
    RunScriptChunk("effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectType(e) == "
                   "EFFECT_TYPE_VISUALEFFECT || GetEffectType(e) == EFFECT_TYPE_BEAM) RemoveEffect(OBJECT_SELF, e); "
                   "e = GetNextEffect(OBJECT_SELF); }",
                   event.player);
    NuiSetText(event.player, kWindow, "status", "Visual effects on you removed.");
  } else if (event.element == "vfx" && event.array_index >= 0) {
    const int pick = Picked(event.player, event.array_index);
    if (pick >= 0 && pick < static_cast<int>(g_rows.size())) Play(event.player, g_rows[pick]);
  }
}

std::vector<std::string> VfxCommand(void* creature, const std::string&) {
  Open(Field<uint32_t>(creature, ee::kObjectId));
  return {};
}

}  // namespace

void InstallVfxBrowser() {
  if (!AddNuiWindow(kWindow, &OnEvent)) return;
  AddChatCommand("!vfx", &VfxCommand);
}
