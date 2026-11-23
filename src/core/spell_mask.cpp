// The spells' particle masks (#184): spell_mask.h. The mask's shape and its uniforms' layout are rules/particle_mask.h's,
// which spells and which mask shows rules/spell_masks.h's; this file measures, keeps and sends them.

#include "spell_mask.h"

#include <windows.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "light_sources.h"
#include "log.h"
#include "object_cover.h"
#include "rules/area_origin.h"
#include "rules/spell_masks.h"
#include "rules/spells2024.h"
#include "script_hooks.h"
#include "server_tick.h"
#include "spell_cast_listeners.h"

namespace {

struct Vec3 {
  float x, y, z;
};
using ClearLineOfSightFn = int (*)(void* area, Vec3 from, Vec3 to, Vec3* hit, uint32_t* hit_object, uint32_t from_id,
                                   uint32_t to_id, int flag);
using ComputeHeightFn = float (*)(void* area, Vec3 at);

constexpr float kRingLowest = 0.5f;    // the ring's height over the ground under the origin, held between these
constexpr float kRingHighest = 2.0f;
constexpr float kWallPrecision = 0.1f;  // metres: a wall's distance found to this
constexpr float kWallNearest = 0.05f;   // a wall right at the origin (0 would read as none)

ClearLineOfSightFn ClearLine() {
  static const auto fn = reinterpret_cast<ClearLineOfSightFn>(EngineExport(ee::kClearLineOfSight));
  return fn;
}

ComputeHeightFn GroundHeight() {
  static const auto fn = reinterpret_cast<ComputeHeightFn>(EngineExport(ee::kComputeHeight));
  return fn;
}

}  // namespace

