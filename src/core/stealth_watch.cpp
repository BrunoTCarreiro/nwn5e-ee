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
//   Cut at walls (#9 c; owner, 2026-11-15: "yes" - "How hard to do is this though?": the rule already needs a
//   clear line, the drawing is the work): with cone_walls, a calm hostile's cone is drawn as 15-degree slices
//   (visualeffects 2260-3059, tools/data/stealth_rings.py), each as long as its line of sight along the slice's
//   middle - the engine's CNWSArea::ClearLineOfSight from its eyes (+1 m), as its own perception calls it -
//   rounded down to a drawn length (1 ... 50 m; rules/cone_slices.h), never past its sight. Measured once a
//   second, in the pass, for the hostiles whose rings were drawn in the last one (the nearest stealth_rings, so at
//   most 6 x 6 slices a second by default), just before the pass puts them on; each slice its own effect, changed
//   only when its row does. Until a creature's first slices are measured its whole cone shows. The rule agrees
//   (ConeCutsSight, rules_perception.cpp): a calm hostile whose cone is drawn in slices sees a hider only inside a
//   drawn slice, no farther than its length, read from the rows drawn - so it never sees past where its slice is
//   cut; the engine's own line of sight still applies inside a slice (a pillar off the slice's middle still hides
//   you), so the drawing may show more than it sees, never less.
//   Idle guards look about (#9 d; owner, 2026-11-15: "yes, sounds fun but we have to tune this"): while a player
//   character hides, each calm hostile within 40 m of it looks about now and then - every idle_look_min_seconds ..
//   idle_look_max_seconds (8-16) at random, it turns idle_look_min_degrees .. idle_look_max_degrees (40-120) left or
//   right of its post's facing (SetFacing), holds the look idle_look_hold seconds (3; the look-far animation) and
//   turns back to its post's facing - so its cone sweeps. All queued as actions, so anything the creature does of
//   its own (it hears you, a fight) takes over; its next look starts from its post again, so a look cut short
//   doesn't leave it turned for good. Not one fighting, searching, investigating a noise, hiding, someone's
//   associate, or with any action under way or queued (walking a route, sitting ...; rules/idle_look.h), nor one in
//   a conversation or not commandable. Its first look comes a whole wait after you hide (not at once), and only
//   while a player character hides: cheaper (no turning or script runs for creatures nobody watches) and fair -
//   the looks only matter to a hider's cones, and calm hostiles see a hider only in them. Each look logged (who,
//   the turn, the next look's time; 300 lines at most), and why one doesn't look when that changes (60 at most).
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
//   bright; on the 8 tiles round it, dim. The tile light getters take the tile's
//   GRID coordinate (x / 10, y / 10) as the location's vector; 0.32.0 passed
//   metres, so they read -1 and tile torches never counted. 0.37.0 (owner: a
//   room lit only by small candles still showed "in light"): by each flame's
//   own place instead - light_sources.cpp puts a stand-in (tag nwn5e_flame_...)
//   where the tile model's flame is - a bright colour (odd: white, pale) bright
//   within 4 m and dim within 7.5 m, a pale dark one (even: half as bright) dim
//   within 4 m; a doused one nothing. Their light zones show as rings too,
//   drawn on the floor below the flame. 0.37.2 (owner: "I'm seeing some light
//   sources affecting my character through a wall"): a light counts only with a
//   clear line from it to the hider's chest (LineOfSightVector, from half a
//   metre out of the light toward the hider so its own model or wall doesn't
//   block it) - lit placeables, light effects and tile flames alike. The
//   game's own lighting may still shine through walls on screen. Its light
//   zone rings show only for a light with that clear line too. 0.38.0 (owner:
//   "we can also try the light reach by brightness"): a lit placeable's reach is
//   its own, by its light colour's brightness (light_sources.cpp keeps it on it:
//   nwn5e_light_bright / _dim in cm, -1 none; a brazier 4 / 9 m, a candle 2 /
//   4.5 m), and its zone rings match; one without them (loaded before 0.38.0)
//   keeps 4 / 9 m. (Owner, 2026-10-25, on candles not lighting: "I don't mind
//   single candles being just there, but not counting as actual sources of
//   light" - left so.)
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

#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "rules/cone_slices.h"
#include "rules/idle_look.h"
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
// Once a second - but a creature that starts or stops hiding is handled at
// once (0.40.3; owner: "when turning off stealth mode it takes a while for the
// light markers, vision cones and hearing markers to disappear, can these be
// expedited?" - up to a second's wait for the next check).
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
std::unordered_map<uint32_t, int> g_colour;            // by creature: its cone's colour (ColourOf)

