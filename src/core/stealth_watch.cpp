// The stealth watch: once a second while a player character is in stealth mode,
// one pass over the hostile creatures near it.
// - Markers (0.26.0; owner, 2026-10-25: "how possible is it to show enemys' sight
//   ranges?" - "build your recommendation"): an eye over each hostile within 40 m
//   that sees it, an ear over one that only hears it - the game's Spot / Listen
//   icons by name in nwn5e's models (tools/data/stealth_marker.py; visualeffects
//   1571 / 1572). `stealth_marker`.
// - Rings (0.27.0; owner: "next up we'll add the range indicators"): the nearest
//   stealth_rings hostiles (within 45 m) get a ring on the ground at how far they
//   see (amber) and hear (blue, dashed) - each creature's own distances, the
//   engine's +0x7b0 sight / +0x7b8 hearing (+0x7b4 / +0x7bc while it fights:
//   what DoSpotDetection / DoListenDetection use), written on it as locals by the
//   creature update and shown by the nearest of the ring models
//   (tools/data/stealth_rings.py; visualeffects 1573-1592). `stealth_rings`.
//   Vision cones (0.29.0; owner: "Go with option 2"): with the 2024 rules'
//   hiders_need_search, a calm hostile sees a hider only inside its vision cone
//   (rules_perception.cpp, vision_cone degrees), so its sight shows as that cone
//   - filled, translucent, turning with its facing (visualeffects 1610-1649:
//   60 / 90 / 120 / 150 degrees x the radii; owner: "Vision cones should be 90
//   degrees, and they should be filled") - and a searching one's (detect mode,
//   or in combat with alert_creatures_search) as the full ring: it sees all
//   round. A hostile that finds the character takes it out of stealth mode
//   (rules_perception.cpp, found_ends_hiding), so all of it goes then.
//   The cone's colour (owner: "different colors depending on how easy it would
//   be for the enemy to spot the PC ... from 50% alpha red for very likely to 20%
//   alpha yellow for very unlikely"): a calm enemy's look isn't a roll but its
//   passive Perception, so the colour is its margin over the hider's Stealth as
//   the engine adds it up (rules_perception.cpp SpotMargin, once a second): red
//   6+ (it sees you in its cone, still or not), orange-red 1-5 (it sees you; a
//   still hider's -5 may save you), orange -4-0 (it doesn't; moving may change
//   that), yellow-orange -9 to -5, yellow -10 or less.
// - Heard, not seen (0.27.0; owner: "for the hearing detection, do option 2"):
//   2024 rules (perception=faithful) - finding a hider by hearing is finding it,
//   so a hostile that hears it but doesn't see it investigates: it walks to where
//   the sound came from in search mode (detect: a rolled Perception, SK5), looks
//   about, waits 6 s (a 6 s detection pass always falls there: its rolls are what
//   find the hider - hiders_need_search), walks back. Not one in combat, in conversation, plot, someone's
//   associate or not commandable; again after 25 s at most. NWN's own AI never
//   acts on hearing ("DO NOT RELY ON HEARING FOR ENEMY DETECTION",
//   nw_c2_default2) beyond a head scratch within 7 m. `heard_investigates`.
// - Light (0.32.0; owner: environmental light, "do the sensible distances"):
//   the light where the hiding character stands, for rules_perception.cpp's G
//   (local nwn5e_light_level, + 1): bright - within 4 m of a lit placeable
//   (GetPlaceableIllumination: braziers, torch brackets ...), or the inner half
//   of a light effect's radius (VFX_DUR_LIGHT_*: 5 / 10 / 15 / 20 m), or
//   carrying a light, or an outdoor area lit 110+ (its sun or moon ambient +
//   half its diffuse, by the time of day; 0-255); dim - within 9 m of a lit
//   placeable or a light effect's radius, or an outdoor area lit 30+; else dark.
//   Indoors / underground the area's own light doesn't count (0.32.1; owner: a
//   small empty room with no light near showed dim - the Training Halls' moon
//   ambient 58 had made the whole area dim). Measured on the
//   Training Halls at night: moon ambient 58 (dim), braziers and torch brackets
//   found lit at 3.9 - 7.8 m. The character is told when it changes.
//   Torches built into the tiles (0.36.0, M3): on a tile with a source light on,
//   bright; on the 8 tiles round it, dim - NWN doesn't say where on its 10 m tile
//   a torch stands. The tile light getters take the tile's GRID coordinate (x / 10,
//   y / 10) as the location's vector; 0.32.0 passed metres, so they read -1 and
//   tile torches never counted.
//   `stealth_light` (2024 perception). Light zones (0.32.1; owner: "Light
//   sources should have a circle indicator so the player can see where they
//   take effect"): while hiding, unless the area is bright everywhere, each lit
//   placeable within 30 m shows a pale solid ring at 4 m (bright ends) and a
//   dashed one at 9 m (dim ends), a light effect of radius r at r/2 and r
//   (tools/data/stealth_rings.py rows 1820-1835).
// All from the engine's own perception (GetObjectSeen / GetObjectHeard), so what
// decides it shows: the 2024 Stealth against passive Perception, line of sight,
// EE's night light terms (at night a watcher without a light or darkvision
// visual spots at -5, a hider carrying a light at +10) and the area's Spot
// modifier. Markers and rings go when it stops hiding, and an area's leftovers
// when it is entered.
// - Henchmen (0.35.0; owner, 2026-10-25: "If they're following the PC they
//   should also enter stealth mode. If they have been asked to stand their
//   ground they should not" - "go with all your recommendations"): once a
//   second while the player character hides, its henchmen enter stealth mode -
//   not one set to Stand your ground (NW_ASSOCIATE_MASTER bit 0x08000000, what
//   the radial menu sets), down, dead, fighting, elsewhere, or found since the
//   player hid (rules_perception.cpp F: local nwn5e_found_out). For the first 2
//   s a stand-ground one in stealth mode is taken out (the SoU / HotU henchman
//   scripts copy the master's mode whatever the order); after that the player's
//   own order stands. When the player stops hiding, following henchmen leave
//   stealth mode with it. `henchmen_follow_stealth`, both rule modes. A hiding
//   henchman gets its own light level and see-through look (and outline,
//   stealth_ghost.cpp) - HS3 - while the markers, rings and cones stay the
//   player character's (HS4).
// - The see-through look's opacity (0.35.0, RB1): the light's step less 2% per
//   point of Stealth bonus beyond ability + proficiency (Expertise, items,
//   spells), 30% at least (rules::StealthOpacity).

