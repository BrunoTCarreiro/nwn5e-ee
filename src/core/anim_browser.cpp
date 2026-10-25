// A test tool (owner, 2026-10-24: "is there a way to preview walking
// animations?" ... "build the !anim window"): `!anim` opens a window listing
// the animations of the game's humanoid model (a_ba / a_fa - the names the
// engine's animation table uses, plus the walks it picks by state: walkinj,
// the injured limp, and walkdead have no id of their own).
// - A click plays one on the player character in place, looping: EE's
//   ReplaceObjectAnimation swaps the look-far loop for it and the character
//   plays that loop (ActionPlayAnimation) - any new order ends it.
// - "Walk a loop" / "Run a loop": the character walks (runs) 5 m ahead and
//   back twice with the selected animation as its walk (run); with nothing
//   selected, the game's own walk - a hurt character's is the limp.
// - The speed buttons set the playback speed (the in-place loop's speed, and
//   the object's animation-speed visual transform for the loops).
// - "Play typed" plays the search field's text as an animation name (other
//   models' animations). "Stop", or closing the window, puts everything back.
// The search field narrows the list as in `!vfx`.

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "nui.h"

void ResendLook(uint32_t id);  // stance_look.cpp (the exhausted look, the crouch when hiding)

namespace {

constexpr const char* kWindow = "nwn5eanim";
constexpr const char* kVarAll = "nwn5e_anim_all";    // every label (json array)
constexpr const char* kVarShown = "nwn5e_anim_idx";  // the shown rows' indices into it (json array)
constexpr const char* kVarPick = "nwn5e_anim_pick";
constexpr const char* kVarSelected = "nwn5e_anim_sel";  // the animation picked last (a local string)
constexpr const char* kHoldSlot = "lookfar";  // the loop swapped for the previewed animation

struct Animation {
  const char* name;
  const char* note;
};

// The humanoid model's animations (a_ba; "(fa)" = the female set's too), movement first.
const Animation kAnimations[] = {
    {"walk", "walk"},
    {"walkinj", "injured walk - a limp (the game's walk when hurt)"},
    {"walkdead", "walk (the dead / undead state)"},
    {"run", "run"},
    {"cwalkf", "combat step forward"},
    {"cwalkb", "combat step back"},
    {"cwalkl", "combat step left"},
    {"cwalkr", "combat step right"},
    {"runfl", "run, turning left"},
    {"runfr", "run, turning right"},
    {"pausewalkl", "turn on the spot, left"},
    {"pausewalkr", "turn on the spot, right"},
    {"pausewalkfl", "turn on the spot, front left"},
    {"pausewalkfr", "turn on the spot, front right"},
    {"pauseturn", "turn"},
    {"cturnr", "combat turn"},
    {"walk_swordr", "arms while walking: sword, right"},
    {"walk_swordl", "arms while walking: sword, left"},
    {"walk_shieldl", "arms while walking: shield"},
    {"walk_bowl", "arms while walking: bow"},
    {"run_swordr", "arms while running: sword, right"},
    {"run_swordl", "arms while running: sword, left"},
    {"run_shieldl", "arms while running: shield"},
    {"run_bowl", "arms while running: bow"},
    {"pause1", "idle"},
    {"pause2", "idle 2"},
    {"pausesh", "idle: scratch head"},
    {"pausebrd", "idle: bored"},
    {"pausetrd", "idle: tired"},
    {"pausepsn", "idle: poisoned"},
    {"nwreadyl", "ready, unarmed (left)"},
    {"nwreadyr", "ready, unarmed (right)"},
    {"1hreadyl", "ready, one-handed (left)"},
    {"1hreadyr", "ready, one-handed (right)"},
    {"2hreadyl", "ready, two-handed"},
    {"2wreadyl", "ready, two weapons"},
    {"listen", "listen"},
    {"lookfar", "look far"},
    {"greeting", "greeting"},
    {"salute", "salute"},
    {"bow", "bow"},
    {"taunt", "taunt"},
    {"victoryfr", "victory (fighter)"},
    {"victorymg", "victory (mage)"},
    {"victoryth", "victory (thief)"},
    {"meditate", "meditate"},
    {"worship", "worship"},
    {"sit", "sit"},
    {"sitcross", "sit cross-legged"},
    {"sitcrossps", "sitting cross-legged"},
    {"drink", "drink"},
    {"read", "read"},
    {"steal", "steal"},
    {"getlowlp", "pick up, low"},
    {"getmidlp", "pick up, middle"},
    {"drwright", "draw a weapon, right"},
    {"drwleft", "draw a weapon, left"},
    {"throwr", "throw"},
    {"nwstab", "stab, unarmed"},
    {"whirlwind", "whirlwind attack"},
    {"dodges", "dodge"},
    {"dodgelr", "dodge, side"},
    {"damages", "hit reaction"},
    {"damagel", "hit reaction, left"},
    {"damager", "hit reaction, right"},
    {"spasm", "spasm"},
    {"conjure1", "conjure 1"},
    {"conjure2", "conjure 2"},
    {"castout", "cast, out"},
    {"castself", "cast, self"},
    {"castup", "cast, up"},
    {"castarea", "cast, area"},
    {"castpoint", "cast, point"},
    {"disappearlp", "disappear"},
    {"kdfnt", "knocked down, face down"},
    {"kdbck", "knocked down, on the back"},
    {"kdfntps", "lying face down"},
    {"kdbckps", "lying on the back"},
    {"gustandf", "get up from face down"},
    {"gustandb", "get up from the back"},
    {"gutokdf", "go down, face down"},
    {"gutokdb", "go down, on the back"},
    {"deadfnt", "dead, face down"},
    {"deadbck", "dead, on the back"},
};
constexpr int kAnimationCount = static_cast<int>(sizeof(kAnimations) / sizeof(kAnimations[0]));

struct Preview {
  float speed = 1.0f;
  bool holding = false;  // playing the selected animation in place
};
std::unordered_map<uint32_t, Preview> g_previews;  // by player, the engine's thread only

std::string ScriptLiteral(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

std::string SpeedText(float speed) {
  char text[16];
  sprintf_s(text, "%gx", speed);
  return text;
}

void SetStatus(uint32_t player, const std::string& line) { NuiSetText(player, kWindow, "status", line); }

std::string Layout() {
  using namespace nui;
  return Window(Col(List({
                    Label(Bind("status"), 52.0f),
                    TextEdit("search", "Search - or type any animation name and press Play typed", "search", 40, 32.0f),
                    ListView(Cell(Button("anim", Bind("labels")), 0.0f, true), Bind("count"), 26.0f),
                    Row(List({Button("walk", Str("Walk a loop"), 120.0f), Button("run", Str("Run a loop"), 120.0f),
                              Spacer(), Button("typed", Str("Play typed"), 110.0f)})),
                    Row(List({Label(Str("Speed"), 0.0f), Button("s25", Str("0.25x"), 64.0f),
                              Button("s50", Str("0.5x"), 64.0f), Button("s100", Str("1x"), 64.0f),
                              Button("s200", Str("2x"), 64.0f), Spacer()})),
                    Row(List({Button("stop", Str("Stop"), 120.0f), Spacer(), Button("close", Str("Close"), 100.0f)})),
                })),
                Str("nwn5e - animations"), 60.0f, 60.0f, 500.0f, 720.0f);
}

// The selected animation (the local string) played in place, looping, at the preview speed.
std::string HoldScript(float speed) {
  char code[400];
  sprintf_s(code,
            "string s = GetLocalString(OBJECT_SELF, \"%s\"); if (s != \"\") {"
            " ReplaceObjectAnimation(OBJECT_SELF, \"%s\", s);"
            " SetObjectVisualTransform(OBJECT_SELF, OBJECT_VISUAL_TRANSFORM_ANIMATION_SPEED, 1.0);"
            " ClearAllActions(); ActionPlayAnimation(ANIMATION_LOOPING_LOOK_FAR, %.2f, 600.0); }",
            kVarSelected, kHoldSlot, speed);
  return code;
}

// Picks `name` and plays it in place.
void Hold(uint32_t player, Preview& preview, const std::string& name, const std::string& note) {
  preview.holding = true;
  const std::string code = "SetLocalString(OBJECT_SELF, \"" + std::string(kVarSelected) + "\", " + ScriptLiteral(name) +
                           ");" + HoldScript(preview.speed);
  RunScriptChunk(code.c_str(), player);
  SetStatus(player, name + " - " + note + ". Playing in place at " + SpeedText(preview.speed) +
                        " (any order ends it). Walk / Run a loop use it as the walk / run.");
}

// Picks the search field's text as an animation name (cut to 16 characters) and plays it in place.
void HoldTyped(uint32_t player, Preview& preview) {
  const int token = NuiToken(player, kWindow);
  if (!token) return;
  preview.holding = true;
  char code[700];
  sprintf_s(code,
            "string q = GetStringLeft(GetStringLowerCase(JsonGetString(NuiGetBind(OBJECT_SELF, %d, \"search\"))), 16);"
            "if (q == \"\") { NuiSetBind(OBJECT_SELF, %d, \"status\", JsonString(\"Type an animation name in the "
            "search field first.\")); return; }"
            "SetLocalString(OBJECT_SELF, \"%s\", q);"
            "NuiSetBind(OBJECT_SELF, %d, \"status\", JsonString(q + \" - typed. Playing in place at %s (nothing "
            "moves if the model has no such animation). Walk / Run a loop use it.\"));",
            token, token, kVarSelected, token, SpeedText(preview.speed).c_str());
  const std::string all = std::string(code) + HoldScript(preview.speed);
  RunScriptChunk(all.c_str(), player);
}

// Walks (runs) 5 m ahead and back twice, the selected animation as the walk (run).
void Loop(uint32_t player, Preview& preview, bool run) {
  preview.holding = false;
  const char* slot = run ? "run" : "walk";
  const int token = NuiToken(player, kWindow);
  char code[1500];
  sprintf_s(code,
            "string s = GetLocalString(OBJECT_SELF, \"%s\"); ReplaceObjectAnimation(OBJECT_SELF, \"%s\", \"%s\");"
            "if (s != \"\") ReplaceObjectAnimation(OBJECT_SELF, \"%s\", s);"
            "SetObjectVisualTransform(OBJECT_SELF, OBJECT_VISUAL_TRANSFORM_ANIMATION_SPEED, %.2f);"
            "ClearAllActions(); vector p = GetPosition(OBJECT_SELF); float f = GetFacing(OBJECT_SELF);"
            "object a = GetArea(OBJECT_SELF); location there = Location(a, p + AngleToVector(f) * 5.0, f);"
            "location back = Location(a, p, f + 180.0); int i;"
            "for (i = 0; i < 2; i++) { ActionMoveToLocation(there, %s); ActionMoveToLocation(back, %s); }"
            "NuiSetBind(OBJECT_SELF, %d, \"status\", JsonString(\"%s 5 m out and back twice at %s - \" + "
            "(s == \"\" ? \"the game's own %s (a hurt character limps).\" : s + \" as the %s.\")));",
            kVarSelected, kHoldSlot, kHoldSlot, slot, preview.speed, run ? "TRUE" : "FALSE", run ? "TRUE" : "FALSE", token,
            run ? "Running" : "Walking", SpeedText(preview.speed).c_str(), slot, slot);
  RunScriptChunk(code, player);
}

// Every swap undone, speed 1, the character's actions cleared.
void Restore(uint32_t player, bool clear_actions) {
  std::string code = clear_actions ? "ClearAllActions();" : "";
  for (const char* slot : {kHoldSlot, "walk", "run"}) {
    code += "ReplaceObjectAnimation(OBJECT_SELF, \"" + std::string(slot) + "\", \"" + slot + "\");";  // to itself: stance_look.cpp
  }
  code += "SetObjectVisualTransform(OBJECT_SELF, OBJECT_VISUAL_TRANSFORM_ANIMATION_SPEED, 1.0);";
  RunScriptChunk(code.c_str(), player);
  ResendLook(player);  // an exhausted character's limp, a hiding one's crouch, come back
}

void Open(uint32_t player) {
  g_previews[player] = Preview{};
  if (!NuiOpen(player, kWindow, Layout(),
               {{"status", std::to_string(kAnimationCount) +
                               " animations of the humanoid model - click one to play it on you, in place."},
                {"search", ""}})) {
    return;
  }
  std::string labels = "[";
  for (int i = 0; i < kAnimationCount; ++i) {
    labels += (i ? "," : "") + nui::Str(std::string(kAnimations[i].name) + "  -  " + kAnimations[i].note);
  }
  labels += "]";
  NuiSetJson(player, kWindow, "labels", labels);
  NuiSetJson(player, kWindow, "count", std::to_string(kAnimationCount));
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
            "NuiSetBind(OBJECT_SELF, t, \"status\", JsonString(q == \"\" ? IntToString(n) + \" animations - click "
            "one to play it on you, in place.\" : IntToString(JsonGetLength(shown)) + \" of \" + IntToString(n) + "
            "\" match - or press Play typed to play the text as a name.\"));",
            token, kVarAll, kVarShown);
  RunScriptChunk(code, player);
}

// The animation a click on shown row `index` means (-1 if none).
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

void OnEvent(const NuiEvent& event) {
  const uint32_t player = event.player;
  Preview& preview = g_previews[player];
  if (event.type == "close") {  // the window's X
    Restore(player, preview.holding);
    RunScriptChunk(("DeleteLocalString(OBJECT_SELF, \"" + std::string(kVarSelected) + "\");").c_str(), player);
    g_previews.erase(player);
    return;
  }
  if (event.type == "watch" && event.element == "search") {
    Filter(player);
    return;
  }
  if (event.type != "click") return;
  if (event.element == "close") {
    Restore(player, preview.holding);
    RunScriptChunk(("DeleteLocalString(OBJECT_SELF, \"" + std::string(kVarSelected) + "\");").c_str(), player);
    g_previews.erase(player);
    NuiClose(player, kWindow);
  } else if (event.element == "stop") {
    Restore(player, true);
    preview.holding = false;
    SetStatus(player, "Stopped - every animation back to the game's own, speed 1x.");
  } else if (event.element == "anim" && event.array_index >= 0) {
    const int pick = Picked(player, event.array_index);
    if (pick >= 0 && pick < kAnimationCount) Hold(player, preview, kAnimations[pick].name, kAnimations[pick].note);
  } else if (event.element == "walk" || event.element == "run") {
    Loop(player, preview, event.element == "run");
  } else if (event.element == "typed") {
    HoldTyped(player, preview);
  } else if (event.element == "s25" || event.element == "s50" || event.element == "s100" || event.element == "s200") {
    preview.speed = event.element == "s25" ? 0.25f : event.element == "s50" ? 0.5f : event.element == "s100" ? 1.0f : 2.0f;
    if (preview.holding) {
      RunScriptChunk(HoldScript(preview.speed).c_str(), player);
      SetStatus(player, "Speed " + SpeedText(preview.speed) + " - playing in place.");
    } else {
      char code[160];
      sprintf_s(code, "SetObjectVisualTransform(OBJECT_SELF, OBJECT_VISUAL_TRANSFORM_ANIMATION_SPEED, %.2f);",
                preview.speed);
      RunScriptChunk(code, player);
      SetStatus(player, "Speed " + SpeedText(preview.speed) + ".");
    }
  }
}

}  // namespace

// !animedit's dropdowns: !anim's description of an animation, or nullptr.
const char* AnimationNote(const std::string& name) {
  for (const Animation& animation : kAnimations) {
    if (name == animation.name) return animation.note;
  }
  return nullptr;
}

namespace {

std::vector<std::string> AnimCommand(void* creature, const std::string&) {
  Open(Field<uint32_t>(creature, ee::kObjectId));
  return {};
}

}  // namespace

void InstallAnimBrowser() {
  if (!AddNuiWindow(kWindow, &OnEvent)) return;
  AddChatCommand("!anim", &AnimCommand);
}
