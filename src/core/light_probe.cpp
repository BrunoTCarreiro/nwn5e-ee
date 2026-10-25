// !light (owner, 2026-10-25: "explore if we can make it so that environmental
// light sources can increase detection"): what nwn5e can tell about the light
// where the player character stands - the first step, to calibrate a light level
// on real areas before any rule uses it. Read-only; changes nothing.
// - the area: interior / exterior, above / under ground, natural, day or night
//   (the module's: what EE's own light terms use), its sun and moon light
//   (ambient, diffuse; brightness 0-255);
// - the tile underfoot: its main lights (the tile's coloured ambient) and source
//   lights (its torches, braziers ...: 0 = off), and how many of the 3 x 3 tiles
//   around have a source light on (0.36.0: by the tile's grid coordinate - the
//   getters' location vector is x / 10, y / 10; metres read -1);
// - placeables lit within 15 m (GetPlaceableIllumination: lamps, braziers ...),
//   the nearest one;
// - creatures and placeables within 20 m with a light visual effect (153-180,
//   VFX_DUR_LIGHT_*: the Light spell ...) among their effects;
// - the character itself: the engine's own visual list (+0x160, count +0x168:
//   what DoSpotDetection reads) - a light it carries (a torch, a light item, the
//   Light spell) and the darkvision visual (182) - and so EE's own light terms
//   now (only at night: a watcher without a light or darkvision spots at -5, a
//   hider carrying a light at +10).
// - 0.38.1 (owner: "candles don't seem to have a light at all"): the lights
//   within 10 m as the stealth light rule sees them - nwn5e's tile flame
//   stand-ins (colour, out) and placeables lit or named like a light (candle,
//   torch, lamp, brazier, fire: lit or not, their reach by brightness in cm) -
//   with their distance and whether the line to you is clear.

#include <stdint.h>

#include <string>
#include <vector>

#include "chat.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"