// #9 c: the cones cut at walls.
struct Vec3 {
  float x, y, z;
};
using ClearLineOfSightFn = int (*)(void* area, Vec3 from, Vec3 to, Vec3* hit, uint32_t* hit_object, uint32_t from_id,
                                   uint32_t to_id, int flag);
using ComputeHeightFn = float (*)(void* area, Vec3 at);
ClearLineOfSightFn g_clear_line_of_sight = nullptr;
ComputeHeightFn g_compute_height = nullptr;
int g_pass = 0;                     // the hiding player character's last pass (its nwn5e_watch_pass)
std::unordered_set<uint32_t> g_drawn;  // creatures whose rings that pass drew: the next pass measures their slices
std::unordered_map<uint32_t, int> g_slice_sig;  // by creature: the signature last written on it
constexpr const char* kVarSlice = "nwn5e_cs";         // + position: the slice's row wanted (0 none)
constexpr const char* kVarSliceSig = "nwn5e_csig";    // their signature (0: none measured - the whole cone shows)
constexpr const char* kVarSliceDrawn = "nwn5e_csh";   // + position: the row drawn (the pass script's)
constexpr const char* kVarSliceDrawnSig = "nwn5e_csigh";  // the signature drawn (0: not drawn in slices)

// #9 d: idle guards looking about.
struct Looker {
  uint64_t next = 0;  // ms: when it may look about next
  bool has_post = false;
  float post_x = 0.0f, post_y = 0.0f, post_facing = 0.0f;  // where it stands and faces between looks
  rules::IdleLookBlock last = rules::IdleLookBlock::kNone;
};
std::unordered_map<uint32_t, Looker> g_lookers;  // by creature, within reach of the hiding player character
rules::IdleLookTuning g_look;
std::mt19937 g_look_dice;
void* g_hider_area = nullptr;  // the hiding player character's area and spot, at its last pass
float g_hider_x = 0.0f, g_hider_y = 0.0f;

bool IdleLookOn() {
  return g_config.idle_look && g_config.perception == rules::RuleVariant::kFaithful && g_config.hiders_need_search &&
         g_config.vision_cone > 0 && g_config.vision_cone < 360;
}

double LookRoll() { return std::uniform_real_distribution<double>(0.0, 1.0)(g_look_dice); }

bool SlicesOn() {
  return g_config.cone_walls && g_config.stealth_rings > 0 && g_config.perception == rules::RuleVariant::kFaithful &&
         g_config.hiders_need_search && g_config.vision_cone > 0 && g_config.vision_cone < 360 &&
         g_clear_line_of_sight && g_compute_height;
}

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

// #9 c: one creature's slices, measured now - the row for each position (0: none). `tests` counts the sight lines.
void MeasureSlices(void* creature, uint32_t id, float sight, int colour, int rows[rules::kConeSlicePositions],
                   int* tests) {
  for (int k = 0; k < rules::kConeSlicePositions; ++k) rows[k] = 0;
  void* area = engine::AreaOf(creature);
  const int top = rules::ConeSliceLengthAtMost(sight);
  if (!area || top < 0) return;
  const float x = Field<float>(creature, ee::kObjectPositionX);
  const float y = Field<float>(creature, ee::kObjectPositionY);
  const float z = Field<float>(creature, ee::kObjectPositionZ);
  const float facing = atan2f(Field<float>(creature, ee::kObjectOrientationY),
                              Field<float>(creature, ee::kObjectOrientationX));
  const Vec3 eye{x, y, z + 1.0f};  // as perception looks: 1 m up, to 1 m over the ground there
  const rules::SliceSpan span = rules::ConeSliceSpan(rules::DrawnConeDegrees(g_config.vision_cone));
  for (int k = span.first; k < span.first + span.count; ++k) {
    const float a = facing + rules::ConeSliceMiddle(k) * 3.14159265f / 180.0f;
    const float cx = cosf(a), cy = sinf(a);
    Vec3 hit{0.0f, 0.0f, 0.0f};
    const auto clear = [&](int i, Vec3* blocked_at) {
      ++*tests;
      const float metres = rules::kConeSliceLengths[i];
      Vec3 to{x + cx * metres, y + cy * metres, z + 1.0f};
      if (g_compute_height) {
        // Off the area's tile grid: blocked, without asking (perception never looks there).
        const float ground = g_compute_height(area, to);
        if (ground <= ee::kAreaOffGridHeight + 0.01f) {
          if (blocked_at) *blocked_at = to;
          return false;
        }
        to.z = ground + 1.0f;
      }
      Vec3 at = to;
      uint32_t object = ee::kObjectInvalid;
      const bool open = g_clear_line_of_sight(area, eye, to, &at, &object, id, ee::kObjectInvalid, 1) == 1;
      if (blocked_at) *blocked_at = at;
      return open;
    };
    int reach = top;
    if (!clear(top, &hit)) {
      // Where the engine found it blocked, if that is on the line short of its end: the length just short of it.
      const float along = (hit.x - x) * cx + (hit.y - y) * cy;
      const float across = fabsf((hit.y - y) * cx - (hit.x - x) * cy);
      const int hint = along >= 0.0f && along < rules::kConeSliceLengths[top] - 0.05f && across < 1.0f
                           ? rules::ConeSliceLengthAtMost(along)
                           : -1;
      reach = rules::ConeSliceReach(top - 1, hint, [&](int i) { return clear(i, nullptr); });
    }
    rows[k] = reach >= 0 ? rules::ConeSliceRow(colour, k, reach) : 0;
  }
}

