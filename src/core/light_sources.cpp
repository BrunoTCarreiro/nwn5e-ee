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
//
// Tile flames (0.37.0; owner: "there are smaller torches that should also be
// interactive, I can see they emit light as my character is casting shadows
// that match their position" - and "a large room with two braziers I put out,
// the only illumination left were 6 (I think) small candles ... but my
// character still showed as being 'in light'"): the torches and candles built
// into the tiles are the tiles' source lights - dummy nodes <model>sl1 / sl2 in
// the tile model, where the client spawns fx_flame01 (a flame and a 7 m light)
// while the light's colour isn't 0. On entering an area, for each tile with a
// source light on, its model (the tileset's .set: [TILE<id>] Model=) is read
// for the node's position, turned and moved into the area (GetTileOrientation /
// GetTileHeight x [GENERAL] Transition; data::TilePointInArea), and an
// invisible object (plc_invisobj) is placed there, tagged nwn5e_flame_x_y_slot:
// made a normal, usable, plot placeable as it loads (the LoadPlaceable hook,
// while nwn5e creates them), named "Flame". Used: the flame goes out (that slot's
// colour 0, the other kept; static lighting recomputed) or is lit again (its
// colour back); hit or a cold / wind spell: out; Light: lit. Doused flames are
// put out again on entering if a saved game brought their tile light back. The
// stealth light rule reads them by distance (stealth_watch.cpp) - a bright
// colour (white or pale: odd) bright within 4 m and dim within 7.5 m, a pale
// dark one (even: half as bright) dim within 4 m - in place of 0.36.0's
// "its whole tile bright, the 8 round dim". 0.37.1 (in play: "0 tile flames
// here (tic01) ... 200 lit slots with no flame node in their model"): the light
// getters take the tile's grid coordinate, but GetTileID / GetTileOrientation /
// GetTileHeight go through CNWSArea::GetTile(Vector), which finds the tile by
// metres (i x 10 <= v < (i + 1) x 10) - given grid coordinates every lookup was
// tile 0,0's. They are asked at the tile's centre in metres now.
//
// Fill lights (0.37.3; owner: "I've extinguished all the lights in this one
// corridor, but there's still a shadow being cast by my character, it looks
// like it's an invisible light source over the door" - its tile's main lights
// were 4/0 - "let's try B"): a tile's two main lights (real lights in the tile
// model) go off when every light on it and the 8 tiles round it is out - lit
// placeables, nwn5e's lamps and tile flames - and come back when one is lit
// again; a tile with no light near keeps the module's (data::TileFillLightsOff).
// After a light changes, a script lists the area's lights and the tiles now
// dark (area locals nwn5e_fill_x_y = the main lights kept, (m1 + 1) x 100 +
// m2 + 1), the decision is made here, and a second script switches the tiles
// and recomputes the static lighting once. On entering an area the dark ones
// are set dark again (a saved game may not keep tile lights), and the map
// darkening keeps them dark. `fill_follows_lights`. 0.37.6 (owner: "I see there
// are more of these invisible lights that cast shadows in the corridors, and
// possibly in other places?" - "Let's try A"): indoors and underground a tile
// keeps its fill lights only while a lit light is on it or the 8 round it - a
// room with no visible light goes dark but for the area's ambient, as the
// stealth light rule already counts it; outdoors as before. The switching runs
// in batches of 80 tiles (an interior's first entry can darken most of them),
// with one recompute at the end. 0.37.7 (owner: "I still see the shadow from
// the invisible tile lights" - those next to a lit torch, kept by A - "let's
// try A+"): indoors and underground every tile's fill lights are off; light and
// shadows come only from what can be seen (wall flames' own 7 m lights,
// braziers, lamps, light spells, carried torches).
//
// Light reach by brightness (0.38.0; owner: "we can also try the light reach
// by brightness"): as a placeable loads, its appearance's light colour
// (placeables.2da LightColor -> lightcolor.2da RED / GREEN / BLUE, the strongest)
// gives its reach (rules::LightReachByIntensity: a brazier 4 / 9 m, a candle 2 /
// 4.5 m), kept on it as locals nwn5e_light_bright / nwn5e_light_dim
// (centimetres; -1 for an appearance with no light: it lights nothing even
// "illuminated") and its zone rings' rows (nwn5e_light_rings); the stealth
// light rule reads them (stealth_watch.cpp). Scripts can't read a
// placeable's appearance, so it is done here.
//
// Torch flames rise (0.37.4; owner: "I'm using a torch and I see that the fire
// animation is set to the orientation of the torch, so a torch held sideways
// will spew flames sideways as well"): the held torch's flame (it_torch_000 /
// 001: emitter Flame02) shoots its particles along the torch at 0.15 m/s,
// carried in its frame (inherit), with no mass - so it follows the torch. As
// the served folder is built (before it is registered), those models are read
// from the game, the flame given the smoke's way - speed 0, a small negative
// mass so it floats up in the world, not carried in the torch's frame - and
// written there under their own names: on the player's machine, nothing of the
// game's kept in nwn5e. The candle holder's flame (it_torch_002) doesn't move.
// `torch_flame_up`. 0.37.5 (owner: "the lifetime of the particles needs to be
// greatly reduced and the speed slightly increased"): the particles live
// torch_flame_life_ms (500; the game's 2000) and rise at mass
// -torch_flame_rise / 100 (14; was 10) - in nwn5e.ini, read at start.
//
// Snuff / Light on the radial menu (0.39.0; owner: "go with R1, save R2 for
// barbarian part 3" - and "would it be possible with 'snuff' to maintain
// stealth if it is performed while in stealth?"): the client's radial menu is
// CRadialMenu (exported; 8 slots a ring); a placeable's (CGuiPlaceableMenu,
// its target's client id at +0x7f8) never fills slot 4, the bottom. Activate is
// hooked: when the menu is a placeable's on one of nwn5e's lights - a lamp with
// nwn5e's own use script (local nwn5e_lamp_use) or a tile flame's stand-in -
// slot 4 gets "Snuff" while it is lit, "Light" while it is out (strings
// 16000450 / 16000451; the game's is_darkness / is_light icons), with the menu's
// own Use callback copied from slot 2 (+0x138 + s x 16 handler / callback, +0x1b8
// + s x 8 param; disabled with it): the character walks over and uses it, and
// nwn5e's use script toggles it - no attack, so hiding isn't broken (Bash is an
// attack: it reveals). The server object is read directly (client id & 0x7FFFFFFF:
// a single-player game); without it, no entry. The general radial actions
// (a server transport for new choices) wait for Barbarian part 3 (R2).
// 0.39.1 (owner: "snuff doesn't work with a ranged weapon, the PC will always
// try to walk over to the light"): with a ranged weapon in hand (baseitems.2da
// RangedWeapon), Snuff takes Bash's place instead - it shoots the light from
// where it stands - through nwn5e's own callback, which marks the player
// character (local nwn5e_snuffing) before handing over to Bash's: while it is
// marked, rules_perception.cpp's SetStealthMode hook keeps it hidden through the
// attack (a real find - nwn5e_found_out - still reveals it); a hit that puts the
// light out stops the attack and clears the mark, and the stealth watch clears
// it when it stops attacking. 0.39.2 (owner: "there has to be a maximum
// distance for snuff, and maybe even not a 100% hit rate. failure would create
// noise and maybe alert guards? can't just toggle lights off in the distance
// without consequences for failure"): the ranged Snuff reaches 20 m (farther,
// the entry is greyed out), and each shot (on melee attacked: every attack,
// hit or miss, ranged too) is its own roll - d20 + Dexterity + proficiency
// against DC 13 within 10 m, 18 beyond; a 20 always puts it out, a 1 always
// misses - the engine's own hit no longer puts a snuffed light out. A miss
// clatters: hostiles within 15 m of the light come to look (2024 perception,
// heard_investigates), and the shooting goes on until a shot puts it out. The
// mark is the second it was set (lasts 30 s).
//
// Bash is snuff (0.37.0; owner: "the radial menu on light sources should also
// have an option beyond 'bash', I'm thinking 'snuff' where the character attacks
// the object until hitting it and turning off the light source. once that's
// done they should stop attacking"): a hit that puts a light or flame out also
// stops the attacker (ClearAllActions), and an attack on one already out stops
// it at once (on melee attacked: every swing, hit or miss, melee or ranged) -
// a plot light never breaks, so a Bash would otherwise go on for ever. A
// "Snuff" entry of its own needs the client's radial menu builder patched
// (its entries are fixed: Examine, Cast, Use, Trap, Bash, Lock).

