// Cover from objects (#166 part 2; owner, 2026-11-23: "the results from CV are that from all those, the only one that
// blocks LoS is the closed door, all others leave everything clear" - then "A, and apply cover to all three").
// - The table: data\placeable_bounds.txt beside the DLL - each placeables.2da row's model's visible bounds, made at build
//   time from the player's own game (tools/data/placeable_bounds.js, through mdlmesh --bounds) and never committed -
//   read at launch (InstallCoverObjects): its rows and misses logged.
// - The boxes: an area's placeables listed every 3 s at most when asked (a script on a creature there: id, scale,
//   resref - not plc_invisobj, nwn5e's own markers (tags nwn5e_*), Spiritual Weapon's force (n5s_spiritforce), an
//   illusion's image (n5c_image)), each one's appearance read from the object (CNWSPlaceable +0x32c, uint16, as
//   light_sources.cpp reads it) and looked up; no bounds, or under 0.3 m tall: no box (a miss logged once per
//   appearance). Its box: the bounds turned by its facing (+0xb0) and scaled, at its place (rules::BoxOf). Static
//   placeables count: the server loads them as objects (LoadPlaceable's "Static", light_sources.cpp +0x5a0), and the
//   area's list holds them. Plot or not doesn't matter.
// - A line: blocked when the engine's (CNWSArea::ClearLineOfSight: the tileset's walls, shut doors) is, or a box is in
//   the way (rules::SegmentHitsBox, each box culled by the segment's bounds; a box the line starts inside doesn't
//   count - the looker's own table edge). At most kBoxTestsPerSecond box tests a second for every line together
//   (past it, lines count as clear for the rest of that second); the cost logged once.
// - Cover: the seven body points !los uses (rules::kLosBody: feet, knees, waist, chest, head, shoulders), spread over
//   the target's height by its size and lowered to a crouch in stealth mode (rules::ScaledBodyPoint), from the
//   observer's eyes by its size (rules::EyeHeight: Medium 1.6 m) - the blocked ones counted (rules::CoverFromBlocked:
//   4 Half, 5-6 Three-Quarters, 7 Total; owner, 2026-11-23: "Your recommendations are good"); kept a second while
//   neither moves.
// - Uses: !los (los_command.cpp), the cones' slices (stealth_watch.cpp), a hider behind Three-Quarters or Total Cover
//   unseen (rules_perception.cpp), attacks' AC and Total Cover's refusal (rules_cover.cpp, action_order.cpp), and
//   Dexterity saves from the effect's origin (here: SaveCoverBonus, from rules_save_roll.cpp); #184: the boxes round a
//   spell's point of origin, and the origin itself, for its particle mask (spell_mask.cpp: ObjectBoxesAround,
//   SpellOriginOf; !maskproof's ObjectBoxesNear).

#include "object_cover.h"

#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "player_text.h"
#include "rules/area_origin.h"
#include "rules/cover_objects.h"
#include "rules/los_probe.h"
#include "rules/reposition.h"
#include "rules/spells2024.h"
#include "spell_mask.h"

void* GameObjectById(uint32_t id);       // light_sources.cpp (the NWS object)
bool HoldsRangedWeapon(void* creature);  // light_sources.cpp (baseitems.2da RangedWeapon)

namespace {

struct Vec3 {
  float x, y, z;
};
using ClearLineOfSightFn = int (*)(void* area, Vec3 from, Vec3 to, Vec3* hit, uint32_t* hit_object, uint32_t from_id,
                                   uint32_t to_id, int flag);
using PlaceableByIdFn = void* (*)(void* app, uint32_t id);
using ComputeHeightFn = float (*)(void* area, Vec3 at);
constexpr const char* kGetPlaceableById = "?GetPlaceableByGameObjectID@CServerExoApp@@QEAAPEAVCNWSPlaceable@@I@Z";
constexpr size_t kPlaceableAppearance = 0x32c;  // uint16 (placeables.2da; light_sources.cpp)
constexpr size_t kSpellTargetX = 0x208;  // CNWSObject: the last spell's target location, float x y z (GetSpellTargetLocation)
constexpr uint64_t kRefreshMs = 3000;
constexpr uint64_t kKeepMs = 1000;       // a cover kept while neither moves
constexpr uint64_t kTellEveryMs = 6000;  // a round
constexpr int kBoxTestsPerSecond = 50000;

struct Box {
  uint32_t id = 0;
  rules::ObjectBox box;
};
struct AreaBoxes {
  uint64_t at = 0;
  std::vector<Box> boxes;
};
struct Kept {
  uint64_t at = 0;
  float a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
  rules::Cover cover = rules::Cover::kNone;
  std::string by;
  uint32_t by_id = 0;
  bool crouched = false;
};

std::map<int, rules::ModelBounds> g_bounds;  // placeables.2da row -> its model's bounds
bool g_table = false;
std::unordered_map<void*, AreaBoxes> g_areas;
std::unordered_map<uint32_t, std::string> g_names;  // placeable id -> resref
std::set<int> g_missed;                             // appearances logged without bounds
std::unordered_map<uint64_t, Kept> g_kept;          // (observer, target) -> its cover
std::unordered_map<uint64_t, uint64_t> g_told;      // (kind, a, b) -> when told
ClearLineOfSightFn g_clear_line = nullptr;
PlaceableByIdFn g_placeable_by_id = nullptr;
ComputeHeightFn g_compute_height = nullptr;
std::unordered_map<uint64_t, uint64_t> g_repositioned;  // (attacker, target) -> when its move was queued
void** g_app_manager = nullptr;
int g_logged = 0;

struct Cost {
  uint64_t start = 0;
  int lines = 0, tests = 0, hits = 0;
  double ms = 0;
  bool logged = false, capped = false;
};
Cost g_cost;

uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }
uint64_t Pair(uint32_t a, uint32_t b) { return (static_cast<uint64_t>(a) << 32) | b; }