// #9 c: the slices of the creatures whose rings the last pass drew, written on them for this pass to put on. The cost
// (sight lines, time) logged once; each creature's lengths when they change (30 lines at most).
void MeasureDrawn(void* area) {
  if (g_drawn.empty()) return;
  LARGE_INTEGER start, stop, frequency;
  QueryPerformanceCounter(&start);
  int creatures = 0, slices = 0, tests = 0;
  for (auto it = g_drawn.begin(); it != g_drawn.end();) {
    const uint32_t id = *it;
    void* creature = CreatureById(id);
    if (!creature || engine::AreaOf(creature) != area) {
      g_slice_sig.erase(id);
      it = g_drawn.erase(it);
      continue;
    }
    ++it;
    const bool calm = Field<uint8_t>(creature, ee::kCreatureDetectMode) == 0 &&
                      !(g_config.alert_creatures_search && Field<int32_t>(creature, ee::kCreatureInCombat) != 0);
    int rows[rules::kConeSlicePositions] = {};
    if (calm && engine::GetLocalInt(creature, kVarConeRing) > 0) {
      const bool fighting = Field<int32_t>(creature, ee::kCreatureInCombat) != 0;
      const auto colour = g_colour.find(id);
      MeasureSlices(creature, id, Field<float>(creature, kSightRange + (fighting ? 4 : 0)),
                    colour != g_colour.end() ? colour->second : 2, rows, &tests);
      ++creatures;
      for (int row : rows) slices += row != 0;
    }
    const int sig = rules::ConeSliceSignature(rows);
    int& last = g_slice_sig.emplace(id, -1).first->second;
    if (sig == last) continue;
    last = sig;
    for (int k = 0; k < rules::kConeSlicePositions; ++k) {
      engine::SetLocalInt(creature, (std::string(kVarSlice) + std::to_string(k)).c_str(), rows[k]);
    }
    engine::SetLocalInt(creature, kVarSliceSig, sig);
    static int logged = 0;
    if (sig && logged < 30) {
      ++logged;
      std::string lengths;
      for (int row : rows) {
        int c = 0, k = 0, i = 0;
        if (!rules::ConeSliceRowParts(row, &c, &k, &i)) continue;
        char part[16];
        sprintf_s(part, "%s%g", lengths.empty() ? "" : " / ", rules::kConeSliceLengths[i]);
        lengths += part;
      }
      Log("stealth watch: 0x%08X cone in slices, right to left: %s m (colour %d)", id, lengths.c_str(),
          g_colour.count(id) ? g_colour[id] : 2);
    }
  }
  QueryPerformanceCounter(&stop);
  QueryPerformanceFrequency(&frequency);
  static bool costed = false;
  if (!costed && creatures > 0) {
    costed = true;
    Log("stealth watch: cone slices - %d hostiles, %d slices, %d line-of-sight tests in %.3f ms (once a second while "
        "you hide; at most the nearest %d; logged once)",
        creatures, slices, tests, 1000.0 * (stop.QuadPart - start.QuadPart) / frequency.QuadPart,
        g_config.stealth_rings);
  }
}