#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chat.h"
#include "config.h"
#include "data/vfx_model.h"
#include "data_serving.h"
#include "rules/skills.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "script_hooks.h"

namespace {

using LoadPlaceableFn = int (*)(void* placeable, void* gff, void* gff_struct, int flag, void* tag);
LoadPlaceableFn g_load_placeable = nullptr;

constexpr const char* kLoadPlaceable = "?LoadPlaceable@CNWSPlaceable@@QEAAHPEAVCResGFF@@PEAVCResStruct@@HPEAVCExoString@@@Z";
constexpr size_t kPlaceableStatic = 0x5a0;        // int32 (LoadPlaceable: "Static")
constexpr size_t kPlaceableLightState = 0x594;    // int32 ("LightState": lit)
constexpr size_t kPlaceableHasInventory = 0x4f8;  // int32 ("HasInventory")
constexpr size_t kPlaceableAppearance = 0x32c;    // uint16 ("Appearance": placeables.2da)
int g_unstatic = 0;                               // static lights made normal placeables
constexpr size_t kPlaceableUseable = 0x264;       // int32 ("Useable")
constexpr size_t kObjectPlot = 0xfc;              // int32 (GetPlotFlag)
bool g_creating_flames = false;                   // nwn5e is placing tile flames' stand-ins now

// Light reach per placeables.2da row: bright / dim in cm (-1: no light), loaded once.
struct Reach {
  int bright = -1, dim = -1, rings = 0;
};
std::vector<Reach> g_reach_by_row;
bool g_reach_loaded = false;

constexpr float kLightRingRadii[] = {2.5f, 4.0f, 5.0f, 7.5f, 9.0f, 10.0f, 15.0f, 20.0f};  // tools/data/stealth_rings.py
int NearestLightRing(float metres) {
  int best = 0;
  for (int i = 1; i < 8; ++i) {
    if (fabsf(kLightRingRadii[i] - metres) < fabsf(kLightRingRadii[best] - metres)) best = i;
  }
  return best;
}

const std::vector<Reach>& ReachByRow() {
  if (g_reach_loaded) return g_reach_by_row;
  g_reach_loaded = true;
  const std::vector<std::string> colour = GameTableColumn("placeables", "LightColor");
  const std::vector<std::string> red = GameTableColumn("lightcolor", "RED");
  const std::vector<std::string> green = GameTableColumn("lightcolor", "GREEN");
  const std::vector<std::string> blue = GameTableColumn("lightcolor", "BLUE");
  g_reach_by_row.resize(colour.size());
  int lights = 0;
  for (size_t row = 0; row < colour.size(); ++row) {
    if (colour[row].empty()) continue;
    const size_t c = static_cast<size_t>(atoi(colour[row].c_str()));
    if (c >= red.size() || c >= green.size() || c >= blue.size()) continue;
    const float intensity = (std::max)({static_cast<float>(atof(red[c].c_str())), static_cast<float>(atof(green[c].c_str())),
                                        static_cast<float>(atof(blue[c].c_str()))});
    float bright = 0.0f, dim = 0.0f;
    if (!rules::LightReachByIntensity(intensity, &bright, &dim)) continue;
    Reach& r = g_reach_by_row[row];
    r.bright = static_cast<int>(bright * 100.0f + 0.5f);
    r.dim = static_cast<int>(dim * 100.0f + 0.5f);
    r.rings = (1820 + NearestLightRing(bright)) * 10000 + 1828 + NearestLightRing(dim);
    ++lights;
  }
  Log("lights: %d placeable appearances give light; their reach by their colour's brightness (%zu lightcolor rows)", lights,
      red.size());
  return g_reach_by_row;
}

int LoadPlaceable(void* placeable, void* gff, void* gff_struct, int flag, void* tag) {
  const int result = g_load_placeable(placeable, gff, gff_struct, flag, tag);
  if (result && placeable && !g_creating_flames) {
    const std::vector<Reach>& reach = ReachByRow();
    const size_t row = Field<uint16_t>(placeable, kPlaceableAppearance);
    const Reach r = row < reach.size() ? reach[row] : Reach{};
    if (r.bright > 0) {  // only on lights: no locals on the rest
      engine::SetLocalInt(placeable, "nwn5e_light_bright", r.bright);
      engine::SetLocalInt(placeable, "nwn5e_light_dim", r.dim);
      engine::SetLocalInt(placeable, "nwn5e_light_rings", r.rings);
    } else if (Field<int32_t>(placeable, kPlaceableLightState) != 0) {
      engine::SetLocalInt(placeable, "nwn5e_light_bright", -1);  // "lit" but its appearance gives no light
    }
  }
  if (result && placeable && g_creating_flames) {  // a tile flame's stand-in: clickable, never broken
    SetField<int32_t>(placeable, kPlaceableStatic, 0);
    SetField<int32_t>(placeable, kPlaceableUseable, 1);
    SetField<int32_t>(placeable, kObjectPlot, 1);
    return result;
  }
  if (result && placeable && g_config.light_sources && Field<int32_t>(placeable, kPlaceableStatic) == 1 &&
      Field<int32_t>(placeable, kPlaceableLightState) != 0 && Field<int32_t>(placeable, kPlaceableHasInventory) == 0) {
    SetField<int32_t>(placeable, kPlaceableStatic, 0);
    if (++g_unstatic <= 5 || g_unstatic % 50 == 0) {
      Log("lights: a static light (placeables.2da row %d) loaded as a normal placeable, so it can be put out (%d so far)",
          Field<uint16_t>(placeable, kPlaceableAppearance), g_unstatic);
    }
  }
  return result;
}

constexpr const char* kUseScript = "n5_lamp_use";
constexpr const char* kHitScript = "n5_lamp_hit";
constexpr const char* kSpellScript = "n5_lamp_spl";
constexpr const char* kAttackScript = "n5_lamp_atk";
constexpr const char* kFlameAttackScript = "n5_flame_atk";
constexpr const char* kFlameUseScript = "n5_flame_use";
constexpr const char* kFlameHitScript = "n5_flame_hit";
constexpr const char* kFlameSpellScript = "n5_flame_spl";

// The light out / lit again, on OBJECT_SELF (the placeable).
constexpr const char* kDouse =
    "PlayAnimation(ANIMATION_PLACEABLE_DEACTIVATE); SetPlaceableIllumination(OBJECT_SELF, FALSE);"
    "SetLocalInt(OBJECT_SELF, \"nwn5e_lamp_out\", 1); SetLocalInt(OBJECT_SELF, \"nwn5e_light_changed\", 1);";
constexpr const char* kRelight =
    "PlayAnimation(ANIMATION_PLACEABLE_ACTIVATE); SetPlaceableIllumination(OBJECT_SELF, TRUE);"
    "DeleteLocalInt(OBJECT_SELF, \"nwn5e_lamp_out\"); SetLocalInt(OBJECT_SELF, \"nwn5e_light_changed\", 1);";

// The spells that put a flame out (cold, water, wind).
constexpr const char* kDousingSpells =
    "sp == SPELL_RAY_OF_FROST || sp == SPELL_ICE_DAGGER || sp == SPELL_GUST_OF_WIND || sp == SPELL_CONE_OF_COLD"
    " || sp == SPELL_ICE_STORM";

bool Investigates() {
  return g_config.heard_investigates && g_config.perception == rules::RuleVariant::kFaithful;
}

// L3: hostiles to `u` within `radius` m (with a line to the light when `sight`) come to look.
std::string NoticeCode(float radius = 20.0f, bool sight = true, const char* why = "saw the light go out") {
  if (!Investigates()) return "";
  const std::string r = std::to_string(radius), los = sight ? "TRUE" : "FALSE";
  return "location at = GetLocation(OBJECT_SELF); int came = 0;"
         "object o = GetFirstObjectInShape(SHAPE_SPHERE, " + r + ", at, " + los + ", OBJECT_TYPE_CREATURE);"
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
         "  if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: \" + GetName(o) + \" " + std::string(why) + " and comes to look\");"
         " }"
         " o = GetNextObjectInShape(SHAPE_SPHERE, " + r + ", at, " + los + ", OBJECT_TYPE_CREATURE);"
         "}"
         "SetLocalInt(OBJECT_SELF, \"nwn5e_lamp_came\", came);";
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0) Log("lights: the %s script failed (%d)", what, result);
}