std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  const size_t slash = directory.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L"." : directory.substr(0, slash);
}

void* PlaceableById(uint32_t id) {
  if (!g_placeable_by_id || !g_app_manager || !*g_app_manager) return nullptr;
  void* app = Field<void*>(*g_app_manager, ee::kAppManagerServerApp);
  return app ? g_placeable_by_id(app, id) : nullptr;
}

// Once a round for each (kind, a, b).
bool TellNow(int kind, uint32_t a, uint32_t b) {
  const uint64_t now = GetTickCount64();
  if (g_told.size() > 1024) g_told.clear();
  uint64_t& told = g_told[Pair(a, b) ^ (static_cast<uint64_t>(kind) << 56)];
  if (told && now < told + kTellEveryMs) return false;
  told = now;
  return true;
}

bool LogSlotHere() {
  if (!g_config.log_attacks || g_logged >= 300) return false;
  ++g_logged;
  return true;
}

// The area's placeables as boxes, listed again when older than kRefreshMs.
const std::vector<Box>& BoxesOf(void* area, uint32_t self) {
  AreaBoxes& cache = g_areas[area];
  const uint64_t now = GetTickCount64();
  if (cache.at && now < cache.at + kRefreshMs) return cache.boxes;
  void* me = CreatureById(self);
  if (!me || engine::AreaOf(me) != area) return cache.boxes;
  cache.at = now;
  cache.boxes.clear();
  const int ran = RunScriptChunk(
      "object a = GetArea(OBJECT_SELF); string s = \"\"; object o = GetFirstObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
      "while (GetIsObjectValid(o)) {"
      " string r = GetResRef(o); string t = GetTag(o);"
      " if (r != \"plc_invisobj\" && t != \"n5s_spiritforce\" && t != \"n5c_image\" && GetStringLeft(t, 6) != \"nwn5e_\")"
      "  s += ObjectToString(o) + \" \" + FloatToString(GetObjectVisualTransform(o, OBJECT_VISUAL_TRANSFORM_SCALE), 0, 3)"
      "   + \" \" + r + \";\";"
      " o = GetNextObjectInArea(a, OBJECT_TYPE_PLACEABLE);"
      "}"
      "SetLocalString(OBJECT_SELF, \"nwn5e_cover_list\", s);",
      self);
  static bool failed_told = false;
  if (ran != 0 && !failed_told) {
    failed_told = true;
    Log("cover objects: the placeables' listing script failed (%d) - no boxes", ran);
  }
  const std::string list = engine::GetLocalString(me, "nwn5e_cover_list");
  int listed = 0, missing = 0, small = 0;
  std::string example;
  std::istringstream entries(list);
  std::string entry;
  while (std::getline(entries, entry, ';')) {
    std::istringstream words(entry);
    std::string hex, resref;
    float scale = 1.0f;
    if (!(words >> hex >> scale >> resref)) continue;
    ++listed;
    const uint32_t id = static_cast<uint32_t>(strtoul(hex.c_str(), nullptr, 16));
    void* placeable = PlaceableById(id);
    if (!placeable) continue;
    const int appearance = Field<uint16_t>(placeable, kPlaceableAppearance);
    const auto found = g_bounds.find(appearance);
    if (found == g_bounds.end()) {
      ++missing;
      if (g_missed.insert(appearance).second && g_missed.size() <= 40) {
        Log("cover objects: %s (appearance %d) - no bounds in data\\placeable_bounds.txt: no cover from it (logged once)",
            resref.c_str(), appearance);
      }
      continue;
    }
    if (!rules::BoundsCount(found->second)) {
      ++small;
      continue;
    }
    Box box;
    box.id = id;
    box.box = rules::BoxOf(found->second, Field<float>(placeable, ee::kObjectPositionX),
                           Field<float>(placeable, ee::kObjectPositionY), Field<float>(placeable, ee::kObjectPositionZ),
                           Field<float>(placeable, ee::kObjectOrientationX),
                           Field<float>(placeable, ee::kObjectOrientationY), scale);
    cache.boxes.push_back(box);
    g_names[id] = resref;
    if (example.empty()) {
      char e[160];
      sprintf_s(e, "%s appearance %d: %.2f x %.2f x %.2f m", resref.c_str(), appearance, 2 * box.box.half[0],
                2 * box.box.half[1], 2 * box.box.half[2]);
      example = e;
    }
  }
  static int logged = 0;
  if (logged < 12) {
    ++logged;
    Log("cover objects: an area listed - %d placeables, %zu boxes (%d without bounds, %d under %.1f m)%s%s", listed,
        cache.boxes.size(), missing, small, rules::kCoverMinHeight, example.empty() ? "" : "; e.g. ", example.c_str());
  }
  return cache.boxes;
}