bool MeasureMaskWalls(void* area, uint32_t self, const float origin[3], float reach,
                      float walls[rules::kMaskRingDirections], int* lines) {
  for (int k = 0; k < rules::kMaskRingDirections; ++k) walls[k] = 0.0f;
  int counted = 0;
  if (!lines) lines = &counted;
  const ClearLineOfSightFn clear_line = ClearLine();
  const ComputeHeightFn ground_at = GroundHeight();
  if (!area || !clear_line || !ground_at || !(reach > 0.0f)) return false;
  float ground = ground_at(area, Vec3{origin[0], origin[1], origin[2]});
  if (ground <= ee::kAreaOffGridHeight + 0.01f) ground = origin[2] - 1.0f;
  float h = origin[2] - ground;
  h = h < kRingLowest ? kRingLowest : h > kRingHighest ? kRingHighest : h;
  const Vec3 from{origin[0], origin[1], ground + h};
  for (int k = 0; k < rules::kMaskRingDirections; ++k) {
    const float a = rules::MaskRingAngle(k);
    const float cx = cosf(a), cy = sinf(a);
    // Clear to `d` metres out (its end over the ground there, as high as the ring; off the area's tiles: blocked).
    const auto clear = [&](float d, Vec3* hit) {
      ++*lines;
      Vec3 to{origin[0] + cx * d, origin[1] + cy * d, from.z};
      const float g = ground_at(area, to);
      if (g <= ee::kAreaOffGridHeight + 0.01f) {
        if (hit) *hit = to;
        return false;
      }
      to.z = g + h;
      Vec3 at = to;
      uint32_t object = ee::kObjectInvalid;
      const bool open = clear_line(area, from, to, &at, &object, self, ee::kObjectInvalid, 1) == 1;
      if (hit) *hit = at;
      return open;
    };
    Vec3 hit{0.0f, 0.0f, 0.0f};
    if (clear(reach, &hit)) continue;
    float lo = 0.0f, hi = reach;
    // Where the engine found it blocked, when that is on the line short of its end: tried either side first.
    const float along = (hit.x - origin[0]) * cx + (hit.y - origin[1]) * cy;
    const float across = fabsf((hit.y - origin[1]) * cx - (hit.x - origin[0]) * cy);
    if (along > kWallPrecision && along < reach - kWallPrecision && across < 1.0f) {
      if (clear(along - kWallPrecision, nullptr)) {
        lo = along - kWallPrecision;
        if (!clear(along + kWallPrecision, nullptr)) {
          hi = along + kWallPrecision;
        } else {
          lo = along + kWallPrecision;
        }
      } else {
        hi = along - kWallPrecision;
      }
    }
    while (hi - lo > kWallPrecision) {
      const float mid = (lo + hi) / 2.0f;
      if (clear(mid, nullptr)) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    walls[k] = lo > kWallNearest ? lo : kWallNearest;
  }
  return true;
}

std::string MaskUniformScript(const std::string& player, const std::vector<rules::MaskVec>& uniforms) {
  std::string code;
  for (size_t i = 0; i < uniforms.size(); ++i) {
    char line[240];
    sprintf_s(line, "SetShaderUniformVec(%s, SHADER_UNIFORM_%d, %.4f, %.4f, %.4f, %.4f);", player.c_str(),
              static_cast<int>(i) + 1, uniforms[i].x, uniforms[i].y, uniforms[i].z, uniforms[i].w);
    code += line;
  }
  return code;
}

std::string MaskWords(const rules::ParticleMask& mask) {
  int walled = 0;
  float nearest = 0.0f;
  for (int k = 0; k < rules::kMaskRingDirections; ++k) {
    const float w = mask.walls[k];
    if (!(w > 0.0f) || w >= mask.reach) continue;
    ++walled;
    if (nearest == 0.0f || w < nearest) nearest = w;
  }
  char nearest_text[48] = "";
  if (walled) sprintf_s(nearest_text, " (the nearest %.1f m)", nearest);
  const size_t boxes = mask.boxes.size() > static_cast<size_t>(rules::kParticleMaskMaxBoxes)
                           ? static_cast<size_t>(rules::kParticleMaskMaxBoxes)
                           : mask.boxes.size();
  char text[320];
  sprintf_s(text, "origin (%.1f, %.1f, %.1f), reach %.1f m; walls in %d of %d directions%s; %zu box(es)", mask.origin[0],
            mask.origin[1], mask.origin[2], mask.reach, walled, rules::kMaskRingDirections, nearest_text, boxes);
  std::string out = text;
  for (size_t i = 0; i < mask.boxes.size() && i < static_cast<size_t>(rules::kParticleMaskMaxBoxes); ++i) {
    const rules::ObjectBox& b = mask.boxes[i];
    char box[120];
    sprintf_s(box, "%s(%.1f, %.1f) %.1f x %.1f x %.1f m", i ? ", " : " - ", b.centre[0], b.centre[1], 2 * b.half[0],
              2 * b.half[1], 2 * b.half[2]);
    out += box;
  }
  return out;
}

// --- The masks up, and which each client shows --------------------------------------------------------------------
namespace {

using VmCommandFn = int (*)(void* commands, int command, int parameters);
using PopIntFn = int (*)(void* vm, int* value);
using PushIntFn = int (*)(void* vm, int value);

constexpr const char* kEffectVisualCommand = "?ExecuteCommandEffectVisualEffect@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr const char* kEffectAreaCommand = "?ExecuteCommandEffectAreaEffect@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr uint64_t kReconcileMs = 250;    // the clients' masks checked four times a second while any is up
constexpr uint64_t kSwapWindowMs = 3000;  // a caster's effects swapped this long after its masked spell's script began
constexpr uint64_t kCastKeepMs = 8000;    // a cone's mask set at its cast stands for its impact script
constexpr uint64_t kAreaLookMs = 3000;    // a lasting area's object looked for this long after its script began
constexpr uint64_t kBoxCheckMs = 1000;    // a lasting mask's boxes looked at again (one destroyed outright)
constexpr float kListPast = 6.0f;         // boxes listed this far past the reach (their centres; a near side counts)
constexpr int kLogMost = 400;

struct Active {
  uint64_t id = 0;
  void* area = nullptr;
  uint32_t caster = 0;
  int spell = -1;  // -1: !maskproof's
  uint32_t aoe = 0;  // a lasting one's area of effect
  bool lasting = false;
  uint64_t set_ms = 0, end_ms = 0, checked_ms = 0;
  rules::ParticleMask shape;
  std::vector<uint32_t> box_ids;  // its boxes' placeables, sorted
  std::vector<rules::MaskVec> uniforms;
  std::string what;  // for the log: "Fireball (58)"
};
struct Pending {  // a lasting area's script began: its area of effect looked for
  uint32_t caster = 0;
  int spell = -1;
  uint64_t since = 0;
};
struct Recent {  // a caster's masked spell's impact script began: its effects swapped for their copies
  int spell = -1;
  uint64_t at = 0;
};

bool g_on = false;        // the spells' masks wired
bool g_registry = false;  // the tick in (the spells' or !maskproof's)
std::vector<Active> g_active;
std::vector<Pending> g_pending;
std::unordered_map<uint32_t, uint64_t> g_shown;     // player -> the mask its client shows (0: off)
std::unordered_map<uint32_t, Recent> g_recent;      // caster -> its masked spell's impact script
std::unordered_map<uint64_t, uint64_t> g_cast_set;  // (caster, spell) -> its cone's mask set as the cast began
std::unordered_map<std::string, std::vector<int>> g_scripts;  // an impact script (lower case) -> its masked spells
bool g_scripts_built = false;
uint64_t g_next_id = 1;
uint64_t g_next_tick = 0;
int g_logged = 0;
VmCommandFn g_effect_visual = nullptr;
VmCommandFn g_effect_area = nullptr;
PopIntFn g_pop_int = nullptr;
PushIntFn g_push_int = nullptr;
void** g_vm = nullptr;

bool LogSlot() { return g_logged++ < kLogMost; }
uint64_t Key(uint32_t caster, int spell) { return (static_cast<uint64_t>(caster) << 32) | static_cast<uint32_t>(spell); }
uint64_t AreaKey(void* area) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(area)); }

