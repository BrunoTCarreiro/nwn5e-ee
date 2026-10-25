// Light sources you can put out, and darker maps (0.36.0).
//
// Putting a light out (owner, 2026-10-25: "putting off a light source is a good
// one" - L1: "attacking (shooting an arrow, meleeing or using an appropriate
// spell like ice dagger), interacting directly ... or via another spell like
// mage hand or prestidigitation"; L3 as recommended: guards notice). When a
// player character enters an area, each lit light placeable in it
// (GetPlaceableIllumination: braziers, lamps, torch brackets; marked
// nwn5e_lamp so a doused one is found again after a load) that has no
// inventory and isn't static gets nwn5e's own event scripts wherever its own
// are empty: on used (and made usable: a click - or Use on the radial menu -
// walks the character over; the stock light switches nw_02_onoff, which never
// turned the light itself off, and x2_am_lampuse are replaced too), on damaged
// (a hit: on attacked fires for misses as well), on a spell cast at it. A static
// placeable (most stock blueprints without a Static field load so) is scenery
// to the client - sent once with the area, no updates, not clickable, and
// SetUseableFlag refuses it - so it is left alone (found by the useable flag
// not taking). 0.36.1 (owner: "I can't click any lights" - the log: "0 light
// sources here can be put out (12 static ...)"): every light in the areas tried
// was static, so a lit one without inventory is made a normal placeable as it
// loads - CNWSPlaceable::LoadPlaceable is hooked, and after it its Static
// (+0x5a0; a blueprint without the field is static unless Useable) is cleared
// when LightState (+0x594) is on and HasInventory (+0x4f8) off. That is before
// the area is sent to the client (PackAreaIntoMessage reads it), so the client
// gets it as an object it can click and update. It stays plot (LoadPlaceable
// makes a static placeable plot): a hit does no damage, but OnDamaged still
// fires (OnApplyDamage posts it before its plot test), so it can't be smashed. Those scripts don't exist as
// files: a hook on CVirtualMachine::RunScript (every event script goes through
// it: CNWSObject::RunEventScript -> RunScript(name, self, 1, type x 1000 +
// event)) runs nwn5e's code for the names n5_lamp_use / n5_lamp_hit /
// n5_lamp_spl. Used: a lit light goes out, an unlit one is lit again. Hit by a
// weapon or a non-fire spell's damage, or a cold or wind spell cast at it (Ray
// of Frost, Ice Dagger, Gust of Wind, Cone of Cold, Ice Storm - the area ones
// signal hostile-faction placeables, as stock lights are): it goes out, and
// the damage is mended so it isn't smashed (a stock light has 10-15 hit points;
// one blow big enough still breaks it). The Light spell lights it again.
// Mage Hand and Prestidigitation aren't NWN spells; they come with the spells. Out = the placeable's
// deactivate animation (its flame), illumination off, the area's static
// lighting recomputed - the stealth light rule (stealth_watch.cpp) reads the
// change on its next pass. Guards notice (2024 perception, heard_investigates):
// a hostile within 20 m with a line to the light comes to look - walks there
// searching, waits 6 s, walks back - as for a noise. `light_sources`.
//
// Darker maps (owner: "I'd like to recompute the lighting in the maps as they
// seem to be a bit too bright right now"; M1 / M2 as recommended): when a player
// character first enters an area this session, the area's sun and moon light
// (ambient and diffuse) is set to map_light % of the module's, and each tile's
// two main lights one step darker (bright white -> white -> dim white; a full
// colour -> its dark one, a pale one -> its pale dark one; torches - the source
// lights - untouched), then the static lighting is recomputed. From the
// originals, kept on the area as locals (saved with the game), so nothing is
// darkened twice. `!maplight <percent>` sets it again in the current area and
// saves it to nwn5e.ini. `map_light` (100 = the module's own), `map_tile_dim`.

#include <windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"