void CountCost(uint64_t now, double ms, int tests, bool hit) {
  if (now >= g_cost.start + 1000) {
    if (!g_cost.logged && g_cost.lines > 0) {
      g_cost.logged = true;
      Log("cover objects: in a second, %d lines against the boxes - %d box tests, %d blocked, %.2f ms in all (at most "
          "%d box tests a second; logged once)",
          g_cost.lines, g_cost.tests, g_cost.hits, g_cost.ms, kBoxTestsPerSecond);
    }
    g_cost.start = now;
    g_cost.lines = g_cost.tests = g_cost.hits = 0;
    g_cost.ms = 0;
  }
  ++g_cost.lines;
  g_cost.tests += tests;
  g_cost.hits += hit ? 1 : 0;
  g_cost.ms += ms;
}

// A script's answer: an object's name (GetName), "" when gone.
std::string NameOf(uint32_t self, uint32_t id) {
  void* me = CreatureById(self);
  if (!me) return "";
  char code[200];
  sprintf_s(code, "SetLocalString(OBJECT_SELF, \"nwn5e_cover_name\", GetName(StringToObject(\"%x\")));", id);
  RunScriptChunk(code, self);
  return engine::GetLocalString(me, "nwn5e_cover_name");
}

// The words for what covers: a placeable's name, else "a wall" (the engine's line).
std::string ByWords(uint32_t self, uint32_t by_id) {
  if (by_id == ee::kObjectInvalid) return PlayerText("cover.by_wall");
  const std::string name = NameOf(self, by_id);
  return name.empty() ? PlayerText("cover.by_wall") : name;
}

const char* CoverLogWords(rules::Cover cover) {
  switch (cover) {
    case rules::Cover::kHalf: return "Half Cover";
    case rules::Cover::kThreeQuarters: return "Three-Quarters Cover";
    case rules::Cover::kTotal: return "Total Cover";
    case rules::Cover::kNone: break;
  }
  return "no cover";
}

bool IsPlayerCharacter(void* creature) {
  void* stats = StatsOf(creature);
  return stats && ReadCreatureFacts(stats).is_player;
}

// #166 decision 3 (owner, 2026-11-23): an AI attacker refused by Total Cover moves - a ranged one sideways to a spot its
// own sight test finds a line from (rules::SidestepSpots: 1.5, 3, 4.5 m to either side; no Total Cover from there),
// else closer; a melee one round the obstacle to its target (the engine's path). Queued once a round per attacker and
// target - its actions cleared, the move, then the attack - outside the engine's action processing (DelayCommand).
void Reposition(void* creature, void* target, bool ranged) {
  const uint32_t a = IdOf(creature), t = IdOf(target);
  const uint64_t now = GetTickCount64();
  if (g_repositioned.size() > 512) g_repositioned.clear();
  uint64_t& queued = g_repositioned[Pair(a, t)];
  if (queued && now < queued + kTellEveryMs) return;  // its move is under way: the attack dropped, nothing new
  queued = now;
  void* area = engine::AreaOf(creature);
  const float ax = Field<float>(creature, ee::kObjectPositionX), ay = Field<float>(creature, ee::kObjectPositionY);
  const float tx = Field<float>(target, ee::kObjectPositionX), ty = Field<float>(target, ee::kObjectPositionY);
  const float eye = rules::EyeHeight(Field<int32_t>(creature, ee::kCreatureSize));
  std::vector<rules::Sidestep> spots;
  std::vector<bool> clear;
  std::vector<float> heights;
  if (ranged && area) {
    spots = rules::SidestepSpots(ax, ay, tx, ty);
    for (const rules::Sidestep& spot : spots) {
      float ground = Field<float>(creature, ee::kObjectPositionZ);
      if (g_compute_height) ground = g_compute_height(area, Vec3{spot.x, spot.y, ground});
      heights.push_back(ground);
      if (ground <= ee::kAreaOffGridHeight + 0.01f) {
        clear.push_back(false);
        continue;
      }
      const float from[3] = {spot.x, spot.y, ground + eye};
      CoverPoint points[rules::kLosBodyPoints];
      clear.push_back(CoverFrom(area, a, a, from, target, points, nullptr, nullptr) != rules::Cover::kTotal);
    }
  }
  int pick = -1;
  const rules::Reposition choice = rules::ChooseReposition(ranged, clear, &pick);
  char code[700];
  char what[160];
  if (choice == rules::Reposition::kSidestep) {
    const rules::Sidestep& spot = spots[static_cast<size_t>(pick)];
    sprintf_s(code,
              "object t = StringToObject(\"%x\");"
              "location at = Location(GetArea(OBJECT_SELF), Vector(%.2f, %.2f, %.2f), GetFacing(OBJECT_SELF));"
              "DelayCommand(0.10, ClearAllActions());"
              "DelayCommand(0.11, ActionMoveToLocation(at, TRUE));"
              "DelayCommand(0.12, ActionAttack(t));",
              t, spot.x, spot.y, heights[static_cast<size_t>(pick)]);
    sprintf_s(what, "steps %.1f m aside to (%.1f, %.1f) - a line from there", sqrtf((spot.x - ax) * (spot.x - ax) +
                                                                                   (spot.y - ay) * (spot.y - ay)),
              spot.x, spot.y);
  } else {
    sprintf_s(code,
              "object t = StringToObject(\"%x\");"
              "DelayCommand(0.10, ClearAllActions());"
              "DelayCommand(0.11, ActionMoveToObject(t, TRUE, %.1f));"
              "DelayCommand(0.12, ActionAttack(t));",
              t, choice == rules::Reposition::kCloseIn ? 2.0f : 1.0f);
    sprintf_s(what, "%s", choice == rules::Reposition::kCloseIn
                              ? "closes in (no spot within 5 m to either side has a line)"
                              : "walks round the obstacle to it");
  }
  RunScriptChunk(code, a);
  if (LogSlotHere()) {
    Log("cover: 0x%08X's %s attack on 0x%08X refused - Total Cover; it %s", a, ranged ? "ranged" : "melee", t, what);
  }
}

}  // namespace