// --- Fill lights (see the top).
using GetGameObjectFn = void* (*)(void* app, uint32_t id);
using CastFn = void* (*)(void* object);

// Any object of the server by id (CNWSObject), null if none.
void* ObjectById(uint32_t id) {
  static const auto get = reinterpret_cast<GetGameObjectFn>(EngineExport(ee::kGetGameObject));
  static void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!get || !app_manager || !*app_manager) return nullptr;
  void* app = Field<void*>(*app_manager, ee::kAppManagerServerApp);
  void* object = app ? get(app, id) : nullptr;
  if (!object) return nullptr;
  const auto cast = reinterpret_cast<CastFn>(reinterpret_cast<void* const*>(Field<void*>(object, 0))[ee::kVtableAsNwsObject / 8]);
  return cast ? cast(object) : nullptr;
}

constexpr const char* kFillList =
    "object a = GetArea(OBJECT_SELF); int w = GetAreaSize(AREA_WIDTH, a); int h = GetAreaSize(AREA_HEIGHT, a);"
    "string out = IntToString(w) + \" \" + IntToString(h) + \" \""
    " + IntToString(GetIsAreaInterior(a) || !GetIsAreaAboveGround(a)) + \"|\";"
    "object o = GetFirstObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
    "while (GetIsObjectValid(o)) {"
    " if (GetStringLeft(GetTag(o), 11) == \"nwn5e_flame\")"
    "  out += IntToString(GetLocalInt(o, \"nwn5e_torch_x\")) + \" \" + IntToString(GetLocalInt(o, \"nwn5e_torch_y\")) + \" \""
    "   + (GetLocalInt(o, \"nwn5e_torch_out\") ? \"0\" : \"1\") + \";\";"
    " else if (GetLocalInt(o, \"nwn5e_lamp\") || GetPlaceableIllumination(o)) {"
    "  vector v = GetPosition(o);"
    "  out += IntToString(FloatToInt(v.x / 10.0)) + \" \" + IntToString(FloatToInt(v.y / 10.0)) + \" \""
    "   + (GetPlaceableIllumination(o) ? \"1\" : \"0\") + \";\";"
    " }"
    " o = GetNextObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
    "}"
    "out += \"|\"; int x; int y;"
    "for (y = 0; y < h; y++) for (x = 0; x < w; x++)"
    " if (GetLocalInt(a, \"nwn5e_fill_\" + IntToString(x) + \"_\" + IntToString(y))) out += IntToString(x) + \" \" + IntToString(y) + \";\";"
    "SetLocalString(OBJECT_SELF, \"nwn5e_fill_list\", out);";