namespace {

using RunScriptFn = int (*)(void* vm, void* name, uint32_t self, int self_valid, int event);
using LoadPlaceableFn = int (*)(void* placeable, void* gff, void* gff_struct, int flag, void* tag);
RunScriptFn g_run_script = nullptr;
LoadPlaceableFn g_load_placeable = nullptr;

constexpr const char* kLoadPlaceable = "?LoadPlaceable@CNWSPlaceable@@QEAAHPEAVCResGFF@@PEAVCResStruct@@HPEAVCExoString@@@Z";
constexpr size_t kPlaceableStatic = 0x5a0;        // int32 (LoadPlaceable: "Static")
constexpr size_t kPlaceableLightState = 0x594;    // int32 ("LightState": lit)
constexpr size_t kPlaceableHasInventory = 0x4f8;  // int32 ("HasInventory")
constexpr size_t kPlaceableAppearance = 0x32c;    // uint16 ("Appearance": placeables.2da)
int g_unstatic = 0;                               // static lights made normal placeables

int LoadPlaceable(void* placeable, void* gff, void* gff_struct, int flag, void* tag) {
  const int result = g_load_placeable(placeable, gff, gff_struct, flag, tag);
  if (result && placeable && Field<int32_t>(placeable, kPlaceableStatic) == 1 &&
      Field<int32_t>(placeable, kPlaceableLightState) != 0 && Field<int32_t>(placeable, kPlaceableHasInventory) == 0) {
    SetField<int32_t>(placeable, kPlaceableStatic, 0);
    if (++g_unstatic <= 5 || g_unstatic % 50 == 0) {
      Log("lights: a static light (placeables.2da row %d) loaded as a normal placeable, so it can be put out (%d so far)",
          Field<uint16_t>(placeable, kPlaceableAppearance), g_unstatic);
    }
  }
  return result;
}

constexpr const char* kRunScript = "?RunScript@CVirtualMachine@@QEAAHPEAVCExoString@@IHH@Z";
constexpr const char* kUseScript = "n5_lamp_use";
constexpr const char* kHitScript = "n5_lamp_hit";
constexpr const char* kSpellScript = "n5_lamp_spl";

// The light out / lit again, on OBJECT_SELF (the placeable).
constexpr const char* kDouse =
    "PlayAnimation(ANIMATION_PLACEABLE_DEACTIVATE); SetPlaceableIllumination(OBJECT_SELF, FALSE);"
    "RecomputeStaticLighting(GetArea(OBJECT_SELF)); SetLocalInt(OBJECT_SELF, \"nwn5e_lamp_out\", 1);";
constexpr const char* kRelight =
    "PlayAnimation(ANIMATION_PLACEABLE_ACTIVATE); SetPlaceableIllumination(OBJECT_SELF, TRUE);"
    "RecomputeStaticLighting(GetArea(OBJECT_SELF)); DeleteLocalInt(OBJECT_SELF, \"nwn5e_lamp_out\");";

// The spells that put a flame out (cold, water, wind).
constexpr const char* kDousingSpells =
    "sp == SPELL_RAY_OF_FROST || sp == SPELL_ICE_DAGGER || sp == SPELL_GUST_OF_WIND || sp == SPELL_CONE_OF_COLD"
    " || sp == SPELL_ICE_STORM";

bool Investigates() {
  return g_config.heard_investigates && g_config.perception == rules::RuleVariant::kFaithful;
}

// L3: hostiles to `u` within 20 m with a line to the light come to look.
std::string NoticeCode() {
  if (!Investigates()) return "";
  return "location at = GetLocation(OBJECT_SELF); int came = 0;"
         "object o = GetFirstObjectInShape(SHAPE_SPHERE, 20.0, at, TRUE, OBJECT_TYPE_CREATURE);"
         "while (GetIsObjectValid(o)) {"
         " if (GetIsObjectValid(u) && GetIsEnemy(u, o) && !GetIsDead(o) && !GetIsInCombat(o) && GetCommandable(o)"
         "     && !IsInConversation(o) && !GetPlotFlag(o) && !GetIsObjectValid(GetMaster(o))"
         "     && !GetLocalInt(o, \"nwn5e_seek\")) {"
         "  SetLocalInt(o, \"nwn5e_seek\", 25); came++;"
         "  location home = GetLocation(o);"
         "  AssignCommand(o, ClearAllActions());"
         "  SetActionMode(o, ACTION_MODE_DETECT, TRUE);"
         "  AssignCommand(o, ActionMoveToLocation(at));"
         "  AssignCommand(o, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_LEFT));"
         "  AssignCommand(o, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_RIGHT));"
         "  AssignCommand(o, ActionWait(6.0));"
         "  AssignCommand(o, ActionMoveToLocation(home));"
         "  AssignCommand(o, ActionDoCommand(SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, FALSE)));"
         "  AssignCommand(o, ActionDoCommand(DeleteLocalInt(OBJECT_SELF, \"nwn5e_seek\")));"
         "  if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: \" + GetName(o) + \" saw the light go out and comes to look\");"
         " }"
         " o = GetNextObjectInShape(SHAPE_SPHERE, 20.0, at, TRUE, OBJECT_TYPE_CREATURE);"
         "}"
         "SetLocalInt(OBJECT_SELF, \"nwn5e_lamp_came\", came);";
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0) Log("lights: the %s script failed (%d)", what, result);
}