bool CoverObjectsOn() {
  return g_table && g_config.cover_objects && g_config.mode == rules::RuleVariant::kFaithful;
}

std::string ObjectName(uint32_t id) {
  const auto it = g_names.find(id);
  return it == g_names.end() ? "" : it->second;
}

std::vector<NearObjectBox> ObjectBoxesNear(void* creature, float metres) {
  std::vector<NearObjectBox> out;
  void* area = creature ? engine::AreaOf(creature) : nullptr;
  if (!CoverObjectsOn() || !area) return out;
  const float x = Field<float>(creature, ee::kObjectPositionX), y = Field<float>(creature, ee::kObjectPositionY);
  for (const Box& box : BoxesOf(area, IdOf(creature))) {
    const float d = hypotf(box.box.centre[0] - x, box.box.centre[1] - y);
    if (d > metres) continue;
    NearObjectBox found;  // not "near": windows.h's macro
    found.id = box.id;
    found.name = ObjectName(box.id);
    found.box = box.box;
    found.metres = d;
    out.push_back(found);
  }
  std::sort(out.begin(), out.end(), [](const NearObjectBox& a, const NearObjectBox& b) { return a.metres < b.metres; });
  return out;
}

std::vector<NearObjectBox> ObjectBoxesAround(void* area, uint32_t lister, const float at[3], float metres) {
  std::vector<NearObjectBox> out;
  if (!CoverObjectsOn() || !area) return out;
  for (const Box& box : BoxesOf(area, lister)) {
    if (!PlaceableById(box.id)) continue;
    const float d = rules::BoxGroundDistance(at, box.box);
    if (d > metres) continue;
    NearObjectBox found;
    found.id = box.id;
    found.name = ObjectName(box.id);
    found.box = box.box;
    found.metres = d;
    out.push_back(found);
  }
  std::sort(out.begin(), out.end(), [](const NearObjectBox& a, const NearObjectBox& b) { return a.metres < b.metres; });
  return out;
}

bool ObjectsBlockLine(void* area, uint32_t self, const float a[3], const float b[3], uint32_t* blocker, float* t) {
  if (!CoverObjectsOn() || !area) return false;
  const uint64_t now = GetTickCount64();
  if (now < g_cost.start + 1000 && g_cost.tests >= kBoxTestsPerSecond) {
    if (!g_cost.capped) {
      g_cost.capped = true;
      Log("cover objects: %d box tests in a second - the rest of that second's lines count as clear (logged once)",
          kBoxTestsPerSecond);
    }
    return false;
  }
  LARGE_INTEGER start, stop, frequency;
  QueryPerformanceCounter(&start);
  const std::vector<Box>& boxes = BoxesOf(area, self);
  int tests = 0;
  bool hit = false;
  float best = 2.0f;
  for (const Box& box : boxes) {
    if (!rules::SegmentNearBox(a, b, box.box)) continue;
    ++tests;
    if (rules::PointInBox(a, box.box)) continue;  // the looker stands in it: its own table's edge
    float at = 0.0f;
    if (rules::SegmentHitsBox(a, b, box.box, &at) && at < best) {
      best = at;
      hit = true;
      if (blocker) *blocker = box.id;
    }
  }
  if (t && hit) *t = best;
  QueryPerformanceCounter(&stop);
  QueryPerformanceFrequency(&frequency);
  CountCost(now, 1000.0 * (stop.QuadPart - start.QuadPart) / frequency.QuadPart, tests, hit);
  return hit;
}