// The light where the hider stands (G): 0 dark, 1 dim, 2 bright, stored + 1;
// with `zones`, the light zones round the sources near it too (the player's).
std::string LightPart(int carried, bool zones) {
  const auto lum = [](const char* v) {
    const std::string s(v);
    return "(((" + s + " >> 16) & 255) * 299 + ((" + s + " >> 8) & 255) * 587 + (" + s + " & 255) * 114) / 1000";
  };
  std::string code = "int light = 0; object area = GetArea(pc); int night = GetIsNight();"
         // A light counts only with a clear line from it to the hider's chest (not through a wall).
         "vector me = GetPosition(pc) + Vector(0.0, 0.0, 1.0); vector lp;"
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
         "object q = GetFirstObjectInShape(SHAPE_SPHERE, 10.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
         "while (light < 2 && GetIsObjectValid(q)) {"
         " lp = GetPosition(q) + Vector(0.0, 0.0, 1.0);"
         // Its own reach by its light's brightness (light_sources.cpp; -1: it gives none), else 4 / 9 m.
         " int rb = GetLocalInt(q, \"nwn5e_light_bright\"); int rd = GetLocalInt(q, \"nwn5e_light_dim\");"
         " float qb = rb > 0 ? IntToFloat(rb) / 100.0 : 4.0; float qd = rd > 0 ? IntToFloat(rd) / 100.0 : 9.0;"
         " if (rb >= 0 && GetPlaceableIllumination(q) && LineOfSightVector(lp + VectorNormalize(me - lp) * 0.5, me)) {"
         "  float qq = GetDistanceBetween(pc, q);"
         "  if (qq <= qb) light = 2; else if (qq <= qd && light < 1) light = 1;"
         " }"
         " q = GetNextObjectInShape(SHAPE_SPHERE, 10.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
         "}"
         // Tile flames (their stand-ins): a bright colour bright within 4 m, dim within 7.5 m; a pale dark one dim within 4 m.
         "object tf = GetFirstObjectInShape(SHAPE_SPHERE, 8.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
         "while (light < 2 && GetIsObjectValid(tf)) {"
         " lp = GetPosition(tf);"
         " if (GetStringLeft(GetTag(tf), 11) == \"nwn5e_flame\" && !GetLocalInt(tf, \"nwn5e_torch_out\")"
         "     && LineOfSightVector(lp + VectorNormalize(me - lp) * 0.5, me)) {"
         "  float fd = GetDistanceBetween(pc, tf);"
         "  if (GetLocalInt(tf, \"nwn5e_torch_colour\") % 2 == 1) { if (fd <= 4.0) light = 2; else if (fd <= 7.5 && light < 1) light = 1; }"
         "  else if (fd <= 4.0 && light < 1) light = 1;"
         " }"
         " tf = GetNextObjectInShape(SHAPE_SPHERE, 8.0, at, FALSE, OBJECT_TYPE_PLACEABLE);"
         "}"
         "q = GetFirstObjectInShape(SHAPE_SPHERE, 20.0, at, FALSE, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
         "while (light < 2 && GetIsObjectValid(q)) {"
         " effect e = GetFirstEffect(q);"
         " while (GetIsEffectValid(e)) {"
         "  if (GetEffectType(e, TRUE) == EFFECT_TYPE_VISUALEFFECT) {"
         "   int v = GetEffectInteger(e, 0);"
         "   if (v >= 153 && v <= 180) {"
         "    float r = 5.0 * IntToFloat(1 + (v - 153) % 4); float d = GetDistanceBetween(pc, q);"
         "    lp = GetPosition(q) + Vector(0.0, 0.0, 1.0);"
         "    if (q != pc && !LineOfSightVector(lp + VectorNormalize(me - lp) * 0.5, me)) d = 999.0;"
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
         "  if (GetStringLeft(GetTag(s), 11) == \"nwn5e_flame\") {"
         "   if (!GetLocalInt(s, \"nwn5e_torch_out\")) zone = GetLocalInt(s, \"nwn5e_torch_colour\") % 2 == 1 ? 1821 * 10000 + 1831 : 1829;"
         "  }"
         "  else if (GetObjectType(s) == OBJECT_TYPE_PLACEABLE && GetPlaceableIllumination(s) && GetLocalInt(s, \"nwn5e_light_bright\") >= 0) {"
         "   zone = GetLocalInt(s, \"nwn5e_light_rings\"); if (!zone) zone = 1821 * 10000 + 1832;"
         "  }"
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
         // Only a light with a clear line to the hider shows its zone (not one in the next room).
         " if (zone) {"
         "  lp = GetPosition(s); if (GetStringLeft(GetTag(s), 11) != \"nwn5e_flame\") lp.z = lp.z + 1.0;"
         "  if (!LineOfSightVector(lp + VectorNormalize(me - lp) * 0.5, me)) zone = 0;"
         " }"
         " if (zone != GetLocalInt(s, \"nwn5e_lring\")) {"
         "  effect le = GetFirstEffect(s);"
         "  while (GetIsEffectValid(le)) { if (GetEffectTag(le) == \"nwn5e_lring\") RemoveEffect(s, le); le = GetNextEffect(s); }"
         "  if (zone) {"
         // A tile flame's rings on the floor below it.
         "   vector down = Vector(0.0, 0.0, -GetLocalFloat(s, \"nwn5e_torch_h\"));"
         "   if (zone / 10000) ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect(zone / 10000, FALSE, 1.0, down)), \"nwn5e_lring\"), s);"
         "   if (zone % 10000) ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect(zone % 10000, FALSE, 1.0, down)), \"nwn5e_lring\"), s);"
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
  const bool slices = SlicesOn();
  void* hider = CreatureById(pc);
  if (slices && hider) MeasureDrawn(engine::AreaOf(hider));
  std::string code =
      "object pc = OBJECT_SELF; location at = GetLocation(pc);"
      "int pass = GetLocalInt(pc, \"nwn5e_watch_pass\") + 1; SetLocalInt(pc, \"nwn5e_watch_pass\", pass);"
      // A ranged Snuff's mark (light_sources.cpp) goes when it stops attacking.
      "if (GetLocalInt(pc, \"nwn5e_snuffing\") && GetCurrentAction(pc) != ACTION_ATTACKOBJECT) DeleteLocalInt(pc, \"nwn5e_snuffing\");";
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
        "  int look = searching || cone == 0 ? sight : (cone > 0 ? cone : 0);";
    if (slices) {
      // #9 c: a calm one's cone in slices once they're measured (its whole cone until then).
      code +=
          "  int csig = GetLocalInt(o, \"nwn5e_ring_pass\") == pass && hear > 0 && !searching && cone > 0"
          "   ? GetLocalInt(o, \"" + std::string(kVarSliceSig) + "\") : 0;"
          "  if (csig) look = 0;";
    }
    code +=
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
    if (slices) {
      // Each slice its own effect (tag nwn5e_cs<k>), changed only when its row does.
      code +=
          "  if (csig != GetLocalInt(o, \"" + std::string(kVarSliceDrawnSig) + "\")) {"
          "   float clift = IntToFloat(GetLocalInt(o, \"" + std::string(kVarRingLift) + "\")) / 100.0;"
          "   int k = 0;"
          "   for (k = 0; k < " + Num(rules::kConeSlicePositions) + "; k++) {"
          "    string ks = IntToString(k);"
          "    int w = csig ? GetLocalInt(o, \"" + std::string(kVarSlice) + "\" + ks) : 0;"
          "    if (w != GetLocalInt(o, \"" + std::string(kVarSliceDrawn) + "\" + ks)) {"
          "     effect e = GetFirstEffect(o);"
          "     while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"nwn5e_cs\" + ks) RemoveEffect(o, e); e = GetNextEffect(o); }"
          "     if (w) ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect("
          "      EffectVisualEffect(w, FALSE, 1.0, Vector(0.0, 0.0, clift))), \"nwn5e_cs\" + ks), o);"
          "     SetLocalInt(o, \"" + std::string(kVarSliceDrawn) + "\" + ks, w);"
          "    }"
          "   }"
          "   SetLocalInt(o, \"" + std::string(kVarSliceDrawnSig) + "\", csig);"
          "  }";
    }
  }
  if (g_investigate) {
    code +=
        "  int left = GetLocalInt(o, \"nwn5e_seek\");"
        "  if (left > 0) {"
        "   if (GetIsInCombat(o) || want == 2) DeleteLocalInt(o, \"nwn5e_seek\"); else SetLocalInt(o, \"nwn5e_seek\", left - 1);"
        "  } else if (want == 1 && !GetIsInCombat(o) && GetCommandable(o) && !IsInConversation(o) && !GetPlotFlag(o)"
        "             && !GetIsObjectValid(GetMaster(o))) {"
        "   SetLocalInt(o, \"nwn5e_seek\", 25);"
        // #152's ordering (0.99.74): marked as an investigation - an illusion it sees or hears takes it over.
        "   SetLocalInt(o, \"nwn5e_seek_why\", 1);"
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
        "   AssignCommand(o, ActionDoCommand(DeleteLocalInt(OBJECT_SELF, \"nwn5e_seek_why\")));"
        "   SetLocalInt(pc, \"nwn5e_seek_started\", GetLocalInt(pc, \"nwn5e_seek_started\") + 1);"
        "  }";
  }
  code +=
      " }"
      " o = GetNextObjectInShape(SHAPE_SPHERE, 50.0, at, FALSE, OBJECT_TYPE_CREATURE);"
      "}";
  Run(code, pc, "watch");
  if (hider) g_pass = engine::GetLocalInt(hider, "nwn5e_watch_pass");
}