// The tiles round the lights of OBJECT_SELF's area switched; `all` sets every
// dark tile dark again (on entering), else only the changes. Static lighting
// recomputed either way (the douse / relight scripts leave it to this).
void FillLights(uint32_t self, bool all) {
  if (!g_config.fill_follows_lights) {
    Run("RecomputeStaticLighting(GetArea(OBJECT_SELF));", self, "lighting");
    return;
  }
  Run(kFillList, self, "fill lights listed");
  void* object = ObjectById(self);
  const std::string list = object ? engine::GetLocalString(object, "nwn5e_fill_list") : "";
  const size_t bar1 = list.find('|'), bar2 = bar1 == std::string::npos ? std::string::npos : list.find('|', bar1 + 1);
  int w = 0, h = 0, strict = 0;
  if (bar2 == std::string::npos || sscanf_s(list.c_str(), "%d %d %d", &w, &h, &strict) != 3 || w <= 0 || h <= 0) {
    Run("RecomputeStaticLighting(GetArea(OBJECT_SELF));", self, "lighting");
    return;
  }
  std::vector<data::TileLight> lights;
  for (size_t at = bar1 + 1; at < bar2;) {
    const size_t end = list.find(';', at);
    if (end == std::string::npos || end > bar2) break;
    data::TileLight light;
    int lit = 0;
    if (sscanf_s(list.c_str() + at, "%d %d %d", &light.x, &light.y, &lit) == 3) {
      light.lit = lit != 0;
      lights.push_back(light);
    }
    at = end + 1;
  }
  std::vector<uint8_t> dark_now(static_cast<size_t>(w) * h, 0);
  for (size_t at = bar2 + 1; at < list.size();) {
    const size_t end = list.find(';', at);
    if (end == std::string::npos) break;
    int x = 0, y = 0;
    if (sscanf_s(list.c_str() + at, "%d %d", &x, &y) == 2 && x >= 0 && y >= 0 && x < w && y < h) {
      dark_now[static_cast<size_t>(y) * w + x] = 1;
    }
    at = end + 1;
  }
  std::vector<uint8_t> dark = data::TileFillLightsOff(w, h, lights, strict != 0);
  if (strict) std::fill(dark.begin(), dark.end(), static_cast<uint8_t>(1));  // indoors: no fill lights at all (A+)
  std::vector<std::string> batches(1);
  int offs = 0, ons = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * w + x;
      if (i >= dark.size()) continue;
      const std::string key = "\"nwn5e_fill_" + std::to_string(x) + "_" + std::to_string(y) + "\"";
      const std::string at = "l = Location(a, Vector(" + std::to_string(x) + ".0, " + std::to_string(y) + ".0, 0.0), 0.0);";
      std::string tile;
      if (dark[i] && (all || !dark_now[i])) {
        tile = at + "if (!GetLocalInt(a, " + key + ")) SetLocalInt(a, " + key +
               ", (GetTileMainLight1Color(l) + 1) * 100 + GetTileMainLight2Color(l) + 1); SetTileMainLightColor(l, 0, 0);";
        ++offs;
      } else if (!dark[i] && dark_now[i]) {
        tile = at + "v = GetLocalInt(a, " + key + "); if (v) { SetTileMainLightColor(l, v / 100 - 1, v % 100 - 1); DeleteLocalInt(a, " +
               key + "); }";
        ++ons;
      }
      if (tile.empty()) continue;
      batches.back() += tile;
      if ((offs + ons) % 80 == 0) batches.emplace_back();
    }
  }
  batches.back() += "RecomputeStaticLighting(a);";
  for (const std::string& batch : batches) {
    if (!batch.empty()) Run("object a = GetArea(OBJECT_SELF); location l; string k; int v;" + batch, self, "fill lights");
  }
  if (offs || ons) {
    Log("lights: fill lights - %d tiles dark, %d lit again (%zu lights in the area%s)", offs, ons, lights.size(),
        strict ? "; indoors: no fill lights" : "");
  }
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
  FillLights(lamp, false);
  Log("lights: 0x%08X used", lamp);
}

void Damaged(uint32_t lamp) {
  Run("object u = GetLastDamager(); int all = GetTotalDamageDealt(); int fire = GetDamageDealtByType(DAMAGE_TYPE_FIRE);"
      "if (fire < 0) fire = 0;"
      "if (GetCurrentHitPoints(OBJECT_SELF) < GetMaxHitPoints(OBJECT_SELF))"
      " ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectHeal(GetMaxHitPoints(OBJECT_SELF)), OBJECT_SELF);"
      "if (GetPlaceableIllumination(OBJECT_SELF) && (all <= 0 || all > fire) && !GetLocalInt(u, \"nwn5e_snuffing\")) {" +
          std::string(kDouse) +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: you hit \" + GetName(OBJECT_SELF) + \" and put it out\");"
          " AssignCommand(u, ClearAllActions()); DeleteLocalInt(u, \"nwn5e_snuffing\");" +
          NoticeCode() + "}",
      lamp, "light hit");
  FillLights(lamp, false);
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
  FillLights(lamp, false);
  Log("lights: 0x%08X a spell cast at it", lamp);
}

// A tile flame's stand-in (OBJECT_SELF): its tile slot put out or lit again.
constexpr const char* kFlameState =
    "object a = GetArea(OBJECT_SELF);"
    "location tl = Location(a, Vector(IntToFloat(GetLocalInt(OBJECT_SELF, \"nwn5e_torch_x\")),"
    " IntToFloat(GetLocalInt(OBJECT_SELF, \"nwn5e_torch_y\")), 0.0), 0.0);"
    "int k = GetLocalInt(OBJECT_SELF, \"nwn5e_torch_slot\");"
    "int c1 = GetTileSourceLight1Color(tl); int c2 = GetTileSourceLight2Color(tl); int c = k == 1 ? c1 : c2;";
constexpr const char* kFlameOut =
    "SetLocalInt(OBJECT_SELF, \"nwn5e_torch_colour\", c); SetLocalInt(OBJECT_SELF, \"nwn5e_torch_out\", 1);"
    "SetTileSourceLightColor(tl, k == 1 ? 0 : c1, k == 2 ? 0 : c2); SetLocalInt(OBJECT_SELF, \"nwn5e_light_changed\", 1);";