std::string Hex(uint32_t id) {
  char text[16];
  sprintf_s(text, "%x", id);
  return text;
}

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

bool FileThere(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::string SpellWords(int spell) {
  if (spell < 0) return "the proof";
  if (const rules::Spell2024* s = rules::Spell2024Of(spell)) return std::string(s->name) + " (" + std::to_string(spell) + ")";
  static const std::vector<std::string> labels = GameTableColumn("spells", "Label");
  const size_t row = static_cast<size_t>(spell);
  const std::string label = row < labels.size() ? labels[row] : "";
  return (label.empty() ? std::string("spell") : label) + " (" + std::to_string(spell) + ")";
}

// The area's size across the ground (metres): nwn5e's own spell's shape, else spells.2da's TargetShape and sizes.
float AreaMetres(int spell) {
  if (spell >= rules::kDragonsBreathExhaleFirst && spell <= rules::kDragonsBreathExhaleLast) {
    return rules::FeetToMetres(rules::kDragonsBreathConeFeet);
  }
  if (const rules::Spell2024* s = rules::Spell2024Of(spell)) {
    const float m = rules::FeetToMetres(s->feet);
    switch (s->shape) {
      case rules::SpellShape2024::kCone:
      case rules::SpellShape2024::kSphere: return m;
      case rules::SpellShape2024::kCubeFromCaster: return m * 1.118f;  // its far corners from the caster's face
      case rules::SpellShape2024::kSquare: return m * 0.7072f;         // half its diagonal
      default: break;
    }
  }
  static const std::vector<std::string> shapes = GameTableColumn("spells", "TargetShape");
  static const std::vector<std::string> xs = GameTableColumn("spells", "TargetSizeX");
  static const std::vector<std::string> ys = GameTableColumn("spells", "TargetSizeY");
  const size_t row = static_cast<size_t>(spell);
  const float x = row < xs.size() ? static_cast<float>(atof(xs[row].c_str())) : 0.0f;
  const float y = row < ys.size() ? static_cast<float>(atof(ys[row].c_str())) : 0.0f;
  const std::string shape = row < shapes.size() ? shapes[row] : "";
  if (_stricmp(shape.c_str(), "rectangle") == 0 && x > 0.0f) return 0.5f * hypotf(x, y > 0.0f ? y : x);
  return x > 0.0f ? x : 6.0f;
}

void Reconcile(uint64_t now, uint32_t hint);

// Its boxes picked again from the cover's list (its uniforms packed again): the four that hide the most from its origin.
void PickBoxes(Active* a, uint32_t lister) {
  const std::vector<NearObjectBox> listed =
      ObjectBoxesAround(a->area, lister, a->shape.origin, a->shape.reach + kListPast);
  std::vector<rules::ObjectBox> boxes;
  for (const NearObjectBox& one : listed) boxes.push_back(one.box);
  a->shape.boxes = rules::PickMaskBoxes(a->shape.origin, a->shape.reach, boxes);
  a->box_ids.clear();
  for (const rules::ObjectBox& b : a->shape.boxes) {
    for (const NearObjectBox& one : listed) {
      if (one.box.centre[0] == b.centre[0] && one.box.centre[1] == b.centre[1] && one.box.centre[2] == b.centre[2]) {
        a->box_ids.push_back(one.id);
        break;
      }
    }
  }
  std::sort(a->box_ids.begin(), a->box_ids.end());
  a->uniforms = rules::PackParticleMask(a->shape);
}

// #184's house rule areas_fill_around: a lasting area's mask carries walls only (rules::AreaCountsPlaceables, as its
// rule's lines); a cone's, a line's or a burst's its boxes too.
bool BoxesCount(const Active& a) { return rules::AreaCountsPlaceables(a.lasting, g_config.areas_fill_around); }

void Add(Active a, const char* when, int lines) {
  a.id = g_next_id++;
  if (LogSlot()) {
    char held[64];
    if (a.lasting) {
      sprintf_s(held, "while its area 0x%08X lasts", a.aoe);
    } else {
      sprintf_s(held, "%.1f s", static_cast<double>(a.end_ms - a.set_ms) / 1000.0);
    }
    Log("spell mask: set for %s from 0x%08X, %s - %s; %d sight lines; held %s", a.what.c_str(), a.caster, when,
        MaskWords(a.shape).c_str(), lines, held);
  }
  const uint32_t hint = a.caster;
  g_active.push_back(std::move(a));
  Reconcile(GameMs(), hint);
}

// A spell's mask: its origin the area code's (from `origin_id`, the caster or its area of effect), its reach the
// area's size and kMaskReachPast, its walls and boxes measured now.
void SetMask(uint32_t origin_id, uint32_t caster_id, int spell, const char* when, uint32_t aoe, const std::string& tag) {
  const rules::SpellMaskEntry* entry = rules::SpellMaskOf(spell);
  void* object = GameObjectById(origin_id);
  void* area = object ? engine::AreaOf(object) : nullptr;
  if (!entry || !area) return;
  Active a;
  rules::AreaOrigin kind = rules::AreaOrigin::kCentre;
  if (!SpellOriginOf(origin_id, spell, a.shape.origin, &kind)) return;
  float size = rules::FogCloudRadius(tag.c_str());
  if (!(size > 0.0f)) size = AreaMetres(spell);
  a.shape.reach = rules::MaskReach(size);
  a.area = area;
  a.caster = caster_id;
  a.spell = spell;
  a.aoe = aoe;
  a.lasting = aoe != 0;
  a.set_ms = GameMs();
  a.end_ms = a.lasting ? 0 : a.set_ms + static_cast<uint64_t>(entry->hold_ms);
  a.checked_ms = a.set_ms;
  a.what = SpellWords(spell);
  const uint32_t lister = CreatureById(caster_id) ? caster_id : origin_id;
  int lines = 0;
  MeasureMaskWalls(area, lister, a.shape.origin, a.shape.reach, a.shape.walls, &lines);
  if (BoxesCount(a)) {
    PickBoxes(&a, lister);
  } else {
    a.uniforms = rules::PackParticleMask(a.shape);  // walls only: a lasting area fills in round the placeables
  }
  Add(std::move(a), when, lines);
}

// Each player character's client shown the newest mask up in its area (rules::MaskToShow), or the mask off; only the
// ones that change are sent. `hint`: a creature to run the scripts as (the caster just heard).
void Reconcile(uint64_t now, uint32_t hint) {
  uint32_t runner = hint && CreatureById(hint) ? hint : 0;
  for (const Active& a : g_active) {
    if (!runner && CreatureById(a.caster)) runner = a.caster;
  }
  for (const auto& s : g_shown) {
    if (!runner && CreatureById(s.first)) runner = s.first;
  }
  void* run_as = runner ? CreatureById(runner) : nullptr;
  if (!run_as) return;
  RunScriptChunk("string s = \"\"; object p = GetFirstPC(); while (GetIsObjectValid(p)) { s += ObjectToString(p) + "
                 "\";\"; p = GetNextPC(); } SetLocalString(OBJECT_SELF, \"n5_mask_pcs\", s);",
                 runner);
  const std::string list = engine::GetLocalString(run_as, "n5_mask_pcs");
  std::vector<rules::HeldMask> held;
  for (const Active& a : g_active) held.push_back({a.id, AreaKey(a.area), a.set_ms, a.end_ms});
  std::unordered_map<uint32_t, uint64_t> next;
  std::string code;
  std::istringstream entries(list);
  std::string entry;
  while (std::getline(entries, entry, ';')) {
    if (entry.empty()) continue;
    const uint32_t id = static_cast<uint32_t>(strtoul(entry.c_str(), nullptr, 16));
    void* pc = CreatureById(id);
    void* area = pc ? engine::AreaOf(pc) : nullptr;
    const uint64_t want = rules::MaskToShow(held, AreaKey(area), now);
    const auto had = g_shown.find(id);
    const uint64_t was = had == g_shown.end() ? 0 : had->second;
    next[id] = want;
    if (want == was) continue;
    const std::string who = "StringToObject(\"" + Hex(id) + "\")";
    const Active* shown = nullptr;
    for (const Active& a : g_active) {
      if (a.id == want) shown = &a;
    }
    code += shown ? MaskUniformScript(who, shown->uniforms)
                  : "SetShaderUniformVec(" + who + ", SHADER_UNIFORM_1, 0.0, 0.0, 0.0, 0.0);";
  }
  g_shown.swap(next);  // a player gone from the list is forgotten: its client went with it
  if (!code.empty()) RunScriptChunk(code.c_str(), runner);
}

// A lasting area's mask once its area of effect is there: the nearest to the spell's point that its caster made, of
// that spell, not yet held by a mask.
constexpr const char* kFindArea =
    "object c = GetSpellTargetObject(); location l = GetSpellTargetLocation(); if (GetIsObjectValid(c)) l = "
    "GetLocation(c); string s = \"\"; int i = 1; object a = GetNearestObjectToLocation(OBJECT_TYPE_AREA_OF_EFFECT, l, "
    "i); while (GetIsObjectValid(a) && i <= 10) { if (GetAreaOfEffectCreator(a) == OBJECT_SELF) s += ObjectToString(a) "
    "+ \" \" + GetTag(a) + \";\"; i++; a = GetNearestObjectToLocation(OBJECT_TYPE_AREA_OF_EFFECT, l, i); } "
    "SetLocalString(OBJECT_SELF, \"n5_mask_aoe\", s);";

void LookForAreas(uint64_t now) {
  for (size_t i = 0; i < g_pending.size();) {
    const Pending p = g_pending[i];
    void* caster = CreatureById(p.caster);
    bool done = !caster;
    if (caster && now <= p.since + kAreaLookMs) {
      RunScriptChunk(kFindArea, p.caster);
      std::istringstream entries(engine::GetLocalString(caster, "n5_mask_aoe"));
      std::string entry;
      while (!done && std::getline(entries, entry, ';')) {
        std::istringstream words(entry);
        std::string hex, tag;
        if (!(words >> hex >> tag)) continue;
        const uint32_t id = static_cast<uint32_t>(strtoul(hex.c_str(), nullptr, 16));
        void* aoe = GameObjectById(id);
        if (!aoe || Field<uint8_t>(aoe, ee::kGameObjectType) != ee::kObjectTypeAreaOfEffect ||
            Field<int32_t>(aoe, ee::kAoeEffectSpellId) != p.spell) {
          continue;
        }
        bool held = false;
        for (const Active& a : g_active) held = held || a.aoe == id;
        if (held) continue;
        SetMask(id, p.caster, p.spell, "its area made", id, tag);
        done = true;
      }
    } else if (caster && LogSlot()) {
      Log("spell mask: %s from 0x%08X - no area of effect of its found within %.0f s: no mask", SpellWords(p.spell).c_str(),
          p.caster, kAreaLookMs / 1000.0);
      done = true;
    } else {
      done = true;
    }
    if (done) {
      g_pending.erase(g_pending.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

// A lasting mask's boxes read again; re-sent when they changed. None under areas_fill_around: walls only.
void Recheck(Active* a, const char* why) {
  if (!BoxesCount(*a)) return;
  const std::vector<uint32_t> before = a->box_ids;
  PickBoxes(a, CreatureById(a->caster) ? a->caster : 0);
  if (a->box_ids == before) return;
  a->id = g_next_id++;  // a new id: every client showing it gets it again (its place among the masks kept)
  if (LogSlot()) Log("spell mask: re-sent for %s from 0x%08X - %s: %s", a->what.c_str(), a->caster, why, MaskWords(a->shape).c_str());
}

void Tick() {
  if (!g_registry) return;
  const uint64_t now = GameMs();
  if (now < g_next_tick) return;
  g_next_tick = now + kReconcileMs;
  bool shown = false;
  for (const auto& s : g_shown) shown = shown || s.second != 0;
  if (g_active.empty() && g_pending.empty() && !shown) return;
  LookForAreas(now);
  for (size_t i = 0; i < g_active.size();) {
    Active& a = g_active[i];
    const char* why = nullptr;
    if (!a.lasting) {
      if (now >= a.end_ms) why = a.spell < 0 ? "its time up" : "its look over";
    } else {
      void* aoe = GameObjectById(a.aoe);
      if (!aoe || engine::AreaOf(aoe) != a.area) {
        why = "its area ended";
      } else if (now >= a.checked_ms + kBoxCheckMs) {
        a.checked_ms = now;
        Recheck(&a, "an obstacle in its reach gone");
      }
    }
    if (why) {
      if (LogSlot()) Log("spell mask: cleared for %s from 0x%08X - %s", a.what.c_str(), a.caster, why);
      g_active.erase(g_active.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
  Reconcile(now, 0);
}

void BuildScripts() {
  g_scripts_built = true;
  const std::vector<std::string> impact = GameTableColumn("spells", "ImpactScript");
  for (const rules::SpellMaskEntry& e : rules::SpellMasks()) {
    std::string name = static_cast<size_t>(e.spell) < impact.size() ? impact[static_cast<size_t>(e.spell)] : "";
    if (name.empty()) {
      if (const rules::Spell2024* s = rules::Spell2024Of(e.spell)) name = s->script;
    }
    for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (!name.empty()) g_scripts[name].push_back(e.spell);
  }
  Log("spell mask: %zu impact scripts listened for (%zu spells)", g_scripts.size(), rules::SpellMasks().size());
}

// A masked spell's impact script begins on its caster: its effects swapped for the copies from now (the swap window);
// a burst's mask set (a cone's, when its cast wasn't heard); a lasting area's looked for once it's made.
void ScriptStarts(uint32_t self, const char* name, uint32_t length) {
  if (!g_on || !name || length == 0 || length > 16) return;
  char lower[17];
  for (uint32_t i = 0; i < length; ++i) lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
  lower[length] = '\0';
  if (!g_scripts_built) BuildScripts();
  const auto found = g_scripts.find(lower);
  if (found == g_scripts.end()) return;
  void* object = GameObjectById(self);
  if (!object || Field<uint8_t>(object, ee::kGameObjectType) != ee::kObjectTypeCreature) return;
  const int spell = Field<int32_t>(object, ee::kObjectLastSpellId);
  if (std::find(found->second.begin(), found->second.end(), spell) == found->second.end()) return;
  const rules::SpellMaskEntry* entry = rules::SpellMaskOf(spell);
  if (!entry) return;
  const uint64_t now = GameMs();
  if (g_recent.size() > 256) g_recent.clear();
  g_recent[self] = Recent{spell, now};
  switch (entry->when) {
    case rules::MaskWhen::kCast: {
      const auto cast = g_cast_set.find(Key(self, spell));
      if (cast == g_cast_set.end() || now > cast->second + kCastKeepMs) {
        SetMask(self, self, spell, "as its script began (its cast not heard)", 0, "");
      }
      break;
    }
    case rules::MaskWhen::kImpact: SetMask(self, self, spell, "as it took effect", 0, ""); break;
    case rules::MaskWhen::kLasting: g_pending.push_back(Pending{self, spell, now}); break;
    case rules::MaskWhen::kByCode: break;
  }
}

// A cone's cast begins (CNWSCreature::BroadcastSpellCast): its look plays with the cast, from the caster.
void CastBegins(void* creature, uint32_t spell) {
  if (!g_on || !creature) return;
  const rules::SpellMaskEntry* entry = rules::SpellMaskOf(static_cast<int>(spell));
  if (!entry || entry->when != rules::MaskWhen::kCast) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (g_cast_set.size() > 256) g_cast_set.clear();
  g_cast_set[Key(id, static_cast<int>(spell))] = GameMs();
  SetMask(id, id, static_cast<int>(spell), "as its cast began", 0, "");
}

// The script's EffectVisualEffect / EffectAreaOfEffect: its first argument (the row) swapped for its masked copy's
// while a masked spell's impact script runs on OBJECT_SELF (or just after: the swap window) - the stack's top popped
// and pushed back as the engine's own commands read it.
void Swap(void* commands, bool area) {
  if (!g_on || !commands || !g_vm || !*g_vm || !g_pop_int || !g_push_int) return;
  const uint32_t self = Field<uint32_t>(commands, ee::kVmCommandsObjectSelf);
  const auto recent = g_recent.find(self);
  if (recent == g_recent.end() || GameMs() > recent->second.at + kSwapWindowMs) return;
  int row = 0;
  if (!g_pop_int(*g_vm, &row)) return;
  const int spell = recent->second.spell;
  const int swapped = area ? rules::MaskedAreaRow(spell, row) : rules::MaskedEffectRow(spell, row);
  g_push_int(*g_vm, swapped);
  if (swapped != row && LogSlot()) {
    Log("spell mask: %s's script plays its %s %d as the masked copy %d", SpellWords(spell).c_str(),
        area ? "area (vfx_persistent)" : "effect (visualeffects)", row, swapped);
  }
}

int EffectVisualCommand(void* commands, int command, int parameters) {
  Swap(commands, false);
  return g_effect_visual(commands, command, parameters);
}

int EffectAreaCommand(void* commands, int command, int parameters) {
  Swap(commands, true);
  return g_effect_area(commands, command, parameters);
}

}  // namespace

void InstallSpellMask() {
  const bool faithful = g_config.mode == rules::RuleVariant::kFaithful;
  if ((g_config.spell_mask && faithful) || g_config.debug_mask_proof) g_registry = AddServerTickListener(&Tick);
  if (!g_config.spell_mask) {
    Log("spell masks: off (spell_mask=0) - the spells' looks are the game's, drawn whole");
    return;
  }
  if (!faithful) {
    Log("spell masks: off - not the faithful rules");
    return;
  }
  const std::wstring dll = DllDirectory();
  if (!FileThere(dll + L"\\data\\files\\fsn5pmask.shd")) {
    Log("spell masks: off - no data\\files\\fsn5pmask.shd (build_derived.js's \"particle mask\" line says why)");
    return;
  }
  // Every masked copy the rows name must be there: a row naming a missing model would show nothing.
  std::vector<std::string> named, missing;
  WIN32_FIND_DATAW found;
  const std::wstring rows = dll + L"\\data\\rows\\";
  HANDLE handle = FindFirstFileW((rows + L"*.if_spellmask.txt").c_str(), &found);
  if (handle != INVALID_HANDLE_VALUE) {
    do {
      std::ifstream file((rows + found.cFileName).c_str(), std::ios::binary);
      std::stringstream text;
      text << file.rdbuf();
      // The rows' own lines only: a "#" comment line (the files' headers say "n5k_*") names no model.
      std::string t;
      for (std::string line; std::getline(text, line);) {
        const size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line[first] == '#') continue;
        t += line;
        t += '\n';
      }
      for (char& c : t) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      for (size_t at = t.find("n5k_"); at != std::string::npos; at = t.find("n5k_", at + 4)) {
        size_t end = at + 4;
        while (end < t.size() && (isalnum(static_cast<unsigned char>(t[end])) || t[end] == '_')) ++end;
        if (end == at + 4) continue;  // the bare prefix: no name
        const std::string name = t.substr(at, end - at);
        if (std::find(named.begin(), named.end(), name) != named.end()) continue;
        named.push_back(name);
        const std::wstring wide(name.begin(), name.end());
        if (!FileThere(dll + L"\\data\\files\\" + wide + L".mdl")) missing.push_back(name);
      }
    } while (FindNextFileW(handle, &found));
    FindClose(handle);
  }
  if (named.empty() || !missing.empty()) {
    Log("spell masks: off - %s (build_derived.js's \"spell mask\" lines say why): the spells' looks are the game's",
        named.empty() ? "no data\\rows\\*.if_spellmask.txt names a masked copy"
                      : (std::to_string(missing.size()) + " of the " + std::to_string(named.size()) +
                         " masked copies the rows name aren't in data\\files, " + missing[0] + " first").c_str());
    return;
  }
  ServeConditionalRows("spellmask");
  g_pop_int = reinterpret_cast<PopIntFn>(EngineExport(ee::kStackPopInteger));
  g_push_int = reinterpret_cast<PushIntFn>(EngineExport(ee::kStackPushInteger));
  g_vm = reinterpret_cast<void**>(EngineExport(ee::kVirtualMachine));
  const bool visual = HookExport("script EffectVisualEffect (#184: a masked spell's effect as its copy)", kEffectVisualCommand,
                                 reinterpret_cast<void*>(&EffectVisualCommand), reinterpret_cast<void**>(&g_effect_visual));
  const bool areas = HookExport("script EffectAreaOfEffect (#184: a masked spell's area as its copy)", kEffectAreaCommand,
                                reinterpret_cast<void*>(&EffectAreaCommand), reinterpret_cast<void**>(&g_effect_area));
  const bool heard = AddSpellCastListener(&CastBegins) && AddScriptStartListener(&ScriptStarts);
  g_on = heard && g_registry;
  Log("spell masks: %s - %zu spells' looks masked (%zu copies the rows name, all built); bursts and clouds swapped in "
      "their scripts: effects %s, areas %s",
      g_on ? "on" : "OFF (no spell cast listener, script listener or server tick)", rules::SpellMasks().size(),
      named.size(), visual && g_pop_int && g_push_int && g_vm ? "yes" : "NO", areas ? "yes" : "NO");
}

bool SpellMasksOn() { return g_on; }

void SetSpellMaskNow(void* caster, int spell) {
  if (!g_on || !caster) return;
  const uint32_t id = Field<uint32_t>(caster, ee::kObjectId);
  SetMask(id, id, spell, "at its burst", 0, "");
}

void SpellMaskObstacleGone(void* area, uint32_t id) {
  for (Active& a : g_active) {
    if (!a.lasting || a.area != area) continue;
    char why[96];
    sprintf_s(why, "the placeable 0x%08X broke", id);
    Recheck(&a, why);
  }
  if (!g_active.empty()) Reconcile(GameMs(), 0);
}

void SetProofMask(void* creature, const rules::ParticleMask& mask, int hold_ms) {
  if (!creature) return;
  Active a;
  a.area = engine::AreaOf(creature);
  if (!a.area) return;
  a.caster = Field<uint32_t>(creature, ee::kObjectId);
  a.spell = -1;
  a.set_ms = GameMs();
  a.end_ms = a.set_ms + static_cast<uint64_t>(hold_ms);
  a.shape = mask;
  a.uniforms = rules::PackParticleMask(mask);
  a.what = "the proof";
  Add(std::move(a), "by !maskproof", 0);
}

void DropProofMasks(void* creature) {
  if (!creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  for (size_t i = 0; i < g_active.size();) {
    if (g_active[i].spell < 0 && g_active[i].caster == id) {
      g_active.erase(g_active.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
  Reconcile(GameMs(), id);
}