namespace {

constexpr size_t kVisuals = 0x160;      // CNWSObject: the visual effects on it (0x20 bytes each, id first)
constexpr size_t kVisualCount = 0x168;  // int32

// The object's visual effect ids, as the engine's spot check reads them.
std::vector<int> Visuals(void* object) {
  std::vector<int> ids;
  const int count = Field<int32_t>(object, kVisualCount);
  const uint8_t* list = Field<const uint8_t*>(object, kVisuals);
  if (!list || count <= 0 || count > 256) return ids;
  for (int i = 0; i < count; ++i) ids.push_back(*reinterpret_cast<const uint16_t*>(list + 0x20 * i));
  return ids;
}

std::string Lum(const char* var) {
  const std::string v(var);
  return "IntToString(((" + v + " >> 16) & 255) * 299 / 1000 + ((" + v + " >> 8) & 255) * 587 / 1000 + (" + v +
         " & 255) * 114 / 1000)";
}

std::vector<std::string> LightCommand(void* creature, const std::string&) {
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  // The area, the tile, lit placeables, light visuals around: the script side.
  std::string code =
      "object pc = OBJECT_SELF; object area = GetArea(pc); location at = GetLocation(pc); vector p = GetPosition(pc); string rep = \"\"; string ln;"
      "ln = (\"[light] \" + GetName(area) + \": \" + (GetIsAreaInterior(area) ? \"interior\" : \"exterior\")"
      " + \", \" + (GetIsAreaAboveGround(area) ? \"above ground\" : \"underground\")"
      " + (GetIsAreaNatural(area) ? \", natural\" : \", built\") + \", \" + (GetIsNight() ? \"NIGHT\" : \"day\")); SendMessageToPC(pc, ln); rep += ln + \" || \";"
      "int sa = GetAreaLightColor(AREA_LIGHT_COLOR_SUN_AMBIENT, area);"
      "int sd = GetAreaLightColor(AREA_LIGHT_COLOR_SUN_DIFFUSE, area);"
      "int ma = GetAreaLightColor(AREA_LIGHT_COLOR_MOON_AMBIENT, area);"
      "int md = GetAreaLightColor(AREA_LIGHT_COLOR_MOON_DIFFUSE, area);"
      "ln = (\"[light] area light (0-255): sun ambient \" + " + Lum("sa") + " + \", sun diffuse \" + " +
      Lum("sd") + " + \", moon ambient \" + " + Lum("ma") + " + \", moon diffuse \" + " + Lum("md") + "); SendMessageToPC(pc, ln); rep += ln + \" || \";"
      "int tx = FloatToInt(p.x / 10.0); int ty = FloatToInt(p.y / 10.0);"
      "location tile = Location(area, Vector(IntToFloat(tx), IntToFloat(ty), 0.0), 0.0);"
      "ln = (\"[light] tile \" + IntToString(tx) + \",\" + IntToString(ty) + \": main lights \" + IntToString(GetTileMainLight1Color(tile)) + \"/\""
      " + IntToString(GetTileMainLight2Color(tile)) + \", source lights \" + IntToString(GetTileSourceLight1Color(tile))"
      " + \"/\" + IntToString(GetTileSourceLight2Color(tile)) + \" (0 = off)\"); SendMessageToPC(pc, ln); rep += ln + \" || \";"
      "int lit = 0; int dx; int dy;"
      "for (dx = -1; dx <= 1; dx++) for (dy = -1; dy <= 1; dy++) {"
      " location l = Location(area, Vector(IntToFloat(tx + dx), IntToFloat(ty + dy), 0.0), 0.0);"
      " if (GetTileSourceLight1Color(l) > 0 || GetTileSourceLight2Color(l) > 0) lit++;"
      "}"
      "ln = (\"[light] tiles with a source light on, 3 x 3 around: \" + IntToString(lit)); SendMessageToPC(pc, ln); rep += ln + \" || \";"
      "int n = 0; float nearest = 99.0; string what = \"\";"
      "object o = GetFirstObjectInShape(SHAPE_SPHERE, 15.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
      "while (GetIsObjectValid(o)) {"
      " if (GetPlaceableIllumination(o)) { n++; float d = GetDistanceBetween(pc, o); if (d < nearest) { nearest = d; what = GetName(o) + \" (\" + GetTag(o) + \")\"; } }"
      " o = GetNextObjectInShape(SHAPE_SPHERE, 15.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
      "}"
      "ln = (\"[light] lit placeables within 15 m: \" + IntToString(n)"
      " + (n > 0 ? \", nearest \" + FloatToString(nearest, 0, 1) + \" m: \" + what : \"\")); SendMessageToPC(pc, ln); rep += ln + \" || \";"
      "int m = 0; string seen = \"\";"
      "o = GetFirstObjectInShape(SHAPE_SPHERE, 20.0, at, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
      "while (GetIsObjectValid(o)) {"
      " effect e = GetFirstEffect(o);"
      " while (GetIsEffectValid(e)) {"
      "  if (GetEffectType(e, TRUE) == EFFECT_TYPE_VISUALEFFECT) {"
      "   int v = GetEffectInteger(e, 0);"
      "   if (v >= 153 && v <= 180) { m++; if (GetStringLength(seen) < 120) seen += \" \" + GetName(o) + \":\" + IntToString(v); }"
      "  }"
      "  e = GetNextEffect(o);"
      " }"
      " o = GetNextObjectInShape(SHAPE_SPHERE, 20.0, at, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
      "}"
      "ln = (\"[light] light effects within 20 m: \" + IntToString(m) + seen); SendMessageToPC(pc, ln); rep += ln + \" || \";"
      // The lights near you as the light rule sees them.
      "int nf = 0; string near = \"\"; vector me = p + Vector(0.0, 0.0, 1.0);"
      "o = GetFirstObjectInShape(SHAPE_SPHERE, 10.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
      "while (GetIsObjectValid(o) && nf < 8) {"
      " string nm = GetStringLowerCase(GetName(o) + \" \" + GetTag(o));"
      " int flame = GetStringLeft(GetTag(o), 11) == \"nwn5e_flame\";"
      " if (flame || GetPlaceableIllumination(o) || FindSubString(nm, \"candle\") >= 0 || FindSubString(nm, \"torch\") >= 0"
      "     || FindSubString(nm, \"lamp\") >= 0 || FindSubString(nm, \"brazier\") >= 0 || FindSubString(nm, \"fire\") >= 0) {"
      "  vector lp = GetPosition(o); if (!flame) lp.z = lp.z + 1.0;"
      "  int los = LineOfSightVector(lp + VectorNormalize(me - lp) * 0.5, me);"
      "  near += \" \" + (flame ? \"flame\" : GetName(o)) + \" \" + FloatToString(GetDistanceBetween(pc, o), 0, 1) + \"m\""
      "   + (flame ? \" colour \" + IntToString(GetLocalInt(o, \"nwn5e_torch_colour\")) + (GetLocalInt(o, \"nwn5e_torch_out\") ? \" out\" : \"\")"
      "            : (GetPlaceableIllumination(o) ? \" lit\" : \" unlit\") + \" reach \" + IntToString(GetLocalInt(o, \"nwn5e_light_bright\"))"
      "              + \"/\" + IntToString(GetLocalInt(o, \"nwn5e_light_dim\")))"
      "   + (los ? \" clear\" : \" BLOCKED\") + \";\";"
      "  nf++;"
      " }"
      " o = GetNextObjectInShape(SHAPE_SPHERE, 10.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
      "}"
      "ln = (\"[light] lights within 10 m:\" + (nf ? near : \" none\")); SendMessageToPC(pc, ln); rep += ln + \" || \";"
      "SetLocalString(pc, \"nwn5e_light\", rep);";
  const int result = RunScriptChunk(code.c_str(), id);
  if (result != 0) Log("light probe: the script failed (%d)", result);
  Log("light probe: 0x%08X %s", id, engine::GetLocalString(creature, "nwn5e_light").c_str());
  // The character itself, from the engine's own list.
  int carried = 0;
  bool darkvision = false;
  for (int v : Visuals(creature)) {
    if (v >= 153 && v <= 180) carried = v;
    if (v == 182) darkvision = true;
  }
  std::vector<std::string> lines;
  lines.push_back(carried ? "[light] you carry a light (visual " + std::to_string(carried) +
                                ") - at night, watchers get +10 to spot you"
                          : "[light] you carry no light");
  lines.push_back(std::string("[light] your darkvision visual: ") + (darkvision ? "yes" : "no") +
                  " - at night, a watcher with neither a light nor darkvision spots at -5");
  Log("light probe: 0x%08X - carried light %d, darkvision %d (script %d)", id, carried, darkvision ? 1 : 0, result);
  return lines;
}

}  // namespace

void InstallLightProbe() { AddChatCommand("!light", &LightCommand); }