constexpr const char* kFlameLit =
    "int r = GetLocalInt(OBJECT_SELF, \"nwn5e_torch_colour\"); if (r <= 0) r = 1;"
    "DeleteLocalInt(OBJECT_SELF, \"nwn5e_torch_out\");"
    "SetTileSourceLightColor(tl, k == 1 ? r : c1, k == 2 ? r : c2); SetLocalInt(OBJECT_SELF, \"nwn5e_light_changed\", 1);";

void FlameUsed(uint32_t flame) {
  Run(std::string("object u = GetLastUsedBy();") + kFlameState +
          "if (c > 0) {" + kFlameOut + " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: you put out the flame\");" +
          NoticeCode() + "} else {" + kFlameLit + " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: you light the flame\"); }",
      flame, "flame used");
  FillLights(flame, false);
  Log("lights: tile flame 0x%08X used", flame);
}

void FlameHit(uint32_t flame) {
  Run(std::string("object u = GetLastDamager(); int all = GetTotalDamageDealt();"
                  "int fire = GetDamageDealtByType(DAMAGE_TYPE_FIRE); if (fire < 0) fire = 0;") +
          kFlameState + "if (c > 0 && (all <= 0 || all > fire) && !GetLocalInt(u, \"nwn5e_snuffing\")) {" + kFlameOut +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: you hit the flame and put it out\");"
          " AssignCommand(u, ClearAllActions()); DeleteLocalInt(u, \"nwn5e_snuffing\");" + NoticeCode() + "}",
      flame, "flame hit");
  FillLights(flame, false);
  Log("lights: tile flame 0x%08X hit", flame);
}

void FlameSpell(uint32_t flame) {
  Run(std::string("object u = GetLastSpellCaster(); int sp = GetLastSpell();") + kFlameState + "if (c > 0 && (" +
          kDousingSpells + ")) {" + kFlameOut +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: your spell puts out the flame\");" + NoticeCode() +
          "} else if (c <= 0 && sp == SPELL_LIGHT) {" + kFlameLit +
          " if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: your spell lights the flame\"); }",
      flame, "spell at a flame");
  FillLights(flame, false);
  Log("lights: tile flame 0x%08X a spell cast at it", flame);
}