// Every marker and ring in the character's area off.
void Clear(uint32_t pc) {
  Run("object area = GetArea(OBJECT_SELF);"
      "object o = GetFirstObjectInArea(area, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
      "while (GetIsObjectValid(o)) {"
      " if (GetLocalInt(o, \"nwn5e_noticed\") || GetLocalInt(o, \"nwn5e_ring\") || GetLocalInt(o, \"nwn5e_lring\")"
      "     || GetLocalInt(o, \"nwn5e_csigh\")) {"
      "  effect e = GetFirstEffect(o);"
      "  while (GetIsEffectValid(e)) {"
      "   string t = GetEffectTag(e);"
      "   if (t == \"nwn5e_noticed\" || t == \"nwn5e_ring\" || t == \"nwn5e_lring\" || GetStringLeft(t, 8) == \"nwn5e_cs\")"
      "    RemoveEffect(o, e);"
      "   e = GetNextEffect(o);"
      "  }"
      "  DeleteLocalInt(o, \"nwn5e_noticed\"); DeleteLocalInt(o, \"nwn5e_ring\"); DeleteLocalInt(o, \"nwn5e_lring\");"
      "  int k = 0; for (k = 0; k < 10; k++) DeleteLocalInt(o, \"nwn5e_csh\" + IntToString(k));"
      "  DeleteLocalInt(o, \"nwn5e_csigh\"); DeleteLocalInt(o, \"nwn5e_csig\");"
      " }"
      " o = GetNextObjectInArea(area, OBJECT_TYPE_CREATURE | OBJECT_TYPE_PLACEABLE);"
      "}"
      "DeleteLocalInt(OBJECT_SELF, \"nwn5e_light_level\"); DeleteLocalInt(OBJECT_SELF, \"nwn5e_light_carried\");"
      "effect g = GetFirstEffect(OBJECT_SELF);"
      "while (GetIsEffectValid(g)) { if (GetEffectTag(g) == \"nwn5e_ghost\") RemoveEffect(OBJECT_SELF, g); g = GetNextEffect(OBJECT_SELF); }",
      pc, "clearing");
  SetStealthOpacity(pc, 0.0f);
  // #9 c: nothing drawn now - the next hide measures afresh.
  g_drawn.clear();
  g_slice_sig.clear();
  g_pass = 0;
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
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  if (now < watch.next && hiding == watch.on) return;  // a change: at once (see kCheckMs)
  watch.next = now + kCheckMs;
  void* area = engine::AreaOf(creature);
  if (!area) return;
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

// #9 d: a creature near the hiding player character, each AI pass - when its next look is due, it looks about if
// nothing keeps it from it (rules::IdleLookBlocker), turned from its post's facing and back to it.
void IdleLook(void* creature, uint32_t id) {
  if (!g_hider || !g_hider_area || id == g_hider) return;
  const float x = Field<float>(creature, ee::kObjectPositionX), y = Field<float>(creature, ee::kObjectPositionY);
  const float metres = sqrtf((x - g_hider_x) * (x - g_hider_x) + (y - g_hider_y) * (y - g_hider_y));
  if (engine::AreaOf(creature) != g_hider_area || metres > rules::kIdleLookReach) {
    if (!g_lookers.empty()) g_lookers.erase(id);
    return;
  }
  const uint64_t now = GetTickCount64();
  auto found = g_lookers.find(id);
  if (found == g_lookers.end()) {
    // First near the hider: its first look a whole wait from now, not at once when you hide.
    Looker fresh;
    fresh.next = now + rules::IdleLookWaitMs(g_look, LookRoll());
    g_lookers.emplace(id, fresh);
    return;
  }
  Looker& looker = found->second;
  if (now < looker.next) return;
  looker.next = now + rules::IdleLookWaitMs(g_look, LookRoll());
  const double wait = (looker.next - now) / 1000.0;
  rules::IdleLookFacts facts;
  facts.player = Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0;
  const uint32_t master = Field<uint32_t>(creature, ee::kCreatureMaster);
  facts.associate = master != 0 && master != ee::kObjectInvalid;
  facts.dead = engine::IsDeadOrDying(creature);
  facts.hostile = engine::IsEnemy(creature, g_hider);
  facts.fighting = Field<int32_t>(creature, ee::kCreatureInCombat) != 0;
  facts.searching = Field<uint8_t>(creature, ee::kCreatureDetectMode) != 0;
  facts.investigating = engine::GetLocalInt(creature, "nwn5e_seek") > 0;
  facts.hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  facts.busy = !engine::HasNoOrders(creature);
  facts.metres = metres;
  const rules::IdleLookBlock block = rules::IdleLookBlocker(facts);
  static int blocks_logged = 0;
  if (block != rules::IdleLookBlock::kNone) {
    if (block != looker.last && facts.hostile && blocks_logged < 60) {
      ++blocks_logged;
      Log("stealth watch: 0x%08X doesn't look about - %s (asked again in %.1f s)", id, rules::IdleLookBlockName(block),
          wait);
    }
    looker.last = block;
    if (looker.has_post && rules::IdleLookMovedOff(looker.post_x, looker.post_y, x, y)) looker.has_post = false;
    return;
  }
  const float facing = rules::FacingDegrees(atan2f(Field<float>(creature, ee::kObjectOrientationY),
                                                   Field<float>(creature, ee::kObjectOrientationX)) *
                                            180.0f / 3.14159265f);
  if (!looker.has_post || rules::IdleLookMovedOff(looker.post_x, looker.post_y, x, y)) {
    looker.has_post = true;
    looker.post_x = x;
    looker.post_y = y;
    looker.post_facing = facing;
  }
  const bool left = LookRoll() < 0.5;
  const float turn = rules::IdleLookTurn(g_look, LookRoll(), left);
  const float to = rules::FacingDegrees(looker.post_facing + turn);
  char code[600];
  sprintf_s(code,
            "object o = OBJECT_SELF; int done = 2;"
            "if (!IsInConversation(o) && GetCommandable(o)) {"
            " ActionDoCommand(SetFacing(%.1f));"
            " ActionPlayAnimation(ANIMATION_LOOPING_LOOK_FAR, 1.0, %d.0);"
            " ActionDoCommand(SetFacing(%.1f));"
            " done = 1;"
            "}"
            "SetLocalInt(o, \"nwn5e_look_done\", done);",
            to, g_look.hold_seconds, looker.post_facing);
  Run(code, id, "idle look");
  if (engine::GetLocalInt(creature, "nwn5e_look_done") != 1) {
    if (looker.last != rules::IdleLookBlock::kBusy && blocks_logged < 60) {
      ++blocks_logged;
      Log("stealth watch: 0x%08X doesn't look about - in a conversation or not commandable (asked again in %.1f s)", id,
          wait);
    }
    looker.last = rules::IdleLookBlock::kBusy;
    return;
  }
  looker.last = block;
  static int looks_logged = 0;
  if (looks_logged < 300) {
    ++looks_logged;
    Log("stealth watch: 0x%08X looks about - %+.0f degrees (%s; facing %.0f -> %.0f), holds %d s, back to %.0f; "
        "its next look in %.1f s%s",
        id, turn, left ? "left" : "right", looker.post_facing, to, g_look.hold_seconds, looker.post_facing, wait,
        looks_logged == 300 ? " (further looks not logged)" : "");
  }
}

// Every creature, each AI pass: its ring rows as locals, written when they change.
void WriteRanges(void* creature, uint32_t id) {
  const bool fighting = Field<int32_t>(creature, ee::kCreatureInCombat) != 0;
  const float sight = Field<float>(creature, kSightRange + (fighting ? 4 : 0));
  const float hear = Field<float>(creature, kHearRange + (fighting ? 4 : 0));
  const int sight_row = RingRow(kSightRingRow, sight), hear_row = RingRow(kHearRingRow, hear);
  // The cone's colour: this creature's look against the hiding character, once a second.
  int& colour = g_colour.emplace(id, 2).first->second;
  // #9 c: drawn in the last pass - its slices are measured in the next.
  if (g_hider && g_hider != id && g_pass > 0 && engine::GetLocalInt(creature, "nwn5e_ring_pass") == g_pass) {
    g_drawn.insert(id);
  } else if (!g_drawn.empty()) {
    g_drawn.erase(id);
  }
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
    if (PlayersHenchman(creature)) {
      HenchmanPass(creature, id);
    } else if (g_hider && IdleLookOn()) {
      IdleLook(creature, id);
    }
    return;
  }
  Watch& watch = g_watches[id];
  const uint64_t now = GetTickCount64();
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  if (now < watch.next && hiding == watch.on) return;  // a change: at once (see kCheckMs)
  watch.next = now + kCheckMs;
  void* area = engine::AreaOf(creature);
  if (!area) return;
  if (hiding) {
    g_hider = id;
    g_hider_area = area;
    g_hider_x = Field<float>(creature, ee::kObjectPositionX);
    g_hider_y = Field<float>(creature, ee::kObjectPositionY);
  } else if (g_hider == id) {
    g_hider = 0;
    g_hider_area = nullptr;
    g_lookers.clear();  // the next hide starts the looks afresh
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

// #9 c: the rule's side of the cut cones (rules_perception.cpp) - whether `observer`'s cone, drawn in slices, keeps
// `hider` out of its sight: outside every drawn slice, or past the length of the one its bearing falls in (`metres`
// how far it is, `reach` that slice's length, 0 for none). False when its cone isn't drawn in slices - the rows read
// are the ones drawn, so the rule and the drawing agree.
bool ConeCutsSight(void* observer, void* hider, float* metres, float* reach) {
  *metres = 0.0f;
  *reach = 0.0f;
  if (!SlicesOn() || !observer || !hider || engine::GetLocalInt(observer, kVarSliceDrawnSig) == 0) return false;
  const float dx = Field<float>(hider, ee::kObjectPositionX) - Field<float>(observer, ee::kObjectPositionX);
  const float dy = Field<float>(hider, ee::kObjectPositionY) - Field<float>(observer, ee::kObjectPositionY);
  const float off = rules::DegreesOffFacing(dx, dy, Field<float>(observer, ee::kObjectOrientationX),
                                            Field<float>(observer, ee::kObjectOrientationY));
  *metres = sqrtf(dx * dx + dy * dy);
  int rows[rules::kConeSlicePositions] = {};
  const int k = rules::ConeSliceAt(off);
  if (k >= 0) {
    rows[k] = engine::GetLocalInt(observer, (std::string(kVarSliceDrawn) + std::to_string(k)).c_str());
    int c = 0, position = 0, length = 0;
    if (rules::ConeSliceRowParts(rows[k], &c, &position, &length)) *reach = rules::kConeSliceLengths[length];
  }
  return !rules::SeenInsideSlices(rows, off, *metres);
}

void InstallStealthWatch() {
  g_investigate = g_config.heard_investigates && g_config.perception == rules::RuleVariant::kFaithful;
  if (!g_config.stealth_marker && g_config.stealth_rings <= 0 && !g_investigate) return;
  g_clear_line_of_sight = reinterpret_cast<ClearLineOfSightFn>(EngineExport(ee::kClearLineOfSight));
  rules::IdleLookTuning look;
  look.min_seconds = g_config.idle_look_min_seconds;
  look.max_seconds = g_config.idle_look_max_seconds;
  look.min_degrees = g_config.idle_look_min_degrees;
  look.max_degrees = g_config.idle_look_max_degrees;
  look.hold_seconds = g_config.idle_look_hold;
  g_look = rules::SaneIdleLook(look);
  g_look_dice.seed(static_cast<uint32_t>(GetTickCount64()));
  g_compute_height = reinterpret_cast<ComputeHeightFn>(EngineExport(ee::kComputeHeight));
  if (AddCreatureUpdateListener(&Update)) {
    Log("stealth watch: while a player character hides - markers %s, rings on the nearest %d, heard -> investigate %s",
        g_config.stealth_marker ? "on" : "off", g_config.stealth_rings, g_investigate ? "on (2024)" : "off");
    // #9 c's launch line: its key and what it does.
    if (!g_config.cone_walls) {
      Log("stealth watch: vision cones cut at walls off (cone_walls=0) - whole cones, drawn through walls");
    } else if (!SlicesOn()) {
      Log("stealth watch: vision cones cut at walls - cone_walls=1 but not drawn: %s",
          !g_clear_line_of_sight || !g_compute_height ? "NO CNWSArea::ClearLineOfSight / ComputeHeight export"
          : g_config.stealth_rings <= 0 ? "stealth_rings=0" : "no calm cones (perception, hiders_need_search, vision_cone)");
    } else {
      Log("stealth watch: vision cones cut at walls - cone_walls=1: %d-degree slices (%d for a %d-degree cone), each "
          "its line of sight (CNWSArea::ClearLineOfSight, to 1 m over the ground there), 1-50 m in %d steps, rows "
          "%d-%d; the nearest %d hostiles, "
          "once a second",
          static_cast<int>(rules::kConeSliceDegrees),
          rules::ConeSliceSpan(rules::DrawnConeDegrees(g_config.vision_cone)).count,
          rules::DrawnConeDegrees(g_config.vision_cone),
          rules::kConeSliceLengthCount, rules::kConeSliceRow, rules::kConeSliceRowLast, g_config.stealth_rings);
    }
    // #9 d's launch line.
    if (!g_config.idle_look) {
      Log("stealth watch: idle looks off (idle_look=0) - calm enemies keep facing one way");
    } else if (!IdleLookOn()) {
      Log("stealth watch: idle looks - idle_look=1 but off: no calm vision cones (perception, hiders_need_search, "
          "vision_cone)");
    } else {
      Log("stealth watch: idle looks - idle_look=1 idle_look_min_seconds=%d idle_look_max_seconds=%d "
          "idle_look_min_degrees=%d idle_look_max_degrees=%d idle_look_hold=%d: calm hostiles within %.0f m of a "
          "hiding player character turn left or right, hold the look and turn back to their post",
          g_look.min_seconds, g_look.max_seconds, g_look.min_degrees, g_look.max_degrees, g_look.hold_seconds,
          rules::kIdleLookReach);
    }
  }
}