void Used(uint32_t lamp) {
  Run("object u = GetLastUsedBy();"
      "if (GetPlaceableIllumination(OBJECT_SELF)) {" +
          std::string(kDouse) +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: you put out \" + GetName(OBJECT_SELF));" + NoticeCode() +
          "} else {" + kRelight +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: you light \" + GetName(OBJECT_SELF));"
          "}",
      lamp, "light used");
  Log("lights: 0x%08X used", lamp);
}

void Damaged(uint32_t lamp) {
  Run("object u = GetLastDamager(); int all = GetTotalDamageDealt(); int fire = GetDamageDealtByType(DAMAGE_TYPE_FIRE);"
      "if (fire < 0) fire = 0;"
      "if (GetCurrentHitPoints(OBJECT_SELF) < GetMaxHitPoints(OBJECT_SELF))"
      " ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectHeal(GetMaxHitPoints(OBJECT_SELF)), OBJECT_SELF);"
      "if (GetPlaceableIllumination(OBJECT_SELF) && (all <= 0 || all > fire)) {" +
          std::string(kDouse) +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: you hit \" + GetName(OBJECT_SELF) + \" and put it out\");" +
          NoticeCode() + "}",
      lamp, "light hit");
  Log("lights: 0x%08X hit", lamp);
}

void SpellAt(uint32_t lamp) {
  Run("object u = GetLastSpellCaster(); int sp = GetLastSpell();"
      "if (GetPlaceableIllumination(OBJECT_SELF) && (" +
          std::string(kDousingSpells) + ")) {" + kDouse +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: your spell puts out \" + GetName(OBJECT_SELF));" +
          NoticeCode() + "} else if (!GetPlaceableIllumination(OBJECT_SELF) && sp == SPELL_LIGHT) {" + kRelight +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: your spell lights \" + GetName(OBJECT_SELF));"
          "}",
      lamp, "spell at a light");
  Log("lights: 0x%08X a spell cast at it", lamp);
}

// A CExoString (char* +0, length +8) naming `script`, resrefs being case-blind.
bool Names(void* name, const char* script) {
  if (!name) return false;
  const char* chars = Field<const char*>(name, 0);
  const uint32_t length = Field<uint32_t>(name, 8);
  return chars && length == strlen(script) && _strnicmp(chars, script, length) == 0;
}

int RunScript(void* vm, void* name, uint32_t self, int self_valid, int event) {
  if (name) {
    const char* chars = Field<const char*>(name, 0);
    if (chars && (chars[0] == 'n' || chars[0] == 'N') && (chars[1] == '5')) {
      if (Names(name, kUseScript)) {
        Used(self);
        return 1;
      }
      if (Names(name, kHitScript)) {
        Damaged(self);
        return 1;
      }
      if (Names(name, kSpellScript)) {
        SpellAt(self);
        return 1;
      }
    }
  }
  return g_run_script(vm, name, self, self_valid, event);
}

// --- Per area, when a player character arrives.
int g_map_light = 100;  // percent of the module's area light
std::unordered_set<void*> g_lit_areas;  // areas set up this session (compared only)