// An attack on a light or flame: one already out stops the attacker (Bash is
// snuff); a ranged Snuff's shot is its own roll (see the top).
void AttackedOut(uint32_t target, bool flame) {
  const std::string lit = flame ? "c > 0" : "GetPlaceableIllumination(OBJECT_SELF)";
  Run(std::string("object u = GetLastAttacker();") + (flame ? kFlameState : "") +
          "if (!(" + lit + ")) { AssignCommand(u, ClearAllActions()); DeleteLocalInt(u, \"nwn5e_snuffing\"); }"
          "else if (GetLocalInt(u, \"nwn5e_snuffing\")) {"
          " float sd = GetDistanceBetween(u, OBJECT_SELF); int dc = sd <= 10.0 ? 13 : 18;"
          " int r = d20(); int b = GetAbilityModifier(ABILITY_DEXTERITY, u) + 2 + (GetHitDice(u) - 1) / 4; int t = r + b;"
          " string how = \" (d20 \" + IntToString(r) + (b >= 0 ? \" +\" : \" \") + IntToString(b) + \" = \" + IntToString(t)"
          "  + \" vs DC \" + IntToString(dc) + \")\";"
          " if (r == 20 || (r != 1 && t >= dc)) {" +
          (flame ? std::string(kFlameOut) : std::string(kDouse)) +
          "  if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: your shot puts out \" + GetName(OBJECT_SELF) + how);"
          "  AssignCommand(u, ClearAllActions()); DeleteLocalInt(u, \"nwn5e_snuffing\");" +
          NoticeCode() +
          " } else {"
          "  if (GetIsPC(u)) SendMessageToPC(u, \"nwn5e: your shot misses \" + GetName(OBJECT_SELF) + how + \" - the clatter carries\");" +
          NoticeCode(15.0f, false, "heard the clatter") +
          " }"
          "}",
      target, "light attacked");
  void* object = ObjectById(target);
  if (object && engine::GetLocalInt(object, "nwn5e_light_changed")) {
    engine::SetLocalInt(object, "nwn5e_light_changed", 0);
    FillLights(target, false);
  }
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
        " string fk = \"nwn5e_fill_\" + IntToString(x) + \"_\" + IntToString(y);"
        " if (GetLocalInt(a, fk)) { SetLocalInt(a, fk, (m1 + 1) * 100 + m2 + 1); m1 = 0; m2 = 0; }"  // dark: kept dark
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
    "object a = GetArea(OBJECT_SELF); int n = 0; int st = 0; string skip = \"\";"
    "object o = GetFirstObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
    "while (GetIsObjectValid(o)) {"
    " if ((GetPlaceableIllumination(o) || GetLocalInt(o, \"nwn5e_lamp\")) && GetHasInventory(o) && GetStringLength(skip) < 600)"
    "  skip += GetName(o) + \" (has an inventory); \";"
    " if ((GetPlaceableIllumination(o) || GetLocalInt(o, \"nwn5e_lamp\")) && !GetHasInventory(o)) {"
    "  string used = GetStringLowerCase(GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_USED));"
    "  int ours = used == \"\" || used == \"nw_02_onoff\" || used == \"x2_am_lampuse\" || used == \"n5_lamp_use\";"
    // A static placeable doesn't take the useable flag: scenery, left alone.
    "  int was = GetUseableFlag(o); SetUseableFlag(o, TRUE);"
    "  if (!GetUseableFlag(o)) { st++; if (GetStringLength(skip) < 600) skip += GetName(o) + \" (static); \"; }"
    "  else {"
    "   if (!ours && !was) SetUseableFlag(o, FALSE);"
    "   if (!ours && GetStringLength(skip) < 600) skip += GetName(o) + \" (its own on-used script \" + used + \": hits and spells only); \";"
    "   SetLocalInt(o, \"nwn5e_lamp\", 1); n++;"
    "   if (ours) { SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_USED, \"n5_lamp_use\"); SetLocalInt(o, \"nwn5e_lamp_use\", 1); }"
    "   if (GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_DAMAGED) == \"\")"
    "    SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_DAMAGED, \"n5_lamp_hit\");"
    "   if (GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_MELEEATTACKED) == \"\")"
    "    SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_MELEEATTACKED, \"n5_lamp_atk\");"
    "   if (GetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_SPELLCASTAT) == \"\")"
    "    SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_SPELLCASTAT, \"n5_lamp_spl\");"
    "  }"
    " }"
    " o = GetNextObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
    "}"
    "SetLocalInt(OBJECT_SELF, \"nwn5e_lamps\", n); SetLocalInt(OBJECT_SELF, \"nwn5e_lamps_static\", st);"
    "SetLocalString(OBJECT_SELF, \"nwn5e_lamps_skipped\", skip);";

// --- Tile flames: where they are (see the top).
constexpr uint16_t kResTypeSet = 2013;
struct ModelFlames {
  bool has[2] = {false, false};
  float local[2][3] = {};
};
std::unordered_map<std::string, std::string> g_set_texts;  // tileset -> its .set ("" when unreadable)
std::unordered_map<std::string, ModelFlames> g_model_flames;  // tile model -> its source light nodes

const std::string& SetText(const std::string& tileset) {
  auto it = g_set_texts.find(tileset);
  if (it != g_set_texts.end()) return it->second;
  std::string bytes, why;
  if (!ReadGameResource(tileset, kResTypeSet, &bytes, &why)) {
    Log("lights: the tileset %s couldn't be read (%s) - its tile flames aren't placed", tileset.c_str(), why.c_str());
    bytes.clear();
  }
  return g_set_texts.emplace(tileset, bytes).first->second;
}

const ModelFlames& FlamesOf(const std::string& model) {
  auto it = g_model_flames.find(model);
  if (it != g_model_flames.end()) return it->second;
  ModelFlames flames;
  std::string bytes, why;
  if (!model.empty() && ReadGameResource(model, ee::kResTypeMdl, &bytes, &why)) {
    for (int k = 0; k < 2; ++k) {
      flames.has[k] = data::ReadModelNodePosition(bytes, model + "sl" + std::to_string(k + 1), flames.local[k]);
    }
  }
  return g_model_flames.emplace(model, flames).first->second;
}

std::string Num(float value) {
  char text[32];
  sprintf_s(text, "%.3f", value);
  return text;
}

bool FlamesWanted() {
  return g_config.light_sources ||
         (g_config.stealth_light && g_config.perception == rules::RuleVariant::kFaithful);
}

// The area's tile flames found (lit tiles listed by a script), stand-ins placed for the new ones.
void PlaceFlames(void* creature, uint32_t pc) {
  Run("object a = GetArea(OBJECT_SELF); int w = GetAreaSize(AREA_WIDTH, a); int h = GetAreaSize(AREA_HEIGHT, a);"
      // Doused flames stay out (a saved game may not keep the tiles' lights).
      "int fix = 0; object o = GetFirstObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
      "while (GetIsObjectValid(o)) {"
      " if (GetStringLeft(GetTag(o), 11) == \"nwn5e_flame\" && GetLocalInt(o, \"nwn5e_torch_out\")) {"
      "  location fl = Location(a, Vector(IntToFloat(GetLocalInt(o, \"nwn5e_torch_x\")), IntToFloat(GetLocalInt(o, \"nwn5e_torch_y\")), 0.0), 0.0);"
      "  int fk = GetLocalInt(o, \"nwn5e_torch_slot\"); int f1 = GetTileSourceLight1Color(fl); int f2 = GetTileSourceLight2Color(fl);"
      "  if ((fk == 1 ? f1 : f2) > 0) { SetTileSourceLightColor(fl, fk == 1 ? 0 : f1, fk == 2 ? 0 : f2); fix++; }"
      " }"
      " o = GetNextObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
      "}"
      "if (fix) RecomputeStaticLighting(a);"
      "string out = GetTilesetResRef(a) + \"|\"; int x; int y;"
      "for (y = 0; y < h; y++) for (x = 0; x < w; x++) {"
      " location l = Location(a, Vector(IntToFloat(x), IntToFloat(y), 0.0), 0.0);"
      " int s1 = GetTileSourceLight1Color(l); int s2 = GetTileSourceLight2Color(l);"
      // The tile's model, turn and height: by metres (its centre), unlike the light getters' grid coordinate.
      " location wl = Location(a, Vector(IntToFloat(x) * 10.0 + 5.0, IntToFloat(y) * 10.0 + 5.0, 0.0), 0.0);"
      " if (s1 > 0 || s2 > 0) out += IntToString(x) + \" \" + IntToString(y) + \" \" + IntToString(GetTileID(wl)) + \" \""
      "  + IntToString(GetTileOrientation(wl)) + \" \" + IntToString(GetTileHeight(wl)) + \" \" + IntToString(s1) + \" \" + IntToString(s2) + \";\";"
      "}"
      "SetLocalString(OBJECT_SELF, \"nwn5e_flame_tiles\", out);",
      pc, "tile flames listed");
  const std::string list = engine::GetLocalString(creature, "nwn5e_flame_tiles");
  const size_t bar = list.find('|');
  if (bar == std::string::npos) return;
  const std::string tileset = list.substr(0, bar);
  const std::string& set = SetText(tileset);
  if (set.empty()) return;
  const float transition = data::TilesetTransition(set);
  std::vector<std::string> batches(1);
  int count = 0, missing = 0;
  size_t at = bar + 1;
  while (at < list.size()) {
    const size_t end = list.find(';', at);
    if (end == std::string::npos) break;
    int x = 0, y = 0, id = 0, turn = 0, height = 0, colour[2] = {0, 0};
    if (sscanf_s(list.c_str() + at, "%d %d %d %d %d %d %d", &x, &y, &id, &turn, &height, &colour[0], &colour[1]) == 7) {
      const ModelFlames& flames = FlamesOf(data::TilesetTileModel(set, id));
      for (int k = 0; k < 2; ++k) {
        if (colour[k] <= 0) continue;
        if (!flames.has[k]) {
          ++missing;
          continue;
        }
        float p[3];
        data::TilePointInArea(flames.local[k], x, y, turn, height, transition, p);
        const std::string tag = "nwn5e_flame_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(k + 1);
        std::string& code = batches.back();
        code += "if (!GetIsObjectValid(GetNearestObjectByTag(\"" + tag + "\", OBJECT_SELF))) {"
                " o = CreateObject(OBJECT_TYPE_PLACEABLE, \"plc_invisobj\", Location(a, Vector(" + Num(p[0]) + ", " +
                Num(p[1]) + ", " + Num(p[2]) + "), 0.0), FALSE, \"" + tag + "\");"
                " SetLocalInt(o, \"nwn5e_torch_x\", " + std::to_string(x) + "); SetLocalInt(o, \"nwn5e_torch_y\", " +
                std::to_string(y) + "); SetLocalInt(o, \"nwn5e_torch_slot\", " + std::to_string(k + 1) + ");"
                " SetLocalInt(o, \"nwn5e_torch_colour\", " + std::to_string(colour[k]) + ");"
                " SetLocalFloat(o, \"nwn5e_torch_h\", " + Num(flames.local[k][2]) + "); SetName(o, \"Flame\");";
        if (g_config.light_sources) {
          code += " SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_USED, \"n5_flame_use\");"
                  " SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_DAMAGED, \"n5_flame_hit\");"
                  " SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_MELEEATTACKED, \"n5_flame_atk\");"
                  " SetEventScript(o, EVENT_SCRIPT_PLACEABLE_ON_SPELLCASTAT, \"n5_flame_spl\");";
        }
        code += " n++; }";
        ++count;
        if (count % 16 == 0) batches.emplace_back();
      }
    }
    at = end + 1;
  }
  int placed = 0;
  for (const std::string& batch : batches) {
    if (batch.empty()) continue;
    g_creating_flames = true;
    Run("object a = GetArea(OBJECT_SELF); object o; int n = 0;" + batch + "SetLocalInt(OBJECT_SELF, \"nwn5e_flames_new\", n);",
        pc, "tile flames placed");
    g_creating_flames = false;
    placed += engine::GetLocalInt(creature, "nwn5e_flames_new");
  }
  Log("lights: %d tile flames here (%s), %d new stand-ins placed%s", count, tileset.c_str(), placed,
      missing ? (" - " + std::to_string(missing) + " lit slots with no flame node in their model").c_str() : "");
}

uint32_t g_last_pc = 0;  // the player character (a single-player game)

struct Arrival {
  void* area = nullptr;
  uint64_t next = 0;
};
std::unordered_map<uint32_t, Arrival> g_arrivals;  // by player character

void SetUpArea(void* creature, uint32_t id, void* area, bool force) {
  if (!force && g_lit_areas.count(area)) return;
  g_lit_areas.insert(area);
  if (FlamesWanted()) PlaceFlames(creature, id);
  if (g_config.light_sources) {
    Run(kLampSetup, id, "light setup");
    Log("lights: %d light sources here can be put out (%d static: scenery, left alone)",
        engine::GetLocalInt(creature, "nwn5e_lamps"), engine::GetLocalInt(creature, "nwn5e_lamps_static"));
    const std::string skipped = engine::GetLocalString(creature, "nwn5e_lamps_skipped");
    if (!skipped.empty()) Log("lights: not clickable here: %s", skipped.c_str());
  }
  Run(DarkenCode(g_map_light), id, "map light");
  if (g_config.light_sources) FillLights(id, true);
  if (engine::GetLocalInt(creature, "nwn5e_map_lit")) {
    Log("lights: the area's light at %d%% of the module's%s", g_map_light,
        g_config.map_tile_dim && g_map_light < 100 ? ", tiles' main lights a step darker" : "");
  }
}

void Update(void* creature) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  g_last_pc = id;
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

// data_serving's BuildServedTables, before the served folder is registered (see the top).
void GenerateTorchFlames(const std::wstring& served) {
  if (!g_config.torch_flame_up) return;
  int written = 0;
  for (const char* model : {"it_torch_000", "it_torch_001"}) {
    std::string bytes, why;
    if (!ReadGameResource(model, ee::kResTypeMdl, &bytes, &why)) {
      Log("lights: the torch model %s couldn't be read (%s)", model, why.c_str());
      continue;
    }
    const float life = static_cast<float>(g_config.torch_flame_life_ms) / 1000.0f;   // 144: life (s)
    const float mass = -static_cast<float>(g_config.torch_flame_rise) / 100.0f;      // 148: mass (< 0 rises)
    const int nodes = data::PatchEmitter(&bytes, "Flame02", {{192, 0.0f}, {148, mass}, {144, life}}, 0x40);
    if (nodes == 0) {
      Log("lights: no Flame02 emitter in %s - left as it is", model);
      continue;
    }
    const std::wstring path = served + L"\\" + std::wstring(model, model + strlen(model)) + L".mdl";
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file) {
      fwrite(bytes.data(), 1, bytes.size(), file);
      fclose(file);
      ++written;
    }
  }
  if (written) {
    Log("lights: %d torch models served with their flame rising, whatever the torch's angle (life %d ms, rise %d)",
        written, g_config.torch_flame_life_ms, g_config.torch_flame_rise);
  }
}

