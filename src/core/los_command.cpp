// "!los [tag]" (#166 part 2's research step; owner, 2026-11-23: "How do vision cones interact with objects? Placeables,
// meshes that are part of the tileset, half cover..." - "Start the research and fix the hall map to have more obstacles
// that will allow the testing of the cones with different covers"), a test command ([debug] test_commands): from the
// nearest hostile - or the creature tagged <tag> - to the speaker, the engine's own sight line, as perception draws it
// (CNWSArea::ClearLineOfSight from the observer's position + 1 m, the two creatures' ids, last 1), to six points of the
// speaker's body (rules::kLosBody; #166, 2026-11-23: seven, by its size, crouched in stealth mode).
// Each: clear or blocked, its blocker (object type, resref, tag - or the tileset when there's none) and the hit point.
// And for the same line the area's walk-blocking boxes (CNWArea::NoNonWalkPolysInDoors, as ClearLine in
// rules_perception.cpp asks them: the line's own heights, radius 0.05 m), so a placeable the sight line ignores shows.
// One chat line (the catalogue's test_commands.los_*), the detail in nwn5e.log.
// #166 part 2 (owner, 2026-11-23: "A, and apply cover to all three"): with cover_objects, a line the engine leaves
// clear is blocked by a placeable's box in the way (object_cover.cpp), and the line ends with the cover degree the
// six points give (3-4 blocked Half, 5 Three-Quarters, 6 Total) and what gives it most.
//
// What the sight line weighs (read from nwmain 89.8193.37, CNWSArea::ClearLineOfSight rva 0x35c590-0x35cd0f): the
// tiles the line crosses (CNWSTile::ComputeClippedLineSegment, then CNWSTile::ClearLineOfSight for each - the tile's
// surface mesh), then the area's static objects (CNWArea::NoNonWalkPolysDetailed over the line, its height range the
// lower to the higher end, radius 0, excluding the two creatures): any walkmesh box in the way blocks - a door among
// them is intersected with its own line (CNWArea::IntersectLineSegments) for the hit point, at the door's height + 1 m.
// The hit point starts zeroed and is written only for the tile and door cases: a placeable's block leaves it (0, 0, 0),
// its object id given. What only a session can settle: how a placeable's box height is weighed against the line's
// (a waist-high table, a knee-high bench), whether an open door's walkmesh leaves the list, whether a portcullis
// (genericdoors.2da BlockSight 0) still blocks, which tile meshes (furniture, low walls) block.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <string>
#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "object_cover.h"
#include "player_text.h"
#include "rules/cover_objects.h"
#include "rules/los_probe.h"

namespace {

struct Vec3 {
  float x, y, z;
};
using ClearLineOfSightFn = int (*)(void* area, Vec3 from, Vec3 to, Vec3* hit, uint32_t* hit_object, uint32_t from_id,
                                   uint32_t to_id, int flag);
using StaticLineFn = int (*)(void* area, float x1, float y1, float x2, float y2, float z1, float z2, float radius,
                             uint32_t exclude, uint32_t* blocker, int flag, void* exclude_list, int doors);

uint32_t IdOf(void* creature) { return creature ? Field<uint32_t>(creature, ee::kObjectId) : ee::kObjectInvalid; }

// Each object's type, resref and tag ("placeable plc_bookshelf 'Bookshelf'"), the tileset for none.
std::vector<std::string> Describe(uint32_t self, const std::vector<uint32_t>& ids) {
  std::string code = "object me = OBJECT_SELF; object b; string d;";
  for (size_t i = 0; i < ids.size(); ++i) {
    char part[400];
    sprintf_s(part,
              "b = StringToObject(\"%x\"); d = \"\";"
              "if (GetIsObjectValid(b)) {"
              " int t = GetObjectType(b);"
              " d = (t == OBJECT_TYPE_PLACEABLE ? \"placeable\" : (t == OBJECT_TYPE_DOOR ? \"door\" :"
              "  (t == OBJECT_TYPE_CREATURE ? \"creature\" : \"object \" + IntToString(t)))) + \" \" + GetResRef(b) +"
              "  \" '\" + GetTag(b) + \"'\";"
              "}"
              "SetLocalString(me, \"nwn5e_los_d%d\", d);",
              ids[i], static_cast<int>(i));
    code += part;
  }
  RunScriptChunk(code.c_str(), self);
  void* me = CreatureById(self);
  std::vector<std::string> out;
  for (size_t i = 0; i < ids.size(); ++i) {
    std::string d = me ? engine::GetLocalString(me, ("nwn5e_los_d" + std::to_string(i)).c_str()) : "";
    out.push_back(ids[i] == ee::kObjectInvalid || d.empty() ? "" : d);
  }
  return out;
}

// The resref alone, for the chat line ("placeable plc_bookshelf 'x'" -> "plc_bookshelf").
std::string ShortName(const std::string& described) {
  const size_t a = described.find(' ');
  if (a == std::string::npos) return described;
  const size_t b = described.find(' ', a + 1);
  return described.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
}

}  // namespace