rules::Cover CoverFrom(void* area, uint32_t from_id, uint32_t self, const float eye[3], void* target,
                       CoverPoint points[rules::kLosBodyPoints], std::string* by, uint32_t* by_id) {
  if (by) by->clear();
  if (by_id) *by_id = ee::kObjectInvalid;
  if (!area || !target) return rules::Cover::kNone;
  const uint32_t target_id = IdOf(target);
  const float tx = Field<float>(target, ee::kObjectPositionX), ty = Field<float>(target, ee::kObjectPositionY),
              tz = Field<float>(target, ee::kObjectPositionZ);
  const int size = Field<int32_t>(target, ee::kCreatureSize);
  const bool crouched = Field<uint8_t>(target, ee::kCreatureStealthMode) == 1;
  std::map<uint32_t, int> blockers;
  int blocked = 0;
  for (int i = 0; i < rules::kLosBodyPoints; ++i) {
    CoverPoint& p = points[i];
    p = CoverPoint();
    float to[3];
    const rules::LosBodyPoint body = rules::ScaledBodyPoint(i, size, crouched);
    rules::LosPointAt(body, eye[0], eye[1], tx, ty, tz, &to[0], &to[1], &to[2]);
    if (g_clear_line) {
      Vec3 hit{to[0], to[1], to[2]};
      uint32_t object = ee::kObjectInvalid;
      const bool open = g_clear_line(area, Vec3{eye[0], eye[1], eye[2]}, Vec3{to[0], to[1], to[2]}, &hit, &object,
                                     from_id, target_id, 1) == 1;
      if (!open) {
        p.clear = false;
        p.blocker = ee::kObjectInvalid;
      }
    }
    uint32_t box = 0;
    if (p.clear && ObjectsBlockLine(area, self, eye, to, &box, nullptr)) {
      p.clear = false;
      p.object = true;
      p.blocker = box;
    }
    if (!p.clear) {
      ++blocked;
      ++blockers[p.object ? p.blocker : ee::kObjectInvalid];
    }
  }
  int most = 0;
  for (const auto& b : blockers) {
    if (b.second > most) {
      most = b.second;
      if (by) *by = b.first == ee::kObjectInvalid ? "the tileset" : ObjectName(b.first);
      if (by_id) *by_id = b.first;
    }
  }
  return rules::CoverFromBlocked(blocked);
}

rules::Cover CoverBetween(void* observer, void* target, std::string* by, uint32_t* by_id) {
  if (by) by->clear();
  if (by_id) *by_id = ee::kObjectInvalid;
  if (!observer || !target || !CoverObjectsOn()) return rules::Cover::kNone;
  void* area = engine::AreaOf(observer);
  if (!area || engine::AreaOf(target) != area) return rules::Cover::kNone;
  const float eye[3] = {Field<float>(observer, ee::kObjectPositionX), Field<float>(observer, ee::kObjectPositionY),
                        Field<float>(observer, ee::kObjectPositionZ) +
                            rules::EyeHeight(Field<int32_t>(observer, ee::kCreatureSize))};
  const float at[3] = {Field<float>(target, ee::kObjectPositionX), Field<float>(target, ee::kObjectPositionY),
                       Field<float>(target, ee::kObjectPositionZ)};
  const uint32_t id = IdOf(observer);
  const uint64_t now = GetTickCount64();
  if (g_kept.size() > 2048) g_kept.clear();
  Kept& kept = g_kept[Pair(id, IdOf(target))];
  bool same = kept.at && now < kept.at + kKeepMs;
  for (int k = 0; k < 3 && same; ++k) same = fabsf(kept.a[k] - eye[k]) < 0.05f && fabsf(kept.b[k] - at[k]) < 0.05f;
  same = same && kept.crouched == (Field<uint8_t>(target, ee::kCreatureStealthMode) == 1);
  if (!same) {
    CoverPoint points[rules::kLosBodyPoints];
    kept.cover = CoverFrom(area, id, id, eye, target, points, &kept.by, &kept.by_id);
    kept.at = now;
    kept.crouched = Field<uint8_t>(target, ee::kCreatureStealthMode) == 1;
    for (int k = 0; k < 3; ++k) {
      kept.a[k] = eye[k];
      kept.b[k] = at[k];
    }
  }
  if (by) *by = kept.by;
  if (by_id) *by_id = kept.by_id;
  return kept.cover;
}

std::string CoverWords(rules::Cover cover) {
  switch (cover) {
    case rules::Cover::kHalf: return PlayerText("cover.degree.half");
    case rules::Cover::kThreeQuarters: return PlayerText("cover.degree.three_quarters");
    case rules::Cover::kTotal: return PlayerText("cover.degree.total");
    case rules::Cover::kNone: break;
  }
  return "";
}