// The area's light from its originals: map_light % of the sun and moon light,
// the tiles' main lights a step darker (both back to the module's at 100).
std::string DarkenCode(int percent) {
  const bool dim = percent < 100;
  // At 100% only an area darkened before (its originals kept) is set back.
  std::string code = "object a = GetArea(OBJECT_SELF); float f = " + std::to_string(percent / 100.0) + ";"
                     "if (f < 1.0 || GetLocalInt(a, \"nwn5e_light_orig0\")) {";
  const char* types[] = {"AREA_LIGHT_COLOR_MOON_AMBIENT", "AREA_LIGHT_COLOR_MOON_DIFFUSE",
                         "AREA_LIGHT_COLOR_SUN_AMBIENT", "AREA_LIGHT_COLOR_SUN_DIFFUSE"};
  for (int i = 0; i < 4; ++i) {
    const std::string n = std::to_string(i);
    code += "int o" + n + " = GetLocalInt(a, \"nwn5e_light_orig" + n + "\");"
            "if (!o" + n + ") { o" + n + " = GetAreaLightColor(" + types[i] + ", a) + 1; SetLocalInt(a, \"nwn5e_light_orig" +
            n + "\", o" + n + "); }"
            "o" + n + " -= 1;"
            "SetAreaLightColor(" + types[i] + ", (FloatToInt(IntToFloat((o" + n + " >> 16) & 255) * f) << 16)"
            " | (FloatToInt(IntToFloat((o" + n + " >> 8) & 255) * f) << 8) | FloatToInt(IntToFloat(o" + n + " & 255) * f), a);";
  }
  if (g_config.map_tile_dim) {
    code +=
        "int w = GetAreaSize(AREA_WIDTH, a); int h = GetAreaSize(AREA_HEIGHT, a);"
        "string orig = GetLocalString(a, \"nwn5e_tile_orig\"); int have = GetStringLength(orig) == w * h * 2;"
        "string abc = \"ABCDEFGHIJKLMNOPQRSTUVWXYZ012345\"; string rec = \"\"; int x; int y;"
        "for (y = 0; y < h; y++) for (x = 0; x < w; x++) {"
        " location l = Location(a, Vector(IntToFloat(x), IntToFloat(y), 0.0), 0.0);"
        " int i = (y * w + x) * 2; int m1; int m2;"
        " if (have) { m1 = FindSubString(abc, GetSubString(orig, i, 1)); m2 = FindSubString(abc, GetSubString(orig, i + 1, 1)); }"
        " else {"
        "  m1 = GetTileMainLight1Color(l); m2 = GetTileMainLight2Color(l);"
        "  if (m1 < 0 || m1 > 31) m1 = 0; if (m2 < 0 || m2 > 31) m2 = 0;"
        "  rec += GetSubString(abc, m1, 1) + GetSubString(abc, m2, 1);"
        " }";
    if (dim) {
      // One step darker: white 3 -> 2 -> 1; in a colour's four (pale dark, dark, pale, full) pale -> pale dark, full -> dark.
      code +=
          " if (m1 <= 3) { if (m1 >= 2) m1 -= 1; } else if ((m1 - 4) % 4 >= 2) m1 -= 2;"
          " if (m2 <= 3) { if (m2 >= 2) m2 -= 1; } else if ((m2 - 4) % 4 >= 2) m2 -= 2;";
    }
    code +=
        " SetTileMainLightColor(l, m1, m2);"
        "}"
        "if (!have) SetLocalString(a, \"nwn5e_tile_orig\", rec);";
  }
  code += "RecomputeStaticLighting(a); SetLocalInt(OBJECT_SELF, \"nwn5e_map_lit\", 1);"
          "} else SetLocalInt(OBJECT_SELF, \"nwn5e_map_lit\", 0);";
  return code;
}