// --- Snuff / Light on the radial menu (see the top).
using ActivateFn = void (*)(void* menu);
using SetEntryFn = void (*)(void* menu, int slot, const char* icon, uint32_t strref, void* handler, void* callback,
                            int64_t param);
using DisableEntryFn = void (*)(void* menu, int slot);
ActivateFn g_activate = nullptr;
SetEntryFn g_set_entry = nullptr;
DisableEntryFn g_disable_entry = nullptr;
uintptr_t g_placeable_menu_vtable = 0;
constexpr const char* kRadialActivate = "?Activate@CRadialMenu@@QEAAXXZ";
constexpr const char* kRadialSetEntry = "?SetEntryx@CRadialMenu@@QEAAXHPEBDIPEAXP8CCallbackHandlerBase@@EAAX_J@Z2@Z";
constexpr const char* kRadialDisableEntry = "?DisableEntry@CRadialMenu@@QEAAXH@Z";
constexpr const char* kRttiPlaceableMenu = ".?AVCGuiPlaceableMenu@@";
constexpr size_t kMenuCallbacks = 0x138;  // {handler, callback} x 16 a slot
constexpr size_t kMenuParams = 0x1b8;     // int64 a slot
constexpr size_t kMenuFlags = 0x1f8;      // uint32 a slot: bit 0 disabled
constexpr size_t kMenuIcons = 0x6e0;      // CGuiIcon* a slot (null: free)
constexpr size_t kMenuTarget = 0x7f8;     // the placeable's client id
constexpr int kSlotUse = 2, kSlotFree = 4;
constexpr uint32_t kStrSnuff = 16000450, kStrLight = 16000451;

constexpr int kSlotBash = 5;
using CallbackFn = void (*)(void* handler, int64_t param);
CallbackFn g_bash_callback = nullptr;  // the menu's Bash callback, for the ranged Snuff