void TellAttackCover(void* attacker, void* target, rules::Cover cover, int bonus, uint32_t by_id, bool creature) {
  const uint32_t a = IdOf(attacker), t = IdOf(target);
  if (!TellNow(1, a, t)) return;
  if (LogSlotHere()) {
    Log("cover: 0x%08X has %s (+%d AC) against 0x%08X - %s 0x%08X %s", t, CoverLogWords(cover), bonus, a,
        creature ? "a creature" : by_id == ee::kObjectInvalid ? "the tileset" : "the placeable", by_id,
        creature ? "is in the way" : ObjectName(by_id).c_str());
  }
  const std::string what = creature ? PlayerText("cover.by_creature") : ByWords(t, by_id);
  const std::string degree = CoverWords(cover), plus = std::to_string(bonus);
  TellPlayer(a, PlayerText("cover.target_has_cover", {{"cover", degree}, {"bonus", plus}, {"by", what}}));
  TellPlayer(t, PlayerText("cover.you_have_cover", {{"cover", degree}, {"bonus", plus}, {"by", what}}));
}

bool TotalCoverHoldsAttack(void* creature, void* node) {
  if (!CoverObjectsOn() || !creature || !node) return false;
  void* target = CreatureById(Field<uint32_t>(node, ee::kActionFirstParam));
  if (!target || target == creature) return false;
  std::string by;
  uint32_t by_id = ee::kObjectInvalid;
  const rules::Cover cover = CoverBetween(creature, target, &by, &by_id);
  if (cover != rules::Cover::kTotal) return false;
  const float dx = Field<float>(target, ee::kObjectPositionX) - Field<float>(creature, ee::kObjectPositionX);
  const float dy = Field<float>(target, ee::kObjectPositionY) - Field<float>(creature, ee::kObjectPositionY);
  const float metres = sqrtf(dx * dx + dy * dy);
  const bool ranged = HoldsRangedWeapon(creature);
  if (!rules::CoverRefusesAttack(cover, ranged, metres)) return false;
  const uint32_t a = IdOf(creature), t = IdOf(target);
  if (TellNow(2, a, t)) {
    if (LogSlotHere()) {
      Log("cover: 0x%08X's %s attack on 0x%08X refused - Total Cover (%s), %.1f m", a, ranged ? "ranged" : "melee", t,
          by.c_str(), metres);
    }
    TellPlayer(a, PlayerText("cover.total_no_attack", {{"target", NameOf(a, t)}, {"by", ByWords(a, by_id)}}));
  }
  if (!IsPlayerCharacter(creature)) Reposition(creature, target, ranged);
  return true;
}