std::vector<std::string> LosCommand(void* creature, const std::string& args) {
  static const auto clear_line = reinterpret_cast<ClearLineOfSightFn>(EngineExport(ee::kClearLineOfSight));
  static const auto static_line = reinterpret_cast<StaticLineFn>(EngineExport(ee::kStaticObjectsOnLine));
  std::string tag = args;
  while (!tag.empty() && tag.front() == ' ') tag.erase(tag.begin());
  while (!tag.empty() && tag.back() == ' ') tag.pop_back();
  if (tag.find(' ') != std::string::npos || tag.find('"') != std::string::npos) {
    return {PlayerText("test_commands.los_usage")};
  }
  const uint32_t self = IdOf(creature);
  // The observer: the creature tagged so, or the nearest enemy - else the nearest creature (the owner, 2026-11-23, SM-5:
  // "!los doesn't work here because the orc is neutral").
  const std::string find =
      tag.empty() ? "object o = GetNearestCreature(CREATURE_TYPE_REPUTATION, REPUTATION_TYPE_ENEMY);"
                    "if (!GetIsObjectValid(o)) o = GetNearestObject(OBJECT_TYPE_CREATURE);"
                  : "object o = GetNearestObjectByTag(\"" + tag + "\"); if (GetObjectType(o) != OBJECT_TYPE_CREATURE) "
                                                                  "o = OBJECT_INVALID;";
  RunScriptChunk((find + "SetLocalString(OBJECT_SELF, \"nwn5e_los_o\", GetIsObjectValid(o) ? ObjectToString(o) : \"\");"
                         "SetLocalString(OBJECT_SELF, \"nwn5e_los_n\", GetIsObjectValid(o) ? GetName(o) : \"\");")
                     .c_str(),
                 self);
  const std::string observer_hex = engine::GetLocalString(creature, "nwn5e_los_o");
  const std::string observer_name = engine::GetLocalString(creature, "nwn5e_los_n");
  void* observer = observer_hex.empty() ? nullptr : CreatureById(static_cast<uint32_t>(strtoul(observer_hex.c_str(),
                                                                                                 nullptr, 16)));
  void* area = engine::AreaOf(creature);
  if (!observer || !area || engine::AreaOf(observer) != area) {
    return {PlayerText("test_commands.los_none", {{"tag", tag.empty() ? PlayerText("test_commands.los_an_enemy") : tag}})};
  }
  if (!clear_line) return {PlayerText("test_commands.los_no_engine")};
  const uint32_t from_id = IdOf(observer);
  // #166 (2026-11-23): the observer's eyes by its size, the speaker's body by its size, crouched in stealth mode - as
  // the cover rule (object_cover.cpp) measures them.
  const Vec3 eye{Field<float>(observer, ee::kObjectPositionX), Field<float>(observer, ee::kObjectPositionY),
                 Field<float>(observer, ee::kObjectPositionZ) +
                     rules::EyeHeight(Field<int32_t>(observer, ee::kCreatureSize))};
  const int size = Field<int32_t>(creature, ee::kCreatureSize);
  const bool crouched = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  const float tx = Field<float>(creature, ee::kObjectPositionX), ty = Field<float>(creature, ee::kObjectPositionY),
              tz = Field<float>(creature, ee::kObjectPositionZ);
  struct Line {
    Vec3 to;
    bool clear;
    uint32_t blocker;
    Vec3 hit;
    int walk;  // the walk-blocking boxes: 1 clear, 0 blocked, -1 not asked
    uint32_t walk_blocker;
    bool box;  // blocked by a placeable's box (cover_objects), not the engine's line
  };
  std::vector<Line> lines;
  std::vector<uint32_t> ids;
  for (int p = 0; p < rules::kLosBodyPoints; ++p) {
    const rules::LosBodyPoint point = rules::ScaledBodyPoint(p, size, crouched);
    Line line{};
    rules::LosPointAt(point, eye.x, eye.y, tx, ty, tz, &line.to.x, &line.to.y, &line.to.z);
    line.hit = line.to;
    line.blocker = ee::kObjectInvalid;
    line.clear = clear_line(area, eye, line.to, &line.hit, &line.blocker, from_id, self, 1) == 1;
    line.box = false;
    if (line.clear) {
      const float a3[3] = {eye.x, eye.y, eye.z}, b3[3] = {line.to.x, line.to.y, line.to.z};
      float t = 0.0f;
      if (ObjectsBlockLine(area, self, a3, b3, &line.blocker, &t)) {
        line.clear = false;
        line.box = true;
        line.hit = Vec3{eye.x + (line.to.x - eye.x) * t, eye.y + (line.to.y - eye.y) * t,
                        eye.z + (line.to.z - eye.z) * t};
      }
    }
    line.walk = -1;
    line.walk_blocker = ee::kObjectInvalid;
    if (static_line) {
      line.walk = static_line(area, eye.x, eye.y, line.to.x, line.to.y, eye.z < line.to.z ? eye.z : line.to.z,
                              eye.z < line.to.z ? line.to.z : eye.z, 0.05f, ee::kObjectInvalid, &line.walk_blocker, 1,
                              nullptr, 0) == 1;
    }
    ids.push_back(line.clear ? ee::kObjectInvalid : line.blocker);
    ids.push_back(line.walk == 0 ? line.walk_blocker : ee::kObjectInvalid);
    lines.push_back(line);
  }
  const std::vector<std::string> names = Describe(self, ids);
  const float dx = tx - eye.x, dy = ty - eye.y;
  Log("los: from 0x%08X %s (eyes %.2f, %.2f, %.2f) to 0x%08X (feet %.2f, %.2f, %.2f; size %d, %s), %.1f m:", from_id,
      observer_name.c_str(), eye.x, eye.y, eye.z, self, tx, ty, tz, size, crouched ? "crouched" : "standing",
      sqrtf(dx * dx + dy * dy));
  std::vector<rules::LosPointResult> results;
  for (size_t i = 0; i < lines.size(); ++i) {
    const Line& line = lines[i];
    const std::string blocker = line.clear ? "" : (names[2 * i].empty() ? PlayerText("test_commands.los_tileset")
                                                                         : names[2 * i]);
    const bool hit_written = !(line.hit.x == 0.0f && line.hit.y == 0.0f && line.hit.z == 0.0f);
    char hit[96] = "";
    if (!line.clear) {
      if (hit_written) {
        const float hx = line.hit.x - eye.x, hy = line.hit.y - eye.y;
        sprintf_s(hit, ", hit at (%.2f, %.2f, %.2f) %.1f m out, %.2f m over the observer's feet", line.hit.x,
                  line.hit.y, line.hit.z, sqrtf(hx * hx + hy * hy), line.hit.z - (eye.z - 1.0f));
      } else {
        sprintf_s(hit, ", no hit point (a placeable's block)");
      }
    }
    const std::string walk = line.walk < 0 ? "not asked"
                             : line.walk ? "clear"
                                         : "blocked by " + (names[2 * i + 1].empty() ? std::string("an object")
                                                                                      : names[2 * i + 1]);
    Log("los:   %-14s (%.2f, %.2f, %.2f): sight %s%s%s%s; walk-blocking boxes %s", rules::kLosBody[i].key, line.to.x,
        line.to.y, line.to.z, line.clear ? "clear" : "blocked by ", blocker.c_str(),
        line.box ? " (its box: cover_objects)" : "", hit, walk.c_str());
    rules::LosPointResult result;
    result.name = PlayerText((std::string("test_commands.") + "los_" + rules::kLosBody[i].key).c_str());
    result.clear = line.clear;
    result.blocker = line.clear ? "" : (names[2 * i].empty() ? blocker : ShortName(names[2 * i]));
    results.push_back(result);
  }
  const std::string summary = rules::LosSummary(results, PlayerText("test_commands.los_clear"),
                                                PlayerText("test_commands.los_blocked"));
  // #166: the cover degree those points give, and what blocks the most of them.
  int blocked = 0;
  std::map<std::string, int> by;
  for (const rules::LosPointResult& result : results) {
    if (result.clear) continue;
    ++blocked;
    ++by[result.blocker];
  }
  std::string most;
  int most_count = 0;
  for (const auto& b : by) {
    if (b.second > most_count) {
      most_count = b.second;
      most = b.first;
    }
  }
  const rules::Cover cover = rules::CoverFromBlocked(blocked);
  Log("los: %s - %d of 6 blocked: %s%s%s%s", summary.c_str(), blocked,
      cover == rules::Cover::kNone ? "no cover" : CoverWords(cover).c_str(), most.empty() ? "" : " (", most.c_str(),
      most.empty() ? "" : ")");
  if (cover == rules::Cover::kNone) {
    return {PlayerText("test_commands.los_summary_no_cover", {{"observer", observer_name}, {"points", summary}})};
  }
  return {PlayerText("test_commands.los_summary_cover", {{"observer", observer_name}, {"points", summary},
                                                         {"cover", CoverWords(cover)}, {"by", most}})};
}