// Whether the creature holds a ranged weapon (baseitems.2da RangedWeapon).
bool HoldsRangedWeapon(void* creature) {
  static const std::vector<std::string> ranged = GameTableColumn("baseitems", "RangedWeapon");
  const int row = creature ? engine::BaseItemRowInSlot(creature, ee::kSlotRightHand) : -1;
  return row >= 0 && static_cast<size_t>(row) < ranged.size() && !ranged[row].empty();
}

// The ranged Snuff: the player character marked (kept hidden while it shoots), then Bash's own callback.
void SnuffRanged(void* handler, int64_t param) {
  if (void* pc = g_last_pc ? CreatureById(g_last_pc) : nullptr) {
    engine::SetLocalInt(pc, "nwn5e_snuffing", static_cast<int>((GetTickCount64() / 1000) & 0x7FFFFFFF));
    Log("lights: 0x%08X snuffs a light from range - kept hidden while it shoots", g_last_pc);
  }
  if (g_bash_callback) g_bash_callback(handler, param);
}

void AddLightEntry(void* menu) {
  if (Field<void*>(menu, kMenuIcons + kSlotFree * 8) || !Field<void*>(menu, kMenuIcons + kSlotUse * 8)) return;
  void* object = ObjectById(Field<uint32_t>(menu, kMenuTarget) & 0x7FFFFFFFu);
  if (!object) return;
  const bool flame = engine::GetLocalInt(object, "nwn5e_torch_slot") > 0;
  if (!flame && !engine::GetLocalInt(object, "nwn5e_lamp_use")) return;  // not one nwn5e's use script toggles
  const bool lit = flame ? engine::GetLocalInt(object, "nwn5e_torch_out") == 0
                         : Field<int32_t>(object, kPlaceableLightState) != 0;
  void* handler = Field<void*>(menu, kMenuCallbacks + kSlotUse * 16);
  void* callback = Field<void*>(menu, kMenuCallbacks + kSlotUse * 16 + 8);
  int64_t param = Field<int64_t>(menu, kMenuParams + kSlotUse * 8);
  // With a ranged weapon, Snuff shoots it (Bash's action, through SnuffRanged) instead of walking over.
  void* pc = g_last_pc ? CreatureById(g_last_pc) : nullptr;
  if (lit && Field<void*>(menu, kMenuIcons + kSlotBash * 8) && HoldsRangedWeapon(pc)) {
    handler = Field<void*>(menu, kMenuCallbacks + kSlotBash * 16);
    g_bash_callback = reinterpret_cast<CallbackFn>(Field<void*>(menu, kMenuCallbacks + kSlotBash * 16 + 8));
    callback = reinterpret_cast<void*>(&SnuffRanged);
    param = Field<int64_t>(menu, kMenuParams + kSlotBash * 8);
  }
  g_set_entry(menu, kSlotFree, lit ? "is_darkness" : "is_light", lit ? kStrSnuff : kStrLight, handler, callback, param);
  bool disabled = (Field<uint32_t>(menu, kMenuFlags + kSlotUse * 4) & 1) != 0;
  if (callback == reinterpret_cast<void*>(&SnuffRanged) && pc) {  // a ranged Snuff reaches 20 m
    const float dx = Field<float>(object, ee::kObjectPositionX) - Field<float>(pc, ee::kObjectPositionX);
    const float dy = Field<float>(object, ee::kObjectPositionY) - Field<float>(pc, ee::kObjectPositionY);
    disabled = dx * dx + dy * dy > 20.0f * 20.0f;
  }
  if (disabled && g_disable_entry) g_disable_entry(menu, kSlotFree);
}

void RadialActivate(void* menu) {
  if (menu && g_placeable_menu_vtable && Field<uintptr_t>(menu, 0) == g_placeable_menu_vtable) AddLightEntry(menu);
  g_activate(menu);
}

void InstallLightSources() {
  g_map_light = g_config.map_light < 10 ? 10 : g_config.map_light > 100 ? 100 : g_config.map_light;
  if (g_config.light_sources) {
    // The placeables' scripts nwn5e names (script_hooks.cpp runs them).
    AddScriptHandler(kUseScript, [](uint32_t self) { Used(self); });
    AddScriptHandler(kHitScript, [](uint32_t self) { Damaged(self); });
    AddScriptHandler(kSpellScript, [](uint32_t self) { SpellAt(self); });
    AddScriptHandler(kAttackScript, [](uint32_t self) { AttackedOut(self, false); });
    AddScriptHandler(kFlameAttackScript, [](uint32_t self) { AttackedOut(self, true); });
    AddScriptHandler(kFlameUseScript, [](uint32_t self) { FlameUsed(self); });
    AddScriptHandler(kFlameHitScript, [](uint32_t self) { FlameHit(self); });
    AddScriptHandler(kFlameSpellScript, [](uint32_t self) { FlameSpell(self); });
  }
  if (FlamesWanted()) {
    HookExport("CNWSPlaceable::LoadPlaceable (static lights, tile flames' stand-ins: normal placeables)", kLoadPlaceable,
               reinterpret_cast<void*>(&LoadPlaceable), reinterpret_cast<void**>(&g_load_placeable));
  }
  if (g_config.light_sources) {
    g_set_entry = reinterpret_cast<SetEntryFn>(EngineExport(kRadialSetEntry));
    g_disable_entry = reinterpret_cast<DisableEntryFn>(EngineExport(kRadialDisableEntry));
    g_placeable_menu_vtable = FindVtable("placeable radial menu: its vtable (Snuff / Light)", kRttiPlaceableMenu);
    if (g_set_entry && g_placeable_menu_vtable &&
        HookExport("CRadialMenu::Activate (Snuff / Light on a light's radial menu)", kRadialActivate,
                   reinterpret_cast<void*>(&RadialActivate), reinterpret_cast<void**>(&g_activate))) {
      Log("lights: a light's radial menu has Snuff / Light (its bottom slot)");
    } else {
      Log("SKIP lights: no radial Snuff / Light - the radial menu's functions weren't found");
    }
  }
  if (!FlamesWanted() && g_map_light >= 100) return;
  if (AddCreatureUpdateListener(&Update)) {
    Log("lights: light sources %s; the areas' light at %d%% of the module's%s", g_config.light_sources ? "can be put out" : "as they are",
        g_map_light, g_config.map_tile_dim && g_map_light < 100 ? ", tiles' main lights a step darker" : "");
  }
  AddChatCommand("!maplight", &MapLightCommand);
}