namespace {

// #166 decision 2 (owner, 2026-11-23): the spell's area by its shape (rules/area_origin.h) - nwn5e's own by its
// Spell2024 shape, NWN's own by spells.2da TargetShape and, for one that comes down, its Label. `own`: nwn5e's own;
// `feet`: the area's size.
rules::AreaOrigin AreaKindOf(int spell, bool* own, int* feet) {
  *own = false;
  *feet = 0;
  if (const rules::Spell2024* mine = rules::Spell2024Of(spell)) {
    *own = true;
    *feet = mine->feet;
    return rules::OriginOf2024(mine->shape);
  }
  static const std::vector<std::string> shapes = GameTableColumn("spells", "TargetShape");
  static const std::vector<std::string> labels = GameTableColumn("spells", "Label");
  const size_t row = static_cast<size_t>(spell);
  const std::string shape = spell >= 0 && row < shapes.size() ? shapes[row] : "";
  const std::string label = spell >= 0 && row < labels.size() ? labels[row] : "";
  return rules::OriginOfNwn(shape, rules::ComesFromAbove(label));
}

struct Origin {
  float at[3] = {0, 0, 0};
  rules::AreaOrigin kind = rules::AreaOrigin::kCaster;
  bool own = false;
  const char* from = "its source";
  void* caster = nullptr;  // the creature behind it (told), when known
};

thread_local bool t_forced = false;  // ScopedAreaOrigin's
thread_local float t_forced_at[3] = {0, 0, 0};
thread_local rules::AreaOrigin t_forced_kind = rules::AreaOrigin::kCentre;
thread_local int t_forced_feet = 0;

// Where an effect's lines start: `origin_id` the object it comes from (the caster, an area of effect), `spell` its
// spells.2da row; ScopedAreaOrigin's spot when one is set. False when it can't be placed.
bool OriginOf(uint32_t origin_id, int spell, Origin* out) {
  void* object = GameObjectById(origin_id);
  if (!object) return false;
  const uint8_t type = Field<uint8_t>(object, ee::kGameObjectType);
  int feet = 0;
  out->kind = AreaKindOf(spell, &out->own, &feet);
  out->caster = type == ee::kObjectTypeCreature ? object : nullptr;
  float anchor[3] = {Field<float>(object, ee::kObjectPositionX), Field<float>(object, ee::kObjectPositionY),
                     Field<float>(object, ee::kObjectPositionZ)};
  void* eyes_of = out->caster;
  if (t_forced) {
    out->kind = t_forced_kind;
    feet = t_forced_feet;
    for (int k = 0; k < 3; ++k) anchor[k] = t_forced_at[k];
    out->from = "the burst's centre";
  } else if (type == ee::kObjectTypeAreaOfEffect) {
    // A lasting area (Entangle, Web, Grease, Cloudkill ...): by its spell's shape - from its creator for a cone or a
    // line, else from its own place (its centre, or the top over it).
    int area_spell = Field<int32_t>(object, ee::kAoeEffectSpellId);
    if (area_spell < 0) area_spell = spell;
    out->kind = AreaKindOf(area_spell, &out->own, &feet);
    void* creator = CreatureById(Field<uint32_t>(object, ee::kAoeCreator));
    out->caster = creator;
    if (rules::OriginAtCaster(out->kind) && creator && engine::AreaOf(creator) == engine::AreaOf(object)) {
      anchor[0] = Field<float>(creator, ee::kObjectPositionX);
      anchor[1] = Field<float>(creator, ee::kObjectPositionY);
      anchor[2] = Field<float>(creator, ee::kObjectPositionZ);
      eyes_of = creator;
      out->from = "its caster";
    } else {
      eyes_of = nullptr;
      out->from = out->kind == rules::AreaOrigin::kTop ? "the top of the area" : "the area's centre";
    }
  } else if (type == ee::kObjectTypeCreature) {
    out->from = "the caster";
    if (!rules::OriginAtCaster(out->kind)) {
      const float px = Field<float>(object, kSpellTargetX), py = Field<float>(object, kSpellTargetX + 4),
                  pz = Field<float>(object, kSpellTargetX + 8);
      if (!(px == 0.0f && py == 0.0f && pz == 0.0f)) {
        anchor[0] = px;
        anchor[1] = py;
        anchor[2] = pz;
        out->from = out->kind == rules::AreaOrigin::kTop ? "the top of the area" : "the spell's point";
      } else {
        out->kind = rules::AreaOrigin::kCaster;  // no point known: the caster, as before
      }
    }
  } else {
    out->kind = rules::AreaOrigin::kCentre;  // a trap, a door: its own place
  }
  const float eye = rules::EyeHeight(eyes_of ? Field<int32_t>(eyes_of, ee::kCreatureSize) : 3);
  for (int k = 0; k < 3; ++k) out->at[k] = anchor[k];
  out->at[2] += rules::OriginHeight(out->kind, eye, feet);
  return true;
}

const char* OriginWords(rules::AreaOrigin kind) {
  switch (kind) {
    case rules::AreaOrigin::kCaster: return "cone/line from the caster";
    case rules::AreaOrigin::kCasterFace: return "cube from the caster";
    case rules::AreaOrigin::kCentre: return "sphere from its centre";
    case rules::AreaOrigin::kTop: return "from above";
    case rules::AreaOrigin::kCreature: return "emanation";
  }
  return "?";
}

}  // namespace

ScopedAreaOrigin::ScopedAreaOrigin(const float ground[3], rules::AreaOrigin kind, int feet) : outer_(t_forced) {
  for (int k = 0; k < 3; ++k) {
    outer_at_[k] = t_forced_at[k];
    t_forced_at[k] = ground[k];
  }
  outer_kind_ = static_cast<int>(t_forced_kind);
  outer_feet_ = t_forced_feet;
  t_forced = true;
  t_forced_kind = kind;
  t_forced_feet = feet;
}

ScopedAreaOrigin::~ScopedAreaOrigin() {
  t_forced = outer_;
  for (int k = 0; k < 3; ++k) t_forced_at[k] = outer_at_[k];
  t_forced_kind = static_cast<rules::AreaOrigin>(outer_kind_);
  t_forced_feet = outer_feet_;
}

bool SpellOriginOf(uint32_t origin_id, int spell, float at[3], rules::AreaOrigin* kind) {
  Origin origin;
  if (!OriginOf(origin_id, spell, &origin)) return false;
  for (int k = 0; k < 3; ++k) at[k] = origin.at[k];
  if (kind) *kind = origin.kind;
  return true;
}