// The area's lights made puttable-out (see the top).
constexpr const char* kLampSetup =
    "object a = GetArea(OBJECT_SELF); int n = 0; int st = 0;"
    "object o = GetFirstObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
    "while (GetIsObjectValid(o)) {"
    " if ((GetPlaceableIllumination(o) || GetLocalInt(o, \"nwn5e_lamp\")) && !GetHasInventory(o)) {"
    "  string used = GetStringLowerCase(GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_USED));"
    "  int ours = used == \"\" || used == \"nw_02_onoff\" || used == \"x2_am_lampuse\" || used == \"n5_lamp_use\";"
    // A static placeable doesn't take the useable flag: scenery, left alone.
    "  int was = GetUseableFlag(o); SetUseableFlag(o, TRUE);"
    "  if (!GetUseableFlag(o)) st++;"
    "  else {"
    "   if (!ours && !was) SetUseableFlag(o, FALSE);"
    "   SetLocalInt(o, \"nwn5e_lamp\", 1); n++;"
    "   if (ours) SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_USED, \"n5_lamp_use\");"
    "   if (GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_DAMAGED) == \"\")"
    "    SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_DAMAGED, \"n5_lamp_hit\");"
    "   if (GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_SPELLCASTAT) == \"\")"
    "    SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_SPELLCASTAT, \"n5_lamp_spl\");"
    "  }"
    " }"
    " o = GetNextObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
    "}"
    "SetLocalInt(OBJECT_SELF, \"nwn5e_lamps\", n); SetLocalInt(OBJECT_SELF, \"nwn5e_lamps_static\", st);";

struct Arrival {
  void* area = nullptr;
  uint64_t next = 0;
};
std::unordered_map<uint32_t, Arrival> g_arrivals;  // by player character

void SetUpArea(void* creature, uint32_t id, void* area, bool force) {
  if (!force && g_lit_areas.count(area)) return;
  g_lit_areas.insert(area);
  if (g_config.light_sources) {
    Run(kLampSetup, id, "light setup");
    Log("lights: %d light sources here can be put out (%d static: scenery, left alone)",
        engine::GetLocalInt(creature, "nwn5e_lamps"), engine::GetLocalInt(creature, "nwn5e_lamps_static"));
  }
  Run(DarkenCode(g_map_light), id, "map light");
  if (engine::GetLocalInt(creature, "nwn5e_map_lit")) {
    Log("lights: the area's light at %d%% of the module's%s", g_map_light,
        g_config.map_tile_dim && g_map_light < 100 ? ", tiles' main lights a step darker" : "");
  }
}

void Update(void* creature) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  Arrival& arrival = g_arrivals[id];
  const uint64_t now = GetTickCount64();
  if (now < arrival.next) return;
  arrival.next = now + 500;
  void* area = engine::AreaOf(creature);
  if (!area || area == arrival.area) return;
  arrival.area = area;
  SetUpArea(creature, id, area, false);
}

std::vector<std::string> MapLightCommand(void* creature, const std::string& args) {
  const int percent = atoi(args.c_str());
  if (percent < 10 || percent > 100) {
    return {"nwn5e: !maplight <10-100> - the areas' light as a % of the module's (now " + std::to_string(g_map_light) +
            "%)"};
  }
  g_map_light = percent;
  WritePrivateProfileStringW(L"rules", L"map_light", std::to_wstring(percent).c_str(), ConfigPath());
  g_lit_areas.clear();  // other areas take it when entered
  void* area = engine::AreaOf(creature);
  if (area) SetUpArea(creature, Field<uint32_t>(creature, ee::kObjectId), area, true);
  return {"nwn5e: the light here is now " + std::to_string(percent) +
          "% of the module's (saved to nwn5e.ini; other areas take it when you enter them)"};
}

}  // namespace

void InstallLightSources() {
  g_map_light = g_config.map_light < 10 ? 10 : g_config.map_light > 100 ? 100 : g_config.map_light;
  if (g_config.light_sources) {
    HookExport("CVirtualMachine::RunScript (nwn5e's light scripts: put out, attacked, spells)", kRunScript,
               reinterpret_cast<void*>(&RunScript), reinterpret_cast<void**>(&g_run_script));
    HookExport("CNWSPlaceable::LoadPlaceable (a static light loads as a normal placeable)", kLoadPlaceable,
               reinterpret_cast<void*>(&LoadPlaceable), reinterpret_cast<void**>(&g_load_placeable));
  }
  if (!g_config.light_sources && g_map_light >= 100) return;
  if (AddCreatureUpdateListener(&Update)) {
    Log("lights: light sources %s; the areas' light at %d%% of the module's%s", g_config.light_sources ? "can be put out" : "as they are",
        g_map_light, g_config.map_tile_dim && g_map_light < 100 ? ", tiles' main lights a step darker" : "");
  }
  AddChatCommand("!maplight", &MapLightCommand);
}