#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"
#include "rules/skills.h"

bool SpotMargin(void* observer, void* hider, int* margin);  // rules_perception.cpp
bool StealthParts(void* creature, int* extra, bool* proficient);  // rules_skills.cpp
int CarriedLightRadius(void* creature);                      // rules_perception.cpp
void SetStealthOpacity(uint32_t id, float opacity);          // stealth_ghost.cpp
bool StealthGhostOn();                                       // stealth_ghost.cpp

namespace {

constexpr int kSeenRow = 1571;   // NWN5E_STEALTH_SEEN: an eye
constexpr int kHeardRow = 1572;  // NWN5E_STEALTH_HEARD: an ear
constexpr int kSightRingRow = 1573;
constexpr int kHearRingRow = 1583;
constexpr int kConeRow = 1610;  // + 40 x the colour + 10 x the angle's index + the radius' index
constexpr float kConeAngles[] = {60, 90, 120, 150};  // tools/data/stealth_rings.py
constexpr float kRingRadii[] = {5, 10, 15, 20, 25, 28, 30, 35, 40, 50};  // tools/data/stealth_rings.py
constexpr size_t kSightRange = 0x7b0;  // float, metres; +4 in combat (DoSpotDetection)
constexpr size_t kHearRange = 0x7b8;   // float, metres; +4 in combat (DoListenDetection)
constexpr uint64_t kCheckMs = 1000;
constexpr const char* kVarSightRing = "nwn5e_sight_ring";
constexpr const char* kVarHearRing = "nwn5e_hear_ring";
constexpr const char* kVarConeRing = "nwn5e_cone_ring";  // > 0 a cone row; 0 calm sees all round; -1 calm sees nothing
// Each creature's own lift for its cone and rings, centimetres (owner, 2026-10-25:
// "some z-fighting on the cones ... weird interference patterns when they
// overlap"): overlapping ones never share a height. Fixed per creature (its id),
// put on with EffectVisualEffect's vTranslate.
constexpr const char* kVarRingLift = "nwn5e_ring_lift";

struct Watch {
  bool on = false;  // markers or rings may be up in the area
  void* area = nullptr;
  uint64_t next = 0;
  int passes = 0;   // passes since it started hiding
};
std::unordered_map<uint32_t, Watch> g_watches;  // by player character
std::unordered_map<uint32_t, int64_t> g_rings;  // by creature: the rows written on it (sight, hearing, cone)
bool g_investigate = false;
uint32_t g_hider = 0;                            // the hiding player character the cones are coloured against
std::unordered_map<uint32_t, uint64_t> g_next_colour;  // by creature: when its colour is weighed again

int Nearest(const float* values, int count, float value) {
  int best = 0;
  for (int i = 1; i < count; ++i) {
    if (fabsf(values[i] - value) < fabsf(values[best] - value)) best = i;
  }
  return best;
}

int RingRow(int first, float range) {
  if (!(range > 0.0f)) return 0;
  return first + Nearest(kRingRadii, static_cast<int>(sizeof(kRingRadii) / sizeof(kRingRadii[0])), range);
}

// A calm creature's sight: its vision cone's row, 0 when it sees all round (no
// cones: the 2024 hiders_need_search off, NWN rules, or 360), -1 when it sees
// no hider (vision_cone 0).
// 0 (yellow: very unlikely) - 4 (red: very likely) from a calm look's margin.
int ColourOf(int margin) {
  if (margin >= 6) return 4;
  if (margin >= 1) return 3;
  if (margin >= -4) return 2;
  if (margin >= -9) return 1;
  return 0;
}

int ConeRow(float range, int colour) {
  if (!(range > 0.0f) || g_config.perception != rules::RuleVariant::kFaithful || !g_config.hiders_need_search ||
      g_config.vision_cone >= 360) {
    return 0;
  }
  if (g_config.vision_cone <= 0) return -1;
  const int angle = Nearest(kConeAngles, static_cast<int>(sizeof(kConeAngles) / sizeof(kConeAngles[0])),
                            static_cast<float>(g_config.vision_cone));
  return kConeRow + 40 * colour + 10 * angle + RingRow(0, range);
}

void Run(const std::string& code, uint32_t pc, const char* what) {
  static bool told = false;
  const int result = RunScriptChunk(code.c_str(), pc);
  if (result != 0 && !told) {
    told = true;
    Log("stealth watch: the %s script failed (%d)", what, result);
  }
}

std::string Num(int value) { return std::to_string(value); }

// The light where the hider stands (G): 0 dark, 1 dim, 2 bright, stored + 1;
// with `zones`, the light zones round the sources near it too (the player's).
std::string LightPart(int carried, bool zones) {
  const auto lum = [](const char* v) {
    const std::string s(v);
    return "(((" + s + " >> 16) & 255) * 299 + ((" + s + " >> 8) & 255) * 587 + (" + s + " & 255) * 114) / 1000";
  };
  std::string code = "int light = 0; object area = GetArea(pc); int night = GetIsNight();"
         "int amb = GetAreaLightColor(night ? AREA_LIGHT_COLOR_MOON_AMBIENT : AREA_LIGHT_COLOR_SUN_AMBIENT, area);"
         "int dif = GetAreaLightColor(night ? AREA_LIGHT_COLOR_MOON_DIFFUSE : AREA_LIGHT_COLOR_SUN_DIFFUSE, area);"
         "int lum = " + lum("amb") + " + (" + lum("dif") + ") / 2;"
         // Indoors or underground the area's own light is a tint, not light in every room (owner: a small
         // empty room with no light sources showed dim): dark unless a source is near.
         "if (GetIsAreaInterior(area) || !GetIsAreaAboveGround(area)) lum = 0;"
         "if (lum >= 110) light = 2; else if (lum >= 30) light = 1;"
         // A light it carries: 5 m at least dim, 10 m+ bright (rules::CarriedLightLevel).
         "if (" + std::to_string(rules::CarriedLightLevel(carried)) + " > light) light = " +
         std::to_string(rules::CarriedLightLevel(carried)) + ";"
         "object q = GetFirstObjectInShape(SHAPE_SPHERE, 9.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
         "while (light < 2 && GetIsObjectValid(q)) {"
         " if (GetPlaceableIllumination(q)) { if (GetDistanceBetween(pc, q) <= 4.0) light = 2; else if (light < 1) light = 1; }"
         " q = GetNextObjectInShape(SHAPE_SPHERE, 9.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
         "}"
         // Torches in the tiles: its tile bright, the 8 round it dim (grid coordinates).
         "int tx = FloatToInt(GetPosition(pc).x / 10.0); int ty = FloatToInt(GetPosition(pc).y / 10.0); int ix; int iy;"
         "for (ix = -1; ix <= 1 && light < 2; ix++) for (iy = -1; iy <= 1 && light < 2; iy++) {"
         " location tl = Location(area, Vector(IntToFloat(tx + ix), IntToFloat(ty + iy), 0.0), 0.0);"
         " if (GetTileSourceLight1Color(tl) > 0 || GetTileSourceLight2Color(tl) > 0) {"
         "  if (ix == 0 && iy == 0) light = 2; else if (light < 1) light = 1;"
         " }"
         "}"
         "q = GetFirstObjectInShape(SHAPE_SPHERE, 20.0, at, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
         "while (light < 2 && GetIsObjectValid(q)) {"
         " effect e = GetFirstEffect(q);"
         " while (GetIsEffectValid(e)) {"
         "  if (GetEffectType(e, TRUE) == EFFECT_TYPE_VISUALEFFECT) {"
         "   int v = GetEffectInteger(e, 0);"
         "   if (v >= 153 && v <= 180) {"
         "    float r = 5.0 * IntToFloat(1 + (v - 153) % 4); float d = GetDistanceBetween(pc, q);"
         "    if (d <= r / 2.0) light = 2; else if (d <= r && light < 1) light = 1;"
         "   }"
         "  }"
         "  e = GetNextEffect(q);"
         " }"
         " q = GetNextObjectInShape(SHAPE_SPHERE, 20.0, at, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
         "}";
  // Light zones: rings round lit sources within 30 m (none in an area bright everywhere).
  if (zones) {
    code +=
         "object s = GetFirstObjectInShape(SHAPE_SPHERE, 40.0, at, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
         "while (GetIsObjectValid(s)) {"
         " int zone = 0;"
         " if (lum < 110 && s != pc && GetDistanceBetween(pc, s) <= 30.0) {"
         "  if (GetObjectType(s) == OBJECT_TYPE_PLACEABLE && GetPlaceableIllumination(s)) zone = 1821 * 10000 + 1832;"
         "  else {"
         "   effect le = GetFirstEffect(s);"
         "   while (GetIsEffectValid(le) && !zone) {"
         "    if (GetEffectType(le, TRUE) == EFFECT_TYPE_VISUALEFFECT) {"
         "     int lv = GetEffectInteger(le, 0);"
         "     if (lv >= 153 && lv <= 180) {"
         "      int k = (lv - 153) % 4;"
         "      int b = k == 0 ? 1820 : (k == 1 ? 1822 : (k == 2 ? 1823 : 1825));"
         "      int dm = k == 0 ? 1830 : (k == 1 ? 1833 : (k == 2 ? 1834 : 1835));"
         "      zone = b * 10000 + dm;"
         "     }"
         "    }"
         "    le = GetNextEffect(s);"
         "   }"
         "  }"
         " }"
         " if (zone != GetLocalInt(s, \"nwn5e_lring\")) {"
         "  effect le = GetFirstEffect(s);"
         "  while (GetIsEffectValid(le)) { if (GetEffectTag(le) == \"nwn5e_lring\") RemoveEffect(s, le); le = GetNextEffect(s); }"
         "  if (zone) {"
         "   ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect(zone / 10000)), \"nwn5e_lring\"), s);"
         "   ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect(zone % 10000)), \"nwn5e_lring\"), s);"
         "  }"
         "  SetLocalInt(s, \"nwn5e_lring\", zone);"
         " }"
         " s = GetNextObjectInShape(SHAPE_SPHERE, 40.0, at, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
         "}";
  }
  code +=
         "if (light + 1 != GetLocalInt(pc, \"nwn5e_light_level\")) {"
         " SetLocalInt(pc, \"nwn5e_light_level\", light + 1);"
         // The steady ghost look, put on again so the new light's opacity is set (stealth_ghost.cpp).
         " if (" + std::string(StealthGhostOn() ? "TRUE" : "FALSE") + ") {"
         "  effect g = GetFirstEffect(pc);"
         "  while (GetIsEffectValid(g)) { if (GetEffectTag(g) == \"nwn5e_ghost\") RemoveEffect(pc, g); g = GetNextEffect(pc); }"
         "  ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect(VFX_DUR_GHOST_TRANSPARENT)),"
         "   \"nwn5e_ghost\"), pc);"
         " }"
         " if (GetIsPC(pc)) SendMessageToPC(pc, light == 2 ? \"nwn5e: you are in light - enemies see you normally\""
         "  : (light == 1 ? \"nwn5e: you are in dim light - enemies spot you at -5\""
         "  : \"nwn5e: you are in darkness - only darkvision can see you\"));"
         "}"
         "if (" + std::to_string(carried) + " != GetLocalInt(pc, \"nwn5e_light_carried\")) {"
         " SetLocalInt(pc, \"nwn5e_light_carried\", " + std::to_string(carried) + ");"
         " if (GetIsPC(pc) && " + std::to_string(carried) + " > 0)"
         "  SendMessageToPC(pc, \"nwn5e: you carry a light (" + std::to_string(carried) + " m) - enemies spot you at +" +
         std::to_string(rules::CarriedLightSpotBonus(carried)) + "\");"
         "}";
  return code;
}

// The pass, on the hiding character.
void Pass(uint32_t pc, int carried) {
  const int rings = g_config.stealth_rings;
  std::string code =
      "object pc = OBJECT_SELF; location at = GetLocation(pc);"
      "int pass = GetLocalInt(pc, \"nwn5e_watch_pass\") + 1; SetLocalInt(pc, \"nwn5e_watch_pass\", pass);";
  if (g_config.stealth_light && g_config.perception == rules::RuleVariant::kFaithful) code += LightPart(carried, true);
  code +=
      // The nearest hostiles get rings.
      "int n = 1; object o = GetNearestCreature(CREATURE_TYPE_REPUTATION, REPUTATION_TYPE_ENEMY, pc, n);"
      "while (n <= " + Num(rings) + " && GetIsObjectValid(o) && GetDistanceBetween(pc, o) <= 45.0) {"
      " if (!GetIsDead(o)) SetLocalInt(o, \"nwn5e_ring_pass\", pass);"
      " n++; o = GetNearestCreature(CREATURE_TYPE_REPUTATION, REPUTATION_TYPE_ENEMY, pc, n);"
      "}"
      "o = GetFirstObjectInShape(SHAPE_SPHERE, 50.0, at, FALSE, OBJECT_TYPE_CREATURE);"
      "while (GetIsObjectValid(o)) {"
      " if (o != pc) {"
      "  int want = 0;"
      "  if (!GetIsDead(o) && GetIsEnemy(pc, o) && GetDistanceBetween(pc, o) <= 40.0)"
      "   want = GetObjectSeen(pc, o) ? 2 : (GetObjectHeard(pc, o) ? 1 : 0);";
  if (g_config.stealth_marker) {
    code +=
        "  if (want != GetLocalInt(o, \"nwn5e_noticed\")) {"
        "   effect e = GetFirstEffect(o);"
        "   while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"nwn5e_noticed\") RemoveEffect(o, e); e = GetNextEffect(o); }"
        "   if (want) ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect("
        "    want == 2 ? " + Num(kSeenRow) + " : " + Num(kHeardRow) + ")), \"nwn5e_noticed\"), o);"
        "   SetLocalInt(o, \"nwn5e_noticed\", want);"
        "  }";
  }
  if (rings > 0) {
    code +=
        "  int sight = GetLocalInt(o, \"" + std::string(kVarSightRing) + "\");"
        "  int hear = GetLocalInt(o, \"" + std::string(kVarHearRing) + "\");"
        "  int cone = GetLocalInt(o, \"" + std::string(kVarConeRing) + "\");"
        // Calm: its vision cone; searching: the full ring - it sees all round.
        "  int searching = GetActionMode(o, ACTION_MODE_DETECT) || (" + Num(g_config.alert_creatures_search ? 1 : 0) +
        " && GetIsInCombat(o));"
        "  int look = searching || cone == 0 ? sight : (cone > 0 ? cone : 0);"
        "  int ring = 0;"
        "  if (GetLocalInt(o, \"nwn5e_ring_pass\") == pass && hear > 0) ring = look * 10000 + hear;"
        "  if (ring != GetLocalInt(o, \"nwn5e_ring\")) {"
        "   effect e = GetFirstEffect(o);"
        "   while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"nwn5e_ring\") RemoveEffect(o, e); e = GetNextEffect(o); }"
        "   if (ring) {"
        "    float lift = IntToFloat(GetLocalInt(o, \"" + std::string(kVarRingLift) + "\")) / 100.0;"
        "    if (look) ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect("
        "     EffectVisualEffect(look, FALSE, 1.0, Vector(0.0, 0.0, lift))), \"nwn5e_ring\"), o);"
        "    ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect("
        "     EffectVisualEffect(hear, FALSE, 1.0, Vector(0.0, 0.0, lift + 0.01))), \"nwn5e_ring\"), o);"
        "   }"
        "   SetLocalInt(o, \"nwn5e_ring\", ring);"
        "  }";
  }
  if (g_investigate) {
    code +=
        "  int left = GetLocalInt(o, \"nwn5e_seek\");"
        "  if (left > 0) {"
        "   if (GetIsInCombat(o) || want == 2) DeleteLocalInt(o, \"nwn5e_seek\"); else SetLocalInt(o, \"nwn5e_seek\", left - 1);"
        "  } else if (want == 1 && !GetIsInCombat(o) && GetCommandable(o) && !IsInConversation(o) && !GetPlotFlag(o)"
        "             && !GetIsObjectValid(GetMaster(o))) {"
        "   SetLocalInt(o, \"nwn5e_seek\", 25);"
        "   location home = GetLocation(o);"
        "   AssignCommand(o, ClearAllActions());"
        "   SetActionMode(o, ACTION_MODE_DETECT, TRUE);"
        "   AssignCommand(o, ActionMoveToLocation(at));"
        "   AssignCommand(o, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_LEFT));"
        "   AssignCommand(o, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_RIGHT));"
        "   AssignCommand(o, ActionWait(6.0));"
        "   AssignCommand(o, ActionMoveToLocation(home));"
        "   AssignCommand(o, ActionDoCommand(SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, FALSE)));"
        "   AssignCommand(o, ActionDoCommand(DeleteLocalInt(OBJECT_SELF, \"nwn5e_seek\")));"
        "   SetLocalInt(pc, \"nwn5e_seek_started\", GetLocalInt(pc, \"nwn5e_seek_started\") + 1);"
        "  }";
  }
  code +=
      " }"
      " o = GetNextObjectInShape(SHAPE_SPHERE, 50.0, at, FALSE, OBJECT_TYPE_CREATURE);"
      "}";
  Run(code, pc, "watch");
}

// Every marker and ring in the character's area off.
void Clear(uint32_t pc) {
  Run("object area = GetArea(OBJECT_SELF);"
      "object o = GetFirstObjectInArea(area, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
      "while (GetIsObjectValid(o)) {"
      " if (GetLocalInt(o, \"nwn5e_noticed\") || GetLocalInt(o, \"nwn5e_ring\") || GetLocalInt(o, \"nwn5e_lring\")) {"
      "  effect e = GetFirstEffect(o);"
      "  while (GetIsEffectValid(e)) {"
      "   string t = GetEffectTag(e);"
      "   if (t == \"nwn5e_noticed\" || t == \"nwn5e_ring\" || t == \"nwn5e_lring\") RemoveEffect(o, e);"
      "   e = GetNextEffect(o);"
      "  }"
      "  DeleteLocalInt(o, \"nwn5e_noticed\"); DeleteLocalInt(o, \"nwn5e_ring\"); DeleteLocalInt(o, \"nwn5e_lring\");"
      " }"
      " o = GetNextObjectInArea(area, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
      "}"
      "DeleteLocalInt(OBJECT_SELF, \"nwn5e_light_level\"); DeleteLocalInt(OBJECT_SELF, \"nwn5e_light_carried\");"
      "effect g = GetFirstEffect(OBJECT_SELF);"
      "while (GetIsEffectValid(g)) { if (GetEffectTag(g) == \"nwn5e_ghost\") RemoveEffect(OBJECT_SELF, g); g = GetNextEffect(OBJECT_SELF); }",
      pc, "clearing");
  SetStealthOpacity(pc, 0.0f);
}

// How visible a hider is drawn: its light, less its Stealth bonus (RB1).
float OpacityFor(void* creature) {
  const int level = engine::GetLocalInt(creature, "nwn5e_light_level") - 1;
  int extra = 0;
  bool proficient = false;
  if (!StealthParts(creature, &extra, &proficient)) extra = 0;
  return rules::StealthOpacity(level, extra);
}

// HS1: the player character's henchmen follow it into stealth mode and out
// (see the top); `first`: its first pass hiding, `early`: its first 2. Why each
// one did or didn't is logged when it changes (0.35.1: in play none followed,
// and nothing said why).
std::unordered_map<uint32_t, std::string> g_follow_why;  // by player character

void Follow(uint32_t pc, bool hiding, bool first, bool early) {
  std::string code =
      "int moved = 0; string why = \"\"; int n = 1; object h = GetHenchman(OBJECT_SELF, n);"
      "while (GetIsObjectValid(h)) {"
      " int stand = (GetLocalInt(h, \"NW_ASSOCIATE_MASTER\") & 134217728) != 0;"  // NW_ASC_MODE_STAND_GROUND
      " int hid = GetActionMode(h, ACTION_MODE_STEALTH);"
      " string r = \"\";";
  if (hiding) {
    if (first) code += " DeleteLocalInt(h, \"nwn5e_found_out\");";
    code +=
        " if (stand) {"
        "  r = \"stands its ground\";" +
        std::string(early ? "  if (hid) { SetActionMode(h, ACTION_MODE_STEALTH, FALSE); moved -= 1; r += \" - taken out\"; }"
                          : "") +
        " }"
        " else if (hid) r = \"hiding\";"
        " else if (GetLocalInt(h, \"nwn5e_found_out\")) r = \"found - out until you hide again\";"
        " else if (GetIsDead(h)) r = \"dead\";"
        " else if (GetLocalInt(h, \"nwn5e_down\")) r = \"down\";"
        " else if (GetIsInCombat(h)) r = \"fighting\";"
        " else if (GetArea(h) != GetArea(OBJECT_SELF)) r = \"elsewhere\";"
        " else {"
        "  SetActionMode(h, ACTION_MODE_STEALTH, TRUE); moved += 1;"
        "  r = GetActionMode(h, ACTION_MODE_STEALTH) ? \"follows into stealth mode\" : \"told to hide - the game refused\";"
        " }";
  } else {
    code +=
        " if (!stand && hid) { SetActionMode(h, ACTION_MODE_STEALTH, FALSE); moved -= 1; r = \"out with you\"; }"
        " else r = stand ? \"stands its ground\" : \"not hiding\";"
        " DeleteLocalInt(h, \"nwn5e_found_out\");";
  }
  code +=
      " why += (why == \"\" ? \"\" : \"; \") + GetName(h) + \": \" + r;"
      " n++; h = GetHenchman(OBJECT_SELF, n);"
      "}"
      "SetLocalInt(OBJECT_SELF, \"nwn5e_follow_moved\", moved);"
      "SetLocalString(OBJECT_SELF, \"nwn5e_follow_why\", why == \"\" ? \"no henchmen\" : why);";
  Run(code, pc, "henchmen follow");
  void* creature = CreatureById(pc);
  if (!creature) return;
  const int moved = engine::GetLocalInt(creature, "nwn5e_follow_moved");
  const std::string why = engine::GetLocalString(creature, "nwn5e_follow_why");
  std::string& last = g_follow_why[pc];
  if (why != last || moved != 0) {
    last = why;
    Log("stealth watch: 0x%08X %s - henchmen: %s (%+d into stealth mode)", pc, hiding ? "hides" : "stopped hiding",
        why.c_str(), moved);
  }
}

// A player's henchman: its master is a player character.
bool PlayersHenchman(void* creature) {
  if (Field<uint16_t>(creature, ee::kCreatureAssociateType) != ee::kAssociateTypeHenchman) return false;
  const uint32_t master = Field<uint32_t>(creature, ee::kCreatureMaster);
  void* owner = master && master != ee::kObjectInvalid ? CreatureById(master) : nullptr;
  return owner && Field<int32_t>(owner, ee::kCreatureIsPlayerCharacter) != 0;
}

// A henchman's own ghost look and light level off.
void ClearHider(uint32_t id) {
  Run("DeleteLocalInt(OBJECT_SELF, \"nwn5e_light_level\"); DeleteLocalInt(OBJECT_SELF, \"nwn5e_light_carried\");"
      "effect g = GetFirstEffect(OBJECT_SELF);"
      "while (GetIsEffectValid(g)) { if (GetEffectTag(g) == \"nwn5e_ghost\") RemoveEffect(OBJECT_SELF, g); g = GetNextEffect(OBJECT_SELF); }",
      id, "clearing a henchman");
  SetStealthOpacity(id, 0.0f);
}

// HS3: a player's henchman hiding, once a second - its light and see-through look.
void HenchmanPass(void* creature, uint32_t id) {
  Watch& watch = g_watches[id];
  const uint64_t now = GetTickCount64();
  if (now < watch.next) return;
  watch.next = now + kCheckMs;
  void* area = engine::AreaOf(creature);
  if (!area) return;
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  if (area != watch.area) {
    watch.area = area;
    if (watch.on) ClearHider(id);
    watch.on = false;
  }
  if (hiding) {
    if (!watch.on) Log("stealth watch: henchman 0x%08X hides", id);
    if (g_config.stealth_light && g_config.perception == rules::RuleVariant::kFaithful) {
      Run("object pc = OBJECT_SELF; location at = GetLocation(pc);" + LightPart(CarriedLightRadius(creature), false), id,
          "henchman light");
    }
    if (StealthGhostOn()) SetStealthOpacity(id, OpacityFor(creature));
    watch.on = true;
  } else if (watch.on) {
    ClearHider(id);
    watch.on = false;
    Log("stealth watch: henchman 0x%08X stopped hiding", id);
  }
}

// Every creature, each AI pass: its ring rows as locals, written when they change.
void WriteRanges(void* creature, uint32_t id) {
  const bool fighting = Field<int32_t>(creature, ee::kCreatureInCombat) != 0;
  const float sight = Field<float>(creature, kSightRange + (fighting ? 4 : 0));
  const float hear = Field<float>(creature, kHearRange + (fighting ? 4 : 0));
  const int sight_row = RingRow(kSightRingRow, sight), hear_row = RingRow(kHearRingRow, hear);
  // The cone's colour: this creature's look against the hiding character, once a second.
  static std::unordered_map<uint32_t, int> colours;
  int& colour = colours.emplace(id, 2).first->second;
  const uint64_t now = GetTickCount64();
  uint64_t& next = g_next_colour[id];
  if (g_hider && g_hider != id && now >= next) {
    next = now + 1000;
    int margin = 0;
    if (SpotMargin(creature, CreatureById(g_hider), &margin)) colour = ColourOf(margin);
  }
  const int cone_row = ConeRow(sight, colour);
  const int64_t key = (static_cast<int64_t>(sight_row) * 10000 + hear_row) * 10000 + (cone_row + 1);
  auto it = g_rings.find(id);
  if (it != g_rings.end() && it->second == key) return;
  g_rings[id] = key;
  engine::SetLocalInt(creature, kVarSightRing, sight_row);
  engine::SetLocalInt(creature, kVarHearRing, hear_row);
  engine::SetLocalInt(creature, kVarConeRing, cone_row);
  engine::SetLocalInt(creature, kVarRingLift, static_cast<int>(id % 11) * 2);
  static int logged = 0;
  if (logged < 8) {
    ++logged;
    Log("stealth watch: 0x%08X sees %.1f m, hears %.1f m%s", id, sight, hear, fighting ? " (fighting)" : "");
  }
}

void Update(void* creature) {
  if (!creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (g_config.stealth_rings > 0) WriteRanges(creature, id);
  if (Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) {
    if (PlayersHenchman(creature)) HenchmanPass(creature, id);
    return;
  }
  Watch& watch = g_watches[id];
  const uint64_t now = GetTickCount64();
  if (now < watch.next) return;
  watch.next = now + kCheckMs;
  void* area = engine::AreaOf(creature);
  if (!area) return;
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  if (hiding) {
    g_hider = id;
  } else if (g_hider == id) {
    g_hider = 0;
  }
  if (area != watch.area) {
    // A new area (or a loaded game): markers and rings left in it from before go.
    watch.area = area;
    Clear(id);
    watch.on = false;
  }
  if (hiding) {
    if (!watch.on) {
      Log("stealth watch: 0x%08X hides - watching who notices it", id);
      watch.passes = 0;
    }
    ++watch.passes;
    Pass(id, CarriedLightRadius(creature));
    // The see-through look's opacity for its light and Stealth bonus (stealth_ghost.cpp).
    if (StealthGhostOn()) SetStealthOpacity(id, OpacityFor(creature));
    if (g_config.henchmen_follow_stealth) Follow(id, true, watch.passes == 1, watch.passes <= 2);
    watch.on = true;
  } else if (watch.on) {
    Clear(id);
    watch.on = false;
    if (g_config.henchmen_follow_stealth) Follow(id, false, false, false);
    const int started = engine::GetLocalInt(creature, "nwn5e_seek_started");
    Log("stealth watch: 0x%08X stopped hiding - markers and rings off (%d investigations so far)", id, started);
  }
}

}  // namespace

void InstallStealthWatch() {
  g_investigate = g_config.heard_investigates && g_config.perception == rules::RuleVariant::kFaithful;
  if (!g_config.stealth_marker && g_config.stealth_rings <= 0 && !g_investigate) return;
  if (AddCreatureUpdateListener(&Update)) {
    Log("stealth watch: while a player character hides - markers %s, rings on the nearest %d, heard -> investigate %s",
        g_config.stealth_marker ? "on" : "off", g_config.stealth_rings, g_investigate ? "on (2024)" : "off");
  }
}