bool LeftOutOfArea(void* caster, int spell, uint32_t target_id) {
  if (!CoverObjectsOn() || !caster) return false;
  void* target = CreatureById(target_id);
  void* area = engine::AreaOf(caster);
  if (!target || target == caster || !area || engine::AreaOf(target) != area) return false;
  Origin origin;
  if (!OriginOf(IdOf(caster), spell, &origin)) return false;
  CoverPoint points[rules::kLosBodyPoints];
  std::string by;
  uint32_t by_id = ee::kObjectInvalid;
  const rules::Cover cover = CoverFrom(area, IdOf(caster), target_id, origin.at, target, points, &by, &by_id);
  if (rules::AreaCoverEffectFor(cover, true, false) != rules::AreaCoverEffect::kLeftOut) return false;
  const uint32_t c = IdOf(caster);
  if (TellNow(4, c, target_id)) {
    if (LogSlotHere()) {
      Log("cover: 0x%08X behind Total Cover from spell %d's point of origin (%s, %s, at %.1f, %.1f, %.1f) - left out "
          "of the area: no save, no effect - %s", target_id, spell, OriginWords(origin.kind), origin.from,
          origin.at[0], origin.at[1], origin.at[2], by.c_str());
    }
    const std::string what = ByWords(target_id, by_id);
    TellPlayer(target_id, PlayerText("cover.area_you", {{"by", what}}));
    TellPlayer(c, PlayerText("cover.area_target", {{"target", NameOf(c, target_id)}, {"by", what}}));
  }
  return true;
}

int SaveCoverBonus(void* creature, uint32_t versus, uint32_t script_self, int spell) {
  if (!CoverObjectsOn() || !creature || rules::SpellIgnoresCover(spell)) return 0;  // Sacred Flame
  void* area = engine::AreaOf(creature);
  const uint32_t origin_id = script_self != ee::kObjectInvalid ? script_self : versus;
  void* origin_object = GameObjectById(origin_id);
  if (!area || !origin_object || origin_object == creature || engine::AreaOf(origin_object) != area) return 0;
  Origin origin;
  if (!OriginOf(origin_id, spell, &origin)) return 0;
  CoverPoint points[rules::kLosBodyPoints];
  std::string by;
  uint32_t by_id = ee::kObjectInvalid;
  const rules::Cover cover = CoverFrom(area, origin_id, IdOf(creature), origin.at, creature, points, &by, &by_id);
  if (rules::AreaCoverEffectFor(cover, origin.own, true) == rules::AreaCoverEffect::kNone) return 0;
  const int bonus = rules::CoverSaveBonus(cover);
  if (!bonus) return 0;
  const uint32_t t = IdOf(creature);
  if (TellNow(3, origin_id, t)) {
    if (LogSlotHere()) {
      Log("cover: 0x%08X has %s from %s (0x%08X, spell %d, %s, at %.1f, %.1f, %.1f): +%d to its Dexterity save - %s",
          t, CoverLogWords(cover), origin.from, origin_id, spell, OriginWords(origin.kind), origin.at[0],
          origin.at[1], origin.at[2], bonus, by.c_str());
    }
    const std::string degree = CoverWords(cover), plus = std::to_string(bonus), what = ByWords(t, by_id);
    TellPlayer(t, PlayerText("cover.save_you", {{"cover", degree}, {"bonus", plus}, {"by", what}}));
    void* caster = origin.caster ? origin.caster : CreatureById(versus);
    if (caster && caster != creature) {
      TellPlayer(IdOf(caster), PlayerText("cover.save_target", {{"target", NameOf(t, t)}, {"cover", degree},
                                                                {"bonus", plus}, {"by", what}}));
    }
  }
  return bonus;
}

void InstallCoverObjects() {
  g_clear_line = reinterpret_cast<ClearLineOfSightFn>(EngineExport(ee::kClearLineOfSight));
  g_placeable_by_id = reinterpret_cast<PlaceableByIdFn>(EngineExport(kGetPlaceableById));
  g_compute_height = reinterpret_cast<ComputeHeightFn>(EngineExport(ee::kComputeHeight));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!g_config.cover_objects) {
    Log("cover objects: off (cover_objects=0) - sight and cover are the engine's: walls and shut doors only");
    return;
  }
  std::ifstream file(DllDirectory() + L"\\data\\placeable_bounds.txt", std::ios::binary);
  int misses = 0;
  std::string line;
  while (file && std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    int row = -1;
    rules::ModelBounds bounds;
    if (rules::ParseBoundsLine(line, &row, &bounds)) {
      g_bounds[row] = bounds;
    } else if (line.rfind("# miss ", 0) == 0) {
      ++misses;
    }
  }
  g_table = !g_bounds.empty();
  if (!g_table) {
    Log("cover objects: cover_objects=1 but no data\\placeable_bounds.txt (build.bat makes it from your game with "
        "tools\\data\\build_derived.js) - no cover from objects");
    return;
  }
  Log("cover objects: cover_objects=1 (%s) - %zu placeables.2da rows' bounds from data\\placeable_bounds.txt, %d rows "
      "with a model unread or nothing drawn (no cover from them); a line is blocked by the engine's sight (walls, shut doors) or a placeable's box %.1f m "
      "tall or more; cover from six body points (3-4 Half, 5 Three-Quarters, 6 Total)%s",
      g_config.mode == rules::RuleVariant::kFaithful ? "faithful" : "off: not the faithful rules", g_bounds.size(), misses,
      rules::kCoverMinHeight, g_placeable_by_id ? "" : " - NO GetPlaceableByGameObjectID: no boxes");
}
