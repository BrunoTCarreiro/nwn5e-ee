// #39 Part 1, build 4a: the utility cantrips - Fire Play and the doors (docs/PLAN.md "Phase 4"; owner, 2026-11-07:
// "1A, 2A, 3A, 4 not too about the uses but A, 5 let's look into these deeper, 6 A ...", Q5's closer look, Q8 A, then
// "start building the utility cantrips"), `spellcasting` faithful. One impact script, n5_c_utility, for Elementalism,
// Prestidigitation and Thaumaturgy (each a radial menu of its effects: rules::UtilityEffectOf), Druidcraft (0.93.1:
// Fire Play alone - its Weather Sensor, Bloom and Sensory Effect out, the owner's call) and Mage Hand:
// - The target: the object it was cast at, or a point; Elementalism fills a 5-foot Cube (every light and door within
//   1.5 m of the point). Ranges 30 ft, Prestidigitation's 10 ft (rules::CantripRangeFeet; rules_cantrips.cpp's
//   ComputeSpellRange hook).
// - Lights (light_sources.cpp's FirePlayAt - as a click, a hit or the Light spell: the deactivate animation, the
//   area's lighting, the stealth light rule, the guards who see one go out): Druidcraft, Prestidigitation's Fire
//   Play and Mage Hand light or snuff one; Beckon Fire's embers light those in the Cube, Beckon Water's
//   mist snuffs them; Thaumaturgy's Fire Play dims one out for 1 minute (Q8 A), then it burns again.
// - Doors: Invisible Hand and Mage Hand open an unlocked shut door or shut an open one; Beckon Air's breeze shuts the
//   open ones in the Cube.
// - Booming Voice: Advantage on Intimidation checks for 1 minute (cantrip_effects.h: rules_skill_dcs.cpp's
//   GetIsSkillSuccessful d20, !skill), its icon on the effect bar.
// - Magic Mark: a blue glow at the point (an invisible placeable) for an hour of play, three at a time (the oldest
//   goes).
// - Beckon Earth: a visual and a line (a lasting patch waits for the owner's call, as Beckon Water's puddle).
// Build 4b (owner, 2026-11-08: "go for 4b"; Q1-Q3 "Go with your recommendations, but I'll design the looks"):
// - Sounds at a point (Q6 A, Q7 A): Minor Illusion's (voices, footsteps, a roar, thunder), Thaumaturgy's Phantom
//   Sound (whispers, thunder), Druidcraft's Animal Sound - the game's own sound files. Voices, footsteps and
//   whispers draw the creatures hostile to the caster that hear them (60 ft): they come to look, as for a light put
//   out (heard_investigates). A roar, thunder or the animal's sound makes animals, beasts and vermin flee for 2
//   rounds (rules::FleesFromScaringSound); the others come to look. None fighting, talking or the caster's own.
// - Minor Illusion's image (treasure, a corpse, a humanoid, a small creature: the game's treasure pile, corpse,
//   commoner and leopard) for 1 minute -
//   inert, unseen by the AI, passed through. #152: those that see it react by what they are (rules::ReactionToImage)
//   - a sentient hostile one studies it (d20 + Intelligence against the spell save DC), an animal flees or attacks
//   it, the leopard by size; the treasure drawing hands, the corpse a study or an animal's meal; seen through,
//   hit or touched, it vanishes, and a sentient hostile one searches for the caster.
//   Cast again, it ends.
// - Dancing Lights: four lights (NWN's 5 m yellow light and a glow) at the point, Concentration 1 minute; its
//   Move a Bonus Action (bonus_actions.h), up to 60 ft and within 120 ft (the order's point, action_guard.h).
// - Mage Hand at a lever (its OnUsed with you as its user) or a container (its inventory, as if you stood by it).
// - Beckon Water at the ground: a puddle 1 minute; Beckon Earth: a dust patch 10 minutes (Q1 A; placeholder looks).
// NWN mode: none (they aren't NWN's spells).

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cmath>
#include <deque>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "action_guard.h"
#include "attack_roll.h"
#include "bonus_actions.h"
#include "cantrip_effects.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "light_sources.h"
#include "log.h"
#include "log_once.h"
#include "magic_mark_surface.h"
#include "script_hooks.h"
#include "spell_impact.h"
#include "rules/cantrips.h"
#include "rules/mark_surface.h"
#include "rules/skills.h"

namespace {

using rules::DoorPlay;
using rules::FirePlay;
using rules::UtilityEffect;

constexpr float kMetresPerFoot = 0.3048f;
constexpr float kMarkSeconds = 3600.0f;  // Magic Mark: an hour of play
// 0.96.9 (MO-5; owner, 2026-11-09: "older marks don't fade, they just pop out of existence"): a mark's model fades
// in 1.5 s when its effect ends (its cessation: alpha, glow, size; the client keeps it that long), so an invisible
// object holding one is destroyed this long after - its effect ended first, never the object under it.
constexpr float kMarkFadeLeft = 3.0f;

int g_lines = 0;
bool LogSlot() {
  if (g_lines >= 400) return false;
  ++g_lines;
  return true;
}
LogOnce g_once(500);

std::unordered_map<uint32_t, uint64_t> g_booming;            // Booming Voice: until (game ms), by caster
// Magic Mark: a mark kept - the invisible object holding it (no tag), or its effect's tag on the object it's on.
struct KeptMark {
  uint32_t object = 0;
  std::string tag;
};
std::unordered_map<uint32_t, std::deque<KeptMark>> g_marks;  // Magic Mark: each caster's, oldest first

void Replace(std::string* text, const std::string& key, const std::string& value) {
  for (size_t at = text->find(key); at != std::string::npos; at = text->find(key, at + value.size())) {
    text->replace(at, key.size(), value);
  }
}

void Run(const std::string& code, uint32_t self, const char* what) {
  const int result = RunScriptChunk(code.c_str(), self);
  if (result != 0 && LogSlot()) Log("cantrips: 0x%08X %s - its script returned %d", self, what, result);
}

// The cast's targets - the object it was cast at, or (Elementalism's Cube) every placeable and door within 5 feet of
// its point - as "id;id;", its point kept, and the effect's look ($LOOK, with `t` and `l`).
const char* const kTargets = R"nss(
object t = GetSpellTargetObject();
location l = GetSpellTargetLocation();
if (GetIsObjectValid(t)) l = GetLocation(t);
string r = "";
if ($CUBE) {
  object o = GetFirstObjectInShape(SHAPE_SPHERE, $METRES, l, FALSE, OBJECT_TYPE_PLACEABLE | OBJECT_TYPE_DOOR);
  while (GetIsObjectValid(o)) {
    r += ObjectToString(o) + ";";
    o = GetNextObjectInShape(SHAPE_SPHERE, $METRES, l, FALSE, OBJECT_TYPE_PLACEABLE | OBJECT_TYPE_DOOR);
  }
} else if (GetIsObjectValid(t) && t != OBJECT_SELF) {
  r = ObjectToString(t) + ";";
}
SetLocalString(OBJECT_SELF, "n5c_targets", r);
SetLocalLocation(OBJECT_SELF, "n5c_at", l);
SetLocalInt(OBJECT_SELF, "n5c_at_object", GetIsObjectValid(t) && t != OBJECT_SELF);
SetLocalInt(OBJECT_SELF, "n5c_dc", GetSpellSaveDC());
$LOOK
)nss";

// Each effect's visual and line (the lights' and doors' lines come from what they did).
std::string LookOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kBeckonAir:
      return "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_WIND), l);";
    case UtilityEffect::kBeckonEarth:
      return "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_DUST_EXPLOSION), l);"
             "SendMessageToPC(OBJECT_SELF, \"nwn5e: Elementalism - Beckon Earth: a thin shroud of dust covers the "
             "ground there.\");";
    case UtilityEffect::kBeckonFire:
      return "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_FLAME_S), l);"
             "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_FNF_SMOKE_PUFF), l);";
    case UtilityEffect::kBeckonWater:
      return "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_WATER), l);";
    case UtilityEffect::kBoomingVoice:
      return "effect e = GetFirstEffect(OBJECT_SELF);"
             "while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"n5c_booming\") RemoveEffect(OBJECT_SELF, e);"
             " e = GetNextEffect(OBJECT_SELF); }"
             "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_HEAD_SONIC), OBJECT_SELF);"
             "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(SupernaturalEffect(EffectIcon($ICON)), "
             "\"n5c_booming\"), OBJECT_SELF, $SECONDS);"
             "SendMessageToPC(OBJECT_SELF, \"nwn5e: Thaumaturgy - Booming Voice: your voice booms; Advantage on "
             "Intimidation checks for 1 minute.\");";
    case UtilityEffect::kMageHand:
      return "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_MAGBLUE), l);";
    default:
      return "";
  }
}

// A door at OBJECT_SELF: opened (unlocked, shut) or shut (open); $CLOSE_ONLY shuts only. 1 opened, 2 shut, 3 locked,
// 0 nothing, -1 not a door (stored + 10).
const char* const kDoor = R"nss(
int r = -1;
if (GetObjectType(OBJECT_SELF) == OBJECT_TYPE_DOOR) {
  if (GetIsOpen(OBJECT_SELF)) { ActionCloseDoor(OBJECT_SELF); r = 2; }
  else if ($CLOSE_ONLY) r = 0;
  else if (GetLocked(OBJECT_SELF)) r = 3;
  else { ActionOpenDoor(OBJECT_SELF); r = 1; }
}
SetLocalInt(OBJECT_SELF, "n5c_door", r + 10);
SetLocalString(OBJECT_SELF, "n5c_name", GetName(OBJECT_SELF));
)nss";

int DoorAt(uint32_t id, DoorPlay play, std::string* name) {
  void* object = GameObjectById(id);
  if (!object) return -1;
  engine::SetLocalInt(object, "n5c_door", 0);
  std::string code = kDoor;
  Replace(&code, "$CLOSE_ONLY", play == DoorPlay::kClose ? "TRUE" : "FALSE");
  Run(code, id, "a door");
  const int stored = engine::GetLocalInt(object, "n5c_door");
  if (name) *name = engine::GetLocalString(object, "n5c_name");
  return stored ? stored - 10 : -1;
}

// Magic Mark: one of the owner's symbols (0.94.1: rules::MagicMarkVisual, a random one) flat on the ground at the
// point, turned a random way, for an hour of play; the caster's fourth takes its oldest away. 0.95.10 (the owner,
// UT-15: "the magic marks are not on the ground, they are floating"): the point goes down to the walkmesh
// (GetGroundHeight) - a click can give a point above it - and how far it went is logged.
const char* const kMark = R"nss(
location l = GetLocalLocation(OBJECT_SELF, "n5c_at");
vector p = GetPositionFromLocation(l);
float g = GetGroundHeight(l);
SetLocalString(OBJECT_SELF, "n5c_mark_drop", g > -6.0 ? FloatToString(p.z - g, 0, 2) : "no ground");
if (g > -6.0) p.z = g;
l = Location(GetAreaFromLocation(l), p, IntToFloat(Random(360)));
object m = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", l, FALSE, "n5c_mark");
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect($VFX), m, $SECONDS);
DestroyObject(m, $SECONDS + $FADE_LEFT);
SetLocalString(OBJECT_SELF, "n5c_mark", ObjectToString(m));
SendMessageToPC(OBJECT_SELF, "nwn5e: Prestidigitation - Magic Mark: a glowing mark there for an hour (three at a time).");
)nss";

// 0.96.0 (owner, 2026-11-08: "Go with A"; SRD 5.2's mark appears "on an object or a surface"): cast at a door or a
// placeable, the mark stands on it, upright, on the caster's side - rules::MarkOnDoor / MarkOnPlaceable. A placeable's
// surface is where its walkmesh starts on the line from the caster (the area's static-object line test, as Push's).
// 0.96.2: and the caster's height and size (its eyes), and the marks already on the object (their models).
const char* const kMarkTarget = R"nss(
object t = StringToObject("$TARGET");
vector p = GetPosition(t);
vector c = GetPosition(OBJECT_SELF);
string used = "";
effect e = GetFirstEffect(t);
while (GetIsEffectValid(e)) {
  string g = GetEffectTag(e);
  if (GetStringLeft(g, 8) == "n5c_mrk_") used += GetSubString(g, 8, GetStringLength(g) - 8) + " ";
  e = GetNextEffect(t);
}
SetLocalString(OBJECT_SELF, "n5c_mark_used", used);
SetLocalString(OBJECT_SELF, "n5c_mark_obj", IntToString(GetObjectType(t)) + " " + FloatToString(p.x, 0, 3) + " " +
               FloatToString(p.y, 0, 3) + " " + FloatToString(p.z, 0, 3) + " " + FloatToString(GetFacing(t), 0, 2) +
               " " + FloatToString(c.x, 0, 3) + " " + FloatToString(c.y, 0, 3) + " " + FloatToString(c.z, 0, 3) +
               " " + IntToString(GetCreatureSize(OBJECT_SELF)) + " " + GetName(t));
)nss";
const char* const kMarkAt = R"nss(
object a = GetArea(OBJECT_SELF);
vector p = Vector($X, $Y, $Z);
if ($ON_GROUND) {
  float g = GetGroundHeight(Location(a, p, 0.0));
  if (g > -6.0) p.z = g + $LIFT;
}
object m = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", Location(a, p, $FACING), FALSE, "n5c_mark");
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect($VFX), m, $SECONDS);
DestroyObject(m, $SECONDS + $FADE_LEFT);
SetLocalString(OBJECT_SELF, "n5c_mark", ObjectToString(m));
SendMessageToPC(OBJECT_SELF, "nwn5e: Prestidigitation - Magic Mark on $WHAT: a glowing mark for an hour (three at a time).");
)nss";
// 0.96.2 (owner, 2026-11-09: "go with 1"; MO-1: "the door swings open but the magic mark stays in place", MO-2: "on
// a table it gets placed somehow inside the model"): the mark on the object itself, on its model's surface where the
// caster's eyes meet it - magic_mark_surface.cpp places it as the client attaches it, from the record kept here (the
// eyes). Each mark on an object is named by its model (its effect's tag, its record), so its symbol is one not on
// the object already in that colour.
const char* const kMarkOnSurface = R"nss(
object t = StringToObject("$TARGET");
SetLocalString(t, "$KEY", "$RECORD");
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(EffectVisualEffect($VFX), "$TAG"), t, $SECONDS);
SendMessageToPC(OBJECT_SELF, "nwn5e: Prestidigitation - Magic Mark on $WHAT: a glowing mark for an hour (three at a time).");
)nss";
constexpr const char* kSurfaceMarkTag = "n5c_mrk_";     // + the model: the mark's effect on the object
constexpr const char* kSurfaceMarkRecord = "n5m_";      // + the model: its record (rules/mark_surface.h)
constexpr int kObjectTypeDoorScript = 8;        // OBJECT_TYPE_DOOR
constexpr int kObjectTypePlaceableScript = 64;  // OBJECT_TYPE_PLACEABLE
constexpr int kSurfaceSteps = 14;               // the walkmesh's start to within ~1/16000 of the line

using StaticLineFn = int (*)(void* area, float x1, float y1, float x2, float y2, float z1, float z2, float radius,
                             uint32_t exclude, uint32_t* blocker, int flag, void* exclude_list, int last);

std::vector<uint32_t> ParseIds(const std::string& list);

// The new mark into the caster's three (the oldest goes: its invisible object, or its effect on the object it's on).
void KeepMark(uint32_t self, const KeptMark& mark, const std::string& where) {
  if (!mark.object || mark.object == ee::kObjectInvalid) return;
  std::deque<KeptMark>& marks = g_marks[self];
  marks.push_back(mark);
  while (marks.size() > static_cast<size_t>(rules::kMagicMarksAtOnce)) {
    const KeptMark& oldest = marks.front();
    char code[320];
    if (oldest.tag.empty()) {
      sprintf_s(code,
                "object m = StringToObject(\"%x\"); effect e = GetFirstEffect(m); while (GetIsEffectValid(e)) { "
                "RemoveEffect(m, e); e = GetNextEffect(m); } DestroyObject(m, %.1f);",
                oldest.object, kMarkFadeLeft);
    } else {
      sprintf_s(code,
                "object t = StringToObject(\"%x\"); effect e = GetFirstEffect(t); while (GetIsEffectValid(e)) { "
                "if (GetEffectTag(e) == \"%s\") RemoveEffect(t, e); e = GetNextEffect(t); }",
                oldest.object, oldest.tag.c_str());
    }
    Run(code, self, "Magic Mark's oldest");
    marks.pop_front();
  }
  if (LogSlot()) {
    Log("cantrips: 0x%08X Magic Mark %x%s%s (%zu up; %s)", self, mark.object, mark.tag.empty() ? "" : " ",
        mark.tag.c_str(), marks.size(), where.c_str());
  }
}

// The invisible object the cast's script just made (its id in the caster's "n5c_mark").
KeptMark HolderMark(void* caster) {
  const std::string mark = engine::GetLocalString(caster, "n5c_mark");
  KeptMark kept;
  if (!mark.empty()) kept.object = static_cast<uint32_t>(strtoul(mark.c_str(), nullptr, 16));
  return kept;
}

std::string Quoteless(std::string text) {
  for (char& c : text) {
    if (c == '"' || c == '\\') c = '\'';
  }
  return text;
}

// 0.96.1 (owner, 2026-11-09: "Option b + random"): the mark's colour - the menu entry cast, Random one of the six.
const char* const kTintNames[rules::kMagicMarkTints] = {"white", "gold", "red", "green", "blue", "violet"};

int TintOfCast(void* caster) {
  const int tint = rules::MagicMarkTintOf(Field<int32_t>(caster, ee::kObjectLastSpellId));
  return tint >= 0 ? tint : RollEngineDice(1, rules::kMagicMarkTints) - 1;
}

// 0.96.2: the mark's effect on the object, a symbol not on it already in that colour; its record the caster's eyes.
// 0.96.3: with the click's ray when the click was at this object.
void MarkOnSurface(uint32_t self, void* caster, uint32_t target, const std::string& name, const char* kind, int tint,
                   const rules::Vec3f& eye, const std::optional<MarkClick>& click) {
  const std::string used = " " + engine::GetLocalString(caster, "n5c_mark_used");
  std::vector<bool> taken(rules::kMagicMarkVisuals, false);
  for (int symbol = 1; symbol <= rules::kMagicMarkVisuals; ++symbol) {
    taken[symbol - 1] = used.find(" " + rules::MagicMarkModel(symbol, tint, true) + " ") != std::string::npos;
  }
  const int symbol = rules::FreeMarkSymbol(RollEngineDice(1, rules::FreeMarkSymbols(taken)), taken);
  const std::string model = rules::MagicMarkModel(symbol, tint, true);
  rules::MarkRecord record;
  record.eye = eye;
  const bool clicked = click && click->target == target;
  if (clicked) record.click = rules::MarkRay{click->camera, click->point};
  std::string code = kMarkOnSurface;
  char number[32];
  sprintf_s(number, "%x", target);
  Replace(&code, "$TARGET", number);
  Replace(&code, "$KEY", kSurfaceMarkRecord + model);
  Replace(&code, "$RECORD", rules::MarkRecordText(record));
  Replace(&code, "$TAG", kSurfaceMarkTag + model);
  Replace(&code, "$VFX", std::to_string(rules::MagicMarkRow(symbol, tint, true)));
  sprintf_s(number, "%.1f", kMarkSeconds);
  Replace(&code, "$SECONDS", number);
  Replace(&code, "$WHAT", name.empty() ? std::string("it") : name);
  Run(code, self, "Magic Mark on an object's surface");
  char where[240];
  sprintf_s(where, "%s, on %s (a %s; %s - on its model's surface as the client attaches it)", kTintNames[tint],
            name.empty() ? "it" : name.c_str(), kind, clicked ? "where the cursor was" : "aimed from the eyes");
  KeptMark kept;
  kept.object = target;
  kept.tag = kSurfaceMarkTag + model;
  KeepMark(self, kept, where);
}

// A door or a placeable cast at: the upright mark on it. False when the target is neither (the ground's mark then).
// 0.96.2: on its model's surface (MarkOnSurface); 0.96.0's invisible object at its slab or walkmesh when the client's
// hit test isn't as read (magic_mark_surface.cpp's install).
bool MarkOnObject(uint32_t self, void* caster, int tint, const std::optional<MarkClick>& click) {
  if (!engine::GetLocalInt(caster, "n5c_at_object")) return false;
  const std::vector<uint32_t> ids = ParseIds(engine::GetLocalString(caster, "n5c_targets"));
  if (ids.empty()) return false;
  const uint32_t target = ids.front();
  std::string code = kMarkTarget;
  char id[16];
  sprintf_s(id, "%x", target);
  Replace(&code, "$TARGET", id);
  Run(code, self, "Magic Mark's object");
  const std::string facts = engine::GetLocalString(caster, "n5c_mark_obj");
  int kind = 0, size = 0, name_at = 0;
  float px = 0, py = 0, pz = 0, facing = 0, cx = 0, cy = 0, cz = 0;
  if (sscanf_s(facts.c_str(), "%d %f %f %f %f %f %f %f %d %n", &kind, &px, &py, &pz, &facing, &cx, &cy, &cz, &size,
               &name_at) < 9) {
    return false;
  }
  if (kind != kObjectTypeDoorScript && kind != kObjectTypePlaceableScript) return false;
  const std::string name = Quoteless(name_at > 0 && name_at <= static_cast<int>(facts.size()) ? facts.substr(name_at)
                                                                                               : std::string());
  if (MagicMarkSurfaceReady()) {
    MarkOnSurface(self, caster, target, name, kind == kObjectTypeDoorScript ? "door" : "placeable", tint,
                  rules::Vec3f{cx, cy, cz + rules::MarkEyeHeight(size)}, click);
    return true;
  }
  rules::MarkSpot spot;
  std::string how;
  if (kind == kObjectTypeDoorScript) {
    spot = rules::MarkOnDoor(px, py, pz, facing, cx, cy);
    how = "a door";
  } else {
    static const StaticLineFn static_line = reinterpret_cast<StaticLineFn>(EngineExport(ee::kStaticObjectsOnLine));
    void* area = engine::AreaOf(caster);
    float surface = -1.0f;
    if (static_line && area) {
      surface = rules::ClearShare(
          [&](float share) {
            uint32_t blocker = ee::kObjectInvalid;
            const int clear = static_line(area, cx, cy, cx + (px - cx) * share, cy + (py - cy) * share, pz - 0.1f,
                                          pz + 1.8f, 0.01f, ee::kObjectInvalid, &blocker, 1, nullptr, 0);
            return clear != 1 && blocker == target;
          },
          kSurfaceSteps);
    }
    spot = rules::MarkOnPlaceable(px, py, cx, cy, surface, pz);
    char text[64];
    if (surface >= 0.0f) {
      sprintf_s(text, "a placeable, its walkmesh %.2f of the way", surface);
    } else {
      sprintf_s(text, "a placeable, no walkmesh found");
    }
    how = text;
  }
  std::string mark = kMarkAt;
  char number[32];
  sprintf_s(number, "%.3f", spot.x);
  Replace(&mark, "$X", number);
  sprintf_s(number, "%.3f", spot.y);
  Replace(&mark, "$Y", number);
  sprintf_s(number, "%.3f", spot.z);
  Replace(&mark, "$Z", number);
  sprintf_s(number, "%.2f", spot.facing);
  Replace(&mark, "$FACING", number);
  Replace(&mark, "$ON_GROUND", kind == kObjectTypePlaceableScript ? "TRUE" : "FALSE");
  sprintf_s(number, "%.2f", rules::kMarkObjectHeight);
  Replace(&mark, "$LIFT", number);
  sprintf_s(number, "%.1f", kMarkSeconds);
  Replace(&mark, "$SECONDS", number);
  sprintf_s(number, "%.1f", kMarkFadeLeft);
  Replace(&mark, "$FADE_LEFT", number);
  Replace(&mark, "$VFX",
          std::to_string(rules::MagicMarkRow(RollEngineDice(1, rules::kMagicMarkVisuals), tint, true)));
  Replace(&mark, "$WHAT", name.empty() ? std::string("it") : name);
  CreatingFireLight(true);  // light_sources.cpp: the invisible object a normal placeable, so the client gets its glow
  Run(mark, self, "Magic Mark on an object");
  CreatingFireLight(false);
  char where[160];
  sprintf_s(where, "%s, on %s (%s) at %.2f %.2f %.2f facing %.0f", kTintNames[tint], name.empty() ? "it" : name.c_str(),
            how.c_str(), spot.x, spot.y, spot.z, spot.facing);
  KeepMark(self, HolderMark(caster), where);
  return true;
}

// 0.96.3 (owner, 2026-11-09: "A. but we should try to find a way to deal with walls that have floors behind them so
// that the wall is marked and not the floor"): a click on a wall - the cursor's point wasn't the floor the game sent
// (magic_mark_surface.cpp moved the cast in front of it) - marks the wall at the cursor: upright, facing out of it
// (flat on a ledge's top), on an invisible object there.
const char* const kMarkOnWall = R"nss(
object m = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", Location(GetArea(OBJECT_SELF), Vector($X, $Y, $Z), $FACING), FALSE, "n5c_mark");
ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect($VFX), m, $SECONDS);
DestroyObject(m, $SECONDS + $FADE_LEFT);
SetLocalString(OBJECT_SELF, "n5c_mark", ObjectToString(m));
SendMessageToPC(OBJECT_SELF, "nwn5e: Prestidigitation - Magic Mark: a glowing mark there for an hour (three at a time).");
)nss";

void MarkOnWall(uint32_t self, void* caster, int tint, const MarkClick& click) {
  const bool upright = rules::MarkStandsUpright(click.normal);
  const rules::Vec3f at =
      rules::Add(click.point, rules::Scale(rules::Normalized(click.normal), rules::kMarkSurfaceGap));
  const float facing = upright ? rules::MarkFacingOf(click.normal) : static_cast<float>(RollEngineDice(1, 360) - 1);
  std::string code = kMarkOnWall;
  char number[32];
  sprintf_s(number, "%.3f", at.x);
  Replace(&code, "$X", number);
  sprintf_s(number, "%.3f", at.y);
  Replace(&code, "$Y", number);
  sprintf_s(number, "%.3f", at.z);
  Replace(&code, "$Z", number);
  sprintf_s(number, "%.2f", facing);
  Replace(&code, "$FACING", number);
  sprintf_s(number, "%.1f", kMarkSeconds);
  Replace(&code, "$SECONDS", number);
  sprintf_s(number, "%.1f", kMarkFadeLeft);
  Replace(&code, "$FADE_LEFT", number);
  Replace(&code, "$VFX", std::to_string(rules::MagicMarkRow(RollEngineDice(1, rules::kMagicMarkVisuals), tint, upright)));
  CreatingFireLight(true);  // light_sources.cpp: the invisible object a normal placeable, so the client gets its glow
  Run(code, self, "Magic Mark on a wall");
  CreatingFireLight(false);
  char where[200];
  sprintf_s(where, "%s, on a wall at %.2f %.2f %.2f, %s %.0f (where the cursor was; the game sent the floor behind it)",
            kTintNames[tint], at.x, at.y, at.z, upright ? "upright facing" : "flat, turned", facing);
  KeepMark(self, HolderMark(caster), where);
}

void PlaceMark(uint32_t self, void* caster) {
  const int tint = TintOfCast(caster);
  const std::optional<MarkClick> click =
      TakeMarkClick(self, static_cast<uint32_t>(Field<int32_t>(caster, ee::kObjectLastSpellId)));
  if (MarkOnObject(self, caster, tint, click)) return;
  if (click && click->wall) {
    MarkOnWall(self, caster, tint, *click);
    return;
  }
  std::string code = kMark;
  char seconds[32];
  sprintf_s(seconds, "%.1f", kMarkSeconds);
  Replace(&code, "$SECONDS", seconds);
  sprintf_s(seconds, "%.1f", kMarkFadeLeft);
  Replace(&code, "$FADE_LEFT", seconds);
  Replace(&code, "$VFX",
          std::to_string(rules::MagicMarkRow(RollEngineDice(1, rules::kMagicMarkVisuals), tint, false)));
  CreatingFireLight(true);  // light_sources.cpp: the invisible object a normal placeable, so the client gets its glow
  Run(code, self, "Magic Mark");
  CreatingFireLight(false);
  KeepMark(self, HolderMark(caster), std::string(kTintNames[tint]) + ", on the ground; the point dropped " +
                             engine::GetLocalString(caster, "n5c_mark_drop") + " m to it");
}

std::vector<uint32_t> ParseIds(const std::string& list) {
  std::vector<uint32_t> ids;
  size_t at = 0;
  while (at < list.size()) {
    const size_t end = list.find(';', at);
    if (end == std::string::npos) break;
    if (end > at) ids.push_back(static_cast<uint32_t>(strtoul(list.substr(at, end - at).c_str(), nullptr, 16)));
    at = end + 1;
  }
  return ids;
}

// What the effect says when it found nothing to work on.
const char* NothingLine(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kFirePlay: return "no light there to light or snuff (a lamp, a brazier, a torch, a candle)";
    case UtilityEffect::kThaumaturgyFirePlay: return "no burning light there to dim";
    case UtilityEffect::kBeckonFire: return "no unlit light within 5 feet for its embers";
    case UtilityEffect::kBeckonWater: return "no burning light within 5 feet for its mist";
    case UtilityEffect::kBeckonAir: return "no open door within 5 feet for its breeze";
    case UtilityEffect::kInvisibleHand: return "no door there";
    case UtilityEffect::kMageHand: return "nothing there the hand can work - a door, a light, a lever or a container";
    default: return "nothing happens";
  }
}

// --- Build 4b (owner, 2026-11-08: "go for 4b"): the sounds, the illusions, Dancing Lights, the patches, Mage Hand's
// levers and containers ----------------------------------------------------------------------------------------

using rules::ImageKind;
using rules::SoundPlay;

bool Investigates() { return g_config.heard_investigates && g_config.perception == rules::RuleVariant::kFaithful; }

std::string Hex(uint32_t id) {
  char text[16];
  sprintf_s(text, "%x", id);
  return text;
}

std::string Num(float value) {
  char text[32];
  sprintf_s(text, "%.2f", value);
  return text;
}

// The creatures near the cast's point (n5c_at) that a sound or an image may move: not the caster's own (its party,
// its associates), none fighting, talking or plot - "id:race:enemy:int:score:size;" (race NWN's racial type, enemy 1
// when hostile to the caster, int the Intelligence modifier, score its Intelligence, size NWN's creature size); $SIGHT
// TRUE for those with a line to the point.
const char* const kNear = R"nss(
location l = GetLocalLocation(OBJECT_SELF, "n5c_at");
object u = OBJECT_SELF;
string r = "";
object o = GetFirstObjectInShape(SHAPE_SPHERE, $METRES, l, $SIGHT, OBJECT_TYPE_CREATURE);
while (GetIsObjectValid(o)) {
  if (o != u && !GetIsPC(o) && !GetIsDead(o) && !GetIsInCombat(o) && GetCommandable(o) && !IsInConversation(o) &&
      !GetPlotFlag(o) && !GetIsObjectValid(GetMaster(o)) && !GetFactionEqual(o, u) && !GetLocalInt(o, "nwn5e_seek") &&
      GetTag(o) != "n5c_image") {
    r += ObjectToString(o) + ":" + IntToString(GetRacialType(o)) + ":" + IntToString(GetIsEnemy(u, o)) + ":" +
         IntToString(GetAbilityModifier(ABILITY_INTELLIGENCE, o)) + ":" +
         IntToString(GetAbilityScore(o, ABILITY_INTELLIGENCE)) + ":" + IntToString(GetCreatureSize(o)) + ";";
  }
  o = GetNextObjectInShape(SHAPE_SPHERE, $METRES, l, $SIGHT, OBJECT_TYPE_CREATURE);
}
SetLocalString(OBJECT_SELF, "n5c_near", r);
)nss";

struct Near {
  uint32_t id = 0;
  int race = 0;
  bool enemy = false;
  int intelligence = 0;  // its modifier
  int intelligence_score = 10;
  int size = 3;  // NWN's creature size (1 tiny .. 5 huge)
};

std::vector<Near> NearOf(void* caster, uint32_t self, float metres, bool sight) {
  std::string code = kNear;
  Replace(&code, "$METRES", Num(metres));
  Replace(&code, "$SIGHT", sight ? "TRUE" : "FALSE");
  Run(code, self, "the creatures near");
  std::vector<Near> nearby;
  const std::string list = engine::GetLocalString(caster, "n5c_near");
  size_t at = 0;
  while (at < list.size()) {
    const size_t end = list.find(';', at);
    if (end == std::string::npos) break;
    Near n;
    unsigned int id = 0;
    int enemy = 0;
    if (sscanf_s(list.substr(at, end - at).c_str(), "%x:%d:%d:%d:%d:%d", &id, &n.race, &enemy, &n.intelligence,
                 &n.intelligence_score, &n.size) == 6) {
      n.id = id;
      n.enemy = enemy != 0;
      nearby.push_back(n);
    }
    at = end + 1;
  }
  return nearby;
}

// A creature fleeing the point for 2 rounds (Q7), as the caster's script.
const char* const kFlee = R"nss(
object o = StringToObject("$ID");
location l = GetLocalLocation(OBJECT_SELF, "n5c_at");
SetLocalInt(o, "nwn5e_seek", 1);
AssignCommand(o, ClearAllActions());
AssignCommand(o, ActionMoveAwayFromLocation(l, TRUE, 30.0));
DelayCommand($SECONDS, AssignCommand(o, ClearAllActions()));
DelayCommand($SECONDS, DeleteLocalInt(o, "nwn5e_seek"));
SetLocalString(OBJECT_SELF, "n5c_name", GetName(o));
)nss";

// A creature coming to look at the point - searching, a look round, then home (as a light put out draws one,
// light_sources.cpp); $ARRIVE runs as it gets there (an image's study).
const char* const kLook = R"nss(
object o = StringToObject("$ID");
object u = OBJECT_SELF;
location l = GetLocalLocation(OBJECT_SELF, "n5c_at");
SetLocalInt(o, "nwn5e_seek", 25);
location home = GetLocation(o);
AssignCommand(o, ClearAllActions());
SetActionMode(o, ACTION_MODE_DETECT, TRUE);
AssignCommand(o, ActionMoveToLocation(l));
$ARRIVE
AssignCommand(o, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_LEFT));
AssignCommand(o, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_RIGHT));
AssignCommand(o, ActionWait($WAIT));
AssignCommand(o, ActionMoveToLocation(home));
AssignCommand(o, ActionDoCommand(SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, FALSE)));
AssignCommand(o, ActionDoCommand(DeleteLocalInt(OBJECT_SELF, "nwn5e_seek")));
SetLocalString(OBJECT_SELF, "n5c_name", GetName(o));
)nss";

std::string Joined(const std::vector<std::string>& parts) {
  std::string line;
  for (size_t i = 0; i < parts.size(); ++i) line += (i == 0 ? "" : i + 1 == parts.size() ? " and " : ", ") + parts[i];
  return line;
}

// Minor Illusion's earlier image ends with a new cast (SRD 5.2.1: "The illusion ends if you cast this spell again").
void EndImage(uint32_t self) {
  Run("object old = GetLocalObject(OBJECT_SELF, \"n5c_image\"); if (GetIsObjectValid(old)) DestroyObject(old);"
      "DeleteLocalObject(OBJECT_SELF, \"n5c_image\");",
      self, "Minor Illusion's earlier image");
}

bool IsMinorIllusion(int row) {
  const rules::ChoiceCantrip* cantrip = rules::ChoiceCantripOf(row);
  return cantrip && cantrip->master == rules::kMinorIllusion;
}

// A sound at the point (Q6 A, Q7 A): its creatures - animals, beasts and vermin flee a scaring one for 2 rounds;
// the others hostile to the caster come to look (heard_investigates); none fighting.
const char* const kSound = R"nss(
location l = GetLocalLocation(OBJECT_SELF, "n5c_at");
object s = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", l, FALSE, "n5c_sound");
AssignCommand(s, PlaySound("$SOUND"));
$MORE
DestroyObject(s, 8.0);
)nss";

void PlaySoundAt(uint32_t self, void* caster, UtilityEffect effect, const std::string& name) {
  const SoundPlay play = rules::SoundPlayOf(effect);
  std::string code = kSound;
  Replace(&code, "$SOUND", rules::SoundResrefOf(effect));
  // Footsteps: a few, one after another.
  Replace(&code, "$MORE", effect == UtilityEffect::kIllusionFootsteps
                              ? "DelayCommand(0.6, AssignCommand(s, PlaySound(\"fs_dirt_hard2\")));"
                                "DelayCommand(1.2, AssignCommand(s, PlaySound(\"fs_dirt_hard3\")));"
                                "DelayCommand(1.8, AssignCommand(s, PlaySound(\"fs_dirt_hard1\")));"
                              : "");
  CreatingFireLight(true);  // light_sources.cpp: the invisible object a normal placeable, so the clients hear it
  Run(code, self, "a sound");
  CreatingFireLight(false);
  std::vector<std::string> done;
  for (const Near& n : NearOf(caster, self, rules::kSoundHearingFeet * kMetresPerFoot, false)) {
    if (play == SoundPlay::kScares && rules::FleesFromScaringSound(n.race)) {
      std::string flee = kFlee;
      Replace(&flee, "$ID", Hex(n.id));
      Replace(&flee, "$SECONDS", std::to_string(rules::kScareSeconds) + ".0");
      Run(flee, self, "a creature fleeing");
      done.push_back(engine::GetLocalString(caster, "n5c_name") + " flees");
    } else if (n.enemy && Investigates()) {
      std::string look = kLook;
      Replace(&look, "$ID", Hex(n.id));
      Replace(&look, "$ARRIVE", "");
      Replace(&look, "$WAIT", "6.0");
      Run(look, self, "a creature coming to look");
      done.push_back(engine::GetLocalString(caster, "n5c_name") + " comes to look");
    }
  }
  TellPlayer(self, "nwn5e: " + name + " - " + (done.empty() ? std::string("no one near reacts") : Joined(done)) + ".");
  if (LogSlot()) Log("cantrips: 0x%08X %s (%s): %zu react", self, name.c_str(), rules::SoundResrefOf(effect), done.size());
}

// Minor Illusion's image at the point for 1 minute - the game's own treasure pile, corpse, commoner or leopard
// (an object named for the menu), inert: plot, unseen by
// the AI (EffectEthereal, tagged: a creature that attacks it sees it for the blow, #152) and passed through
// (EffectCutsceneGhost: "things can pass through it"). Its object, name and size are kept on the caster.
const char* const kImage = R"nss(
location l = GetLocalLocation(OBJECT_SELF, "n5c_at");
object i;
if ($CREATURE) {
  i = CreateObject(OBJECT_TYPE_CREATURE, "$BLUEPRINT", l, FALSE, "n5c_image");
  ChangeToStandardFaction(i, STANDARD_FACTION_COMMONER);
  SetPlotFlag(i, TRUE);
  AssignCommand(i, ClearAllActions(TRUE));
  ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectEthereal()), "n5c_ethereal"), i);
  ApplyEffectToObject(DURATION_TYPE_PERMANENT, SupernaturalEffect(EffectCutsceneGhost()), i);
  SetCommandable(FALSE, i);
} else {
  i = CreateObject(OBJECT_TYPE_PLACEABLE, "$BLUEPRINT", l, FALSE, "n5c_image");
  SetUseableFlag(i, FALSE);
  SetPlotFlag(i, TRUE);
  SetName(i, "$NAME");
}
SetLocalObject(OBJECT_SELF, "n5c_image", i);
DestroyObject(i, $SECONDS);
SetLocalString(OBJECT_SELF, "n5c_image_name", GetName(i));
SetLocalString(OBJECT_SELF, "n5c_image_id", ObjectToString(i));
SetLocalInt(OBJECT_SELF, "n5c_image_size", $CREATURE ? GetCreatureSize(i) : 0);
)nss";

// --- #152 (owner, 2026-11-09: "by alignment I meant disposition towards the player, eg enemies. go for A on all"):
// the creatures that see the image react by what they are (rules::ReactionToImage) - a sentient hostile one comes to
// study it, an animal flees or attacks it, the leopard by size. Each is a Reactor: its steps run as its own scripts,
// and its arrivals and timers call back here through nwn5e's script handlers (ExecuteScript("n5_mi_...")), each call
// marked with its serial (n5c_mi_ping) so that an older reaction's timer can't move a newer one. A blow at an image
// misses (an attack-roll skipper: "things can pass through it") and the image vanishes for everyone (Q4 A), as it
// does when a study sees through it.
using rules::AfterStudy;
using rules::ImageReaction;

enum class Stage { kComing, kTouching, kFleeing, kFledAgain, kCharging, kStriking, kSearching };

struct Reactor {
  uint32_t caster = 0;
  uint32_t image = 0;
  ImageKind kind = ImageKind::kNone;
  ImageReaction reaction = ImageReaction::kIgnore;
  bool hostile = false;
  bool sentient = false;
  int intelligence = 0;  // its modifier, for the study
  int dc = 0;
  int serial = 0;
  Stage stage = Stage::kComing;
  std::string name;
  std::string image_name;
};
std::unordered_map<uint32_t, Reactor> g_reactors;  // by creature

struct ImageMade {
  uint32_t caster = 0;
  std::string name;
};
std::unordered_map<uint32_t, ImageMade> g_images;     // by image object
std::vector<std::pair<uint32_t, uint32_t>> g_struck;  // (image, attacker): blows at an image, for the next update
int g_serial = 0;

const char* const kStudyScript = "n5_mi_study";    // got there: it studies the image
const char* const kStrikeScript = "n5_mi_strike";  // got there: it attacks it
const char* const kBackScript = "n5_mi_back";      // its 2 rounds of flight are over
const char* const kLateScript = "n5_mi_late";      // it never got there, or no blow landed
const char* const kDoneScript = "n5_mi_done";      // its search is over
constexpr float kLateSeconds = 30.0f;              // to get there
constexpr float kBlowSeconds = 4.0f;               // for a blow to land

// The handler `script`, called as the creature after `seconds` (not an action: ClearAllActions leaves it).
std::string Later(float seconds, const char* script, int serial) {
  return "DelayCommand(" + Num(seconds) + ", SetLocalInt(OBJECT_SELF, \"n5c_mi_ping\", " + std::to_string(serial) +
         "));\nDelayCommand(" + Num(seconds + 0.05f) + ", ExecuteScript(\"" + script + "\", OBJECT_SELF));\n";
}

// The handler `script`, called as the creature's next action.
std::string Next(const char* script, int serial) {
  return "ActionDoCommand(SetLocalInt(OBJECT_SELF, \"n5c_mi_ping\", " + std::to_string(serial) +
         "));\nActionDoCommand(ExecuteScript(\"" + script + "\", OBJECT_SELF));\n";
}

// The creature's reaction, when the call is its own (not an older reaction's timer).
Reactor* ReactorCalled(uint32_t self) {
  const auto found = g_reactors.find(self);
  if (found == g_reactors.end()) return nullptr;
  void* creature = CreatureById(self);
  if (!creature || engine::GetLocalInt(creature, "n5c_mi_ping") != found->second.serial) return nullptr;
  return &found->second;
}

// Whether it fights (or is dead or gone), and whether its image is still there.
struct Check {
  bool fighting = true;
  bool image_there = false;
};
Check CheckOf(uint32_t self) {
  Run("SetLocalInt(OBJECT_SELF, \"n5c_mi_fight\", GetIsInCombat(OBJECT_SELF) || GetIsDead(OBJECT_SELF));"
      "SetLocalInt(OBJECT_SELF, \"n5c_mi_there\", GetIsObjectValid(GetLocalObject(OBJECT_SELF, \"n5c_mi_image\")));",
      self, "an illusion's watcher checked");
  Check check;
  void* creature = CreatureById(self);
  if (!creature) return check;
  check.fighting = engine::GetLocalInt(creature, "n5c_mi_fight") != 0;
  check.image_there = engine::GetLocalInt(creature, "n5c_mi_there") != 0;
  return check;
}

std::string NameOf(uint32_t id) {
  Run("SetLocalString(OBJECT_SELF, \"n5c_mi_name\", GetName(OBJECT_SELF));", id, "a name");
  void* creature = CreatureById(id);
  return creature ? engine::GetLocalString(creature, "n5c_mi_name") : std::string("something");
}

void Tell(uint32_t caster, const std::string& line) {
  if (caster) TellPlayer(caster, "nwn5e: " + line + ".");
}

void Note(uint32_t self, const Reactor& r, const char* what) {
  if (LogSlot()) Log("cantrips: 0x%08X Minor Illusion's %s: 0x%08X (%s) %s", r.caster, r.image_name.c_str(), self,
                     r.name.c_str(), what);
}

// Left where it stands - fighting, or its flight over (`clear`: its orders ended too).
void LetGo(uint32_t self, bool clear) {
  Run(std::string(clear ? "ClearAllActions();" : "") + "DeleteLocalInt(OBJECT_SELF, \"nwn5e_seek\");", self,
      "an illusion's watcher let go");
  g_reactors.erase(self);
}

// Back where it stood (a look round first, if `look`), out of search mode.
void GoBack(uint32_t self, bool look) {
  std::string code = "ClearAllActions(TRUE);\n";
  if (look) {
    code +=
        "ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_LEFT);\n"
        "ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_RIGHT);\n"
        "ActionWait(2.0);\n";
  }
  code +=
      "ActionMoveToLocation(GetLocalLocation(OBJECT_SELF, \"n5c_mi_home\"));\n"
      "ActionDoCommand(SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, FALSE));\n"
      "ActionDoCommand(DeleteLocalInt(OBJECT_SELF, \"nwn5e_seek\"));\n";
  Run(code, self, "an illusion's watcher going back");
  g_reactors.erase(self);
}

// The image gone, for everyone (Q4 A).
void Vanish(uint32_t image, uint32_t runner) {
  Run("object i = StringToObject(\"" + Hex(image) + "\"); if (GetIsObjectValid(i)) DestroyObject(i);", runner,
      "an illusion vanishing");
  g_images.erase(image);
}

const char* const kComeToLook = R"nss(
ClearAllActions();
SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, TRUE);
ActionMoveToLocation(GetLocalLocation(OBJECT_SELF, "n5c_mi_at"));
)nss";

void ComeToLook(uint32_t self, Reactor& r) {
  r.stage = Stage::kComing;
  Run(std::string(kComeToLook) + Next(kStudyScript, r.serial) + Later(kLateSeconds, kLateScript, r.serial), self,
      "an illusion's watcher coming to look");
}

// Away from the image for 2 rounds (Q7's flight, as a scaring sound's).
const char* const kFleeFrom = R"nss(
ClearAllActions();
SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, FALSE);
ActionMoveAwayFromLocation(GetLocalLocation(OBJECT_SELF, "n5c_mi_at"), TRUE, 30.0);
)nss";

void Flee(uint32_t self, Reactor& r, bool again) {
  r.stage = again ? Stage::kFledAgain : Stage::kFleeing;
  Run(std::string(kFleeFrom) + Later(static_cast<float>(rules::kScareSeconds), kBackScript, r.serial), self,
      "an illusion's watcher fleeing");
}

// Coming to touch it - to take the treasure, to feed on the corpse - walking, not searching.
const char* const kApproach = R"nss(
ClearAllActions();
ActionMoveToLocation(GetLocalLocation(OBJECT_SELF, "n5c_mi_at"));
)nss";

void Approach(uint32_t self, Reactor& r) {
  r.stage = Stage::kTouching;
  Run(std::string(kApproach) + Next(kStudyScript, r.serial) + Later(kLateSeconds, kLateScript, r.serial), self,
      "an illusion's watcher coming to it");
}

const char* const kCharge = R"nss(
ClearAllActions();
ActionMoveToLocation(GetLocalLocation(OBJECT_SELF, "n5c_mi_at"), TRUE);
)nss";

void Charge(uint32_t self, Reactor& r) {
  r.stage = Stage::kCharging;
  Run(std::string(kCharge) + Next(kStrikeScript, r.serial) + Later(kLateSeconds, kLateScript, r.serial), self,
      "an illusion's watcher charging");
}

// At it: its Ethereal off (the attacker must see it to swing; the blow vanishes it) and attacked.
const char* const kStrike = R"nss(
object i = GetLocalObject(OBJECT_SELF, "n5c_mi_image");
effect e = GetFirstEffect(i);
while (GetIsEffectValid(e)) {
  if (GetEffectTag(e) == "n5c_ethereal") RemoveEffect(i, e);
  e = GetNextEffect(i);
}
SetIsTemporaryEnemy(i, OBJECT_SELF);
ClearAllActions();
ActionAttack(i);
)nss";

void Strike(uint32_t self, Reactor& r) {
  r.stage = Stage::kStriking;
  Run(std::string(kStrike) + Later(kBlowSeconds, kLateScript, r.serial), self, "an illusion's watcher striking");
}

// A minute round the image, within the spell's range of it (where its caster stood), in search mode - the hiding
// rules' Perception against the caster's Stealth (stealth_watch.cpp) - then back.
const char* const kSearch = R"nss(
object a = GetArea(OBJECT_SELF);
vector c = GetPositionFromLocation(GetLocalLocation(OBJECT_SELF, "n5c_mi_at"));
ClearAllActions(TRUE);
SetLocalInt(OBJECT_SELF, "nwn5e_seek", 120);
SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, TRUE);
int k;
float f;
float r;
for (k = 0; k < $LEGS; k++) {
  f = IntToFloat(Random(360));
  r = 1.5 + IntToFloat(Random($SPREAD)) / 10.0;
  ActionMoveToLocation(Location(a, Vector(c.x + cos(f) * r, c.y + sin(f) * r, c.z), f));
  ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_LEFT);
  ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_RIGHT);
  ActionWait(3.0);
}
)nss";

void Search(uint32_t self, Reactor& r) {
  r.stage = Stage::kSearching;
  std::string code = kSearch;
  Replace(&code, "$LEGS", "6");
  Replace(&code, "$SPREAD",
          std::to_string(static_cast<int>((rules::kIllusionSearchFeet * kMetresPerFoot - 1.5f) * 10.0f)));
  Run(code + Next(kDoneScript, r.serial) +
          Later(static_cast<float>(rules::kIllusionSearchSeconds), kDoneScript, r.serial),
      self, "an illusion's watcher searching");
}

// It got there (or never did, in kLateSeconds): it studies the image - or, finding it gone, a sentient hostile one
// searches for its caster and any other goes back.
void Study(uint32_t self, Reactor& r, const Check& check) {
  const uint32_t caster = r.caster;
  const std::string name = r.name;
  const std::string image = r.image_name;
  if (!check.image_there) {
    Note(self, r, "finds the image gone");
    if (!rules::SearchesAfterVanish(r.sentient, r.hostile)) return GoBack(self, true);
    Tell(caster, name + " finds the " + image + " gone and searches for you");
    return Search(self, r);
  }
  const int d20 = RollEngineDice(1, 20);
  const bool through = rules::SeesThroughIllusion(d20, r.intelligence, r.dc);
  char check_text[64];
  sprintf_s(check_text, " (Investigation %d vs DC %d)", d20 + r.intelligence, r.dc);
  const std::string line =
      name + " studies the " + image + ": " + (through ? "sees through the illusion" : "is fooled") + check_text;
  if (LogSlot()) {
    Log("cantrips: 0x%08X Minor Illusion's %s: 0x%08X (%s) studies it - d20 %d %+d vs DC %d: %s", caster, image.c_str(),
        self, name.c_str(), d20, r.intelligence, r.dc, through ? "sees through it" : "fooled");
  }
  switch (rules::AfterStudyOf(r.kind, through, r.hostile)) {
    case AfterStudy::kVanishThenSearch:
      Vanish(r.image, self);
      Tell(caster, line + " - the illusion fades; " + name + " searches for you");
      return Search(self, r);
    case AfterStudy::kVanishThenGoBack:
      Vanish(r.image, self);
      Tell(caster, line + " - the illusion fades");
      return GoBack(self, false);
    case AfterStudy::kFleeAgain:
      Tell(caster, line + " and flees");
      return Flee(self, r, true);
    case AfterStudy::kGoBack:
      Tell(caster, line);
      return GoBack(self, true);
  }
}

// At it, it touches it (SRD 5.2.1: "Physical interaction with the image reveals it to be an illusion"): a hand
// through the treasure, nothing to bite in the corpse. The image vanishes (Q4 A); a sentient hostile one then
// searches for its caster, any other goes back. Gone already: as a study finds it.
void Touch(uint32_t self, Reactor& r, const Check& check) {
  if (!check.image_there) return Study(self, r, check);
  const std::string name = r.name;
  const bool treasure = r.kind == ImageKind::kTreasure;
  const std::string line = name + (treasure ? " reaches for the " : " comes to feed on the ") + r.image_name +
                           (treasure ? ": its hand passes through it" : ": it finds nothing to bite") +
                           " - the illusion fades";
  Note(self, r, "touches it - the image vanishes");
  Vanish(r.image, self);
  if (rules::SearchesAfterVanish(r.sentient, r.hostile)) {
    Tell(r.caster, line + "; " + name + " searches for you");
    return Search(self, r);
  }
  Tell(r.caster, line);
  GoBack(self, false);
}

// A blow at the image (or none in kBlowSeconds): it passes through, and the image vanishes; a sentient attacker then
// searches for its caster, any other goes back.
void Struck(uint32_t image, uint32_t attacker) {
  const auto made = g_images.find(image);
  if (made == g_images.end()) return;  // gone already
  const ImageMade kept = made->second;
  Vanish(image, attacker);
  const auto found = g_reactors.find(attacker);
  const bool ours = found != g_reactors.end() && found->second.image == image &&
                    (found->second.stage == Stage::kCharging || found->second.stage == Stage::kStriking);
  const std::string name = ours ? found->second.name : NameOf(attacker);
  std::string line = name + " attacks the " + kept.name + ": the blow passes through it - the illusion fades";
  if (LogSlot()) {
    Log("cantrips: 0x%08X Minor Illusion's %s: 0x%08X (%s) attacks it - the image vanishes", kept.caster,
        kept.name.c_str(), attacker, name.c_str());
  }
  if (ours && found->second.sentient) {
    Tell(kept.caster, line + "; " + name + " searches for you");
    return Search(attacker, found->second);
  }
  Tell(kept.caster, line);
  if (ours) GoBack(attacker, false);
}

// The attack-roll skipper: a blow at an image misses ("things can pass through it"); the image vanishes on the next
// update, out of the engine's attack.
bool BlowAtImage(AttackRoll& roll, uintptr_t /*caller*/) {
  if (g_images.empty() || !roll.attacker || !roll.target || !roll.data) return false;
  const uint32_t image = Field<uint32_t>(roll.target, ee::kObjectId);
  if (g_images.find(image) == g_images.end()) return false;
  roll.data[ee::kAttackD20] = static_cast<uint8_t>(RollEngineDice(1, 20));
  SetField<int32_t>(roll.data, ee::kAttackModifier, 0);
  roll.data[ee::kAttackResult] = ee::kAttackResultMiss;
  for (const auto& blow : g_struck) {
    if (blow.first == image) return true;
  }
  g_struck.emplace_back(image, Field<uint32_t>(roll.attacker, ee::kObjectId));
  return true;
}

void StruckUpdate(void* /*creature*/) {
  if (g_struck.empty()) return;
  std::vector<std::pair<uint32_t, uint32_t>> blows;
  blows.swap(g_struck);
  for (const auto& blow : blows) Struck(blow.first, blow.second);
}

// The handlers: each runs as the reacting creature.
void OnStudyScript(uint32_t self) {
  Reactor* r = ReactorCalled(self);
  if (!r || (r->stage != Stage::kComing && r->stage != Stage::kTouching)) return;
  const Check check = CheckOf(self);
  if (check.fighting) {
    Note(self, *r, "is fighting - no study");
    return LetGo(self, false);
  }
  if (r->stage == Stage::kTouching) return Touch(self, *r, check);
  Study(self, *r, check);
}

void OnStrikeScript(uint32_t self) {
  Reactor* r = ReactorCalled(self);
  if (!r || r->stage != Stage::kCharging) return;
  const Check check = CheckOf(self);
  if (check.fighting) {
    Note(self, *r, "is fighting - no blow at the image");
    return LetGo(self, false);
  }
  if (check.image_there) return Strike(self, *r);
  Note(self, *r, "finds the image gone");
  if (!r->sentient) return GoBack(self, true);
  Tell(r->caster, r->name + " finds the " + r->image_name + " gone and searches for you");
  Search(self, *r);
}

void OnBackScript(uint32_t self) {
  Reactor* r = ReactorCalled(self);
  if (!r || (r->stage != Stage::kFleeing && r->stage != Stage::kFledAgain)) return;
  const Check check = CheckOf(self);
  if (check.fighting) return LetGo(self, false);
  if (r->stage == Stage::kFleeing && r->reaction == ImageReaction::kFleeThenStudy) {
    Note(self, *r, "comes back to look");
    return ComeToLook(self, *r);
  }
  LetGo(self, true);
}

void OnLateScript(uint32_t self) {
  Reactor* r = ReactorCalled(self);
  if (!r) return;
  if (r->stage == Stage::kStriking) {  // no blow landed: it vanishes all the same
    Note(self, *r, "landed no blow in 4 s");
    const uint32_t image = r->image;
    Struck(image, self);
    if (g_reactors.count(self) && g_reactors[self].stage == Stage::kStriking) GoBack(self, false);
    return;
  }
  if (r->stage != Stage::kComing && r->stage != Stage::kCharging && r->stage != Stage::kTouching) return;
  const Check check = CheckOf(self);
  if (check.fighting) return LetGo(self, false);
  if (r->stage == Stage::kTouching) {
    Note(self, *r, "never got there in 30 s - goes back");
    return GoBack(self, true);
  }
  if (r->stage == Stage::kComing) {
    Note(self, *r, "never got there in 30 s - studies it from where it stands");
    return Study(self, *r, check);
  }
  Note(self, *r, "never got there in 30 s - attacks it from where it stands");
  if (check.image_there) return Strike(self, *r);
  GoBack(self, true);
}

void OnDoneScript(uint32_t self) {
  Reactor* r = ReactorCalled(self);
  if (!r || r->stage != Stage::kSearching) return;
  const Check check = CheckOf(self);
  if (check.fighting) return LetGo(self, false);
  Note(self, *r, "ends its search");
  GoBack(self, false);
}

// Each reacting creature's start: where it stood, the image and its point, kept on it.
const char* const kWatcher = R"nss(
object u = StringToObject("$CASTER");
SetLocalInt(OBJECT_SELF, "nwn5e_seek", 120);
SetLocalLocation(OBJECT_SELF, "n5c_mi_home", GetLocation(OBJECT_SELF));
SetLocalLocation(OBJECT_SELF, "n5c_mi_at", GetLocalLocation(u, "n5c_at"));
SetLocalObject(OBJECT_SELF, "n5c_mi_image", GetLocalObject(u, "n5c_image"));
SetLocalString(u, "n5c_name", GetName(OBJECT_SELF));
)nss";

const char* ReactionName(ImageReaction reaction) {
  switch (reaction) {
    case ImageReaction::kStudy: return "comes to study it";
    case ImageReaction::kFlee: return "flees";
    case ImageReaction::kFleeThenStudy: return "flees, then comes back to study it";
    case ImageReaction::kAttack: return "attacks it";
    case ImageReaction::kAttackThenSearch: return "attacks it, then searches";
    case ImageReaction::kTouch: return "comes to touch it";
    default: return "ignores it";
  }
}

void MakeImage(uint32_t self, void* caster, UtilityEffect effect, const std::string& name) {
  const ImageKind kind = rules::ImageKindOf(effect);
  std::string code = kImage;
  Replace(&code, "$CREATURE", rules::ImageIsObject(kind) ? "FALSE" : "TRUE");
  Replace(&code, "$NAME", rules::ImageNameOf(kind));
  Replace(&code, "$BLUEPRINT", rules::ImageBlueprintOf(kind));
  Replace(&code, "$SECONDS", std::to_string(rules::kMinorIllusionSeconds) + ".0");
  Run(code, self, "Minor Illusion's image");
  const std::string image = engine::GetLocalString(caster, "n5c_image_name");
  const int image_size = engine::GetLocalInt(caster, "n5c_image_size");
  unsigned int image_id = 0;
  sscanf_s(engine::GetLocalString(caster, "n5c_image_id").c_str(), "%x", &image_id);
  for (auto it = g_images.begin(); it != g_images.end();) {  // its earlier image ended with this cast
    it = it->second.caster == self ? g_images.erase(it) : std::next(it);
  }
  if (image_id) g_images[image_id] = ImageMade{self, image};
  const int dc = engine::GetLocalInt(caster, "n5c_dc");
  std::vector<std::string> done;
  if (Investigates()) {
    for (const Near& n : NearOf(caster, self, rules::kImageSightFeet * kMetresPerFoot, true)) {
      const bool sentient = rules::IsSentient(n.race, n.intelligence_score);
      const ImageReaction reaction = rules::ReactionToImage(kind, sentient, n.enemy, n.size, image_size);
      if (reaction == ImageReaction::kIgnore) continue;
      std::string start = kWatcher;
      Replace(&start, "$CASTER", Hex(self));
      Run(start, n.id, "an illusion's watcher");
      Reactor r;
      r.caster = self;
      r.image = image_id;
      r.kind = kind;
      r.reaction = reaction;
      r.hostile = n.enemy;
      r.sentient = sentient;
      r.intelligence = n.intelligence;
      r.dc = dc;
      r.serial = ++g_serial;
      r.name = engine::GetLocalString(caster, "n5c_name");
      r.image_name = image;
      Reactor& kept = (g_reactors[n.id] = r);
      if (LogSlot()) {
        Log("cantrips: 0x%08X Minor Illusion's %s: 0x%08X (%s) %s - racial type %d, Intelligence %d (%s), %s, size %d "
            "vs %d",
            self, image.c_str(), n.id, r.name.c_str(), ReactionName(reaction), n.race, n.intelligence_score,
            sentient ? "sentient" : "not sentient", n.enemy ? "hostile" : "not hostile", n.size, image_size);
      }
      switch (reaction) {
        case ImageReaction::kStudy:
          ComeToLook(n.id, kept);
          done.push_back(r.name + " comes to look");
          break;
        case ImageReaction::kFlee:
        case ImageReaction::kFleeThenStudy:
          Flee(n.id, kept, false);
          done.push_back(r.name + " flees");
          break;
        case ImageReaction::kAttack:
        case ImageReaction::kAttackThenSearch:
          Charge(n.id, kept);
          done.push_back(r.name + " attacks it");
          break;
        case ImageReaction::kTouch:
          Approach(n.id, kept);
          done.push_back(r.name + (kind == ImageKind::kTreasure ? " comes for it" : " comes to feed"));
          break;
        default: break;
      }
    }
  }
  TellPlayer(self, "nwn5e: " + name + " - a " + image + " there for a minute" +
                       (done.empty() ? std::string(".") : "; " + Joined(done) + "."));
}

// Dancing Lights: four lights at the point (NWN's 5 m yellow light - Dim Light for the stealth rules - and a glow for
// each orb's placeholder look), on Concentration for 1 minute (rules_concentration.cpp holds its spell's effects:
// these carry the row cast); moved with a Bonus Action; out beyond 120 ft of the caster.
struct Lights {
  std::vector<uint32_t> ids;
  int row = rules::kDancingLights;
  uint64_t ends = 0;  // game ms
  float x = 0.0f, y = 0.0f, z = 0.0f;
};
std::unordered_map<uint32_t, Lights> g_lights;  // by caster

const char* const kLightsAt = R"nss(
object a = GetArea(OBJECT_SELF);
$DESTROY
string ids = ""; int k;
for (k = 0; k < $COUNT; k++) {
  float dx = k % 2 == 0 ? -0.9 : 0.9;
  float dy = k < 2 ? -0.9 : 0.9;
  object o = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", Location(a, Vector($X + dx, $Y + dy, $Z), 0.0), FALSE,
                          "n5c_dlight");
  effect e = EffectLinkEffects(EffectVisualEffect(VFX_DUR_LIGHT_YELLOW_5), EffectVisualEffect(VFX_DUR_GLOW_LIGHT_YELLOW));
  ApplyEffectToObject(DURATION_TYPE_TEMPORARY, SetEffectSpellId(e, $ROW), o, $SECONDS);
  ids += ObjectToString(o) + ";";
}
SetLocalString(OBJECT_SELF, "n5c_dlights", ids);
)nss";

std::string DestroyCode(const std::vector<uint32_t>& ids) {
  std::string code;
  for (const uint32_t id : ids) {
    char line[80];
    sprintf_s(line, "DestroyObject(StringToObject(\"%x\"));", id);
    code += line;
  }
  return code;
}

void LightsAt(uint32_t self, void* caster, Lights* lights, float x, float y, float z) {
  const uint64_t now = GameMs();
  const float seconds = lights->ends > now ? static_cast<float>(lights->ends - now) / 1000.0f : 0.0f;
  std::string code = kLightsAt;
  Replace(&code, "$DESTROY", DestroyCode(lights->ids));
  Replace(&code, "$COUNT", std::to_string(rules::kDancingLightsCount));
  Replace(&code, "$X", Num(x));
  Replace(&code, "$Y", Num(y));
  Replace(&code, "$Z", Num(z));
  Replace(&code, "$ROW", std::to_string(lights->row));
  Replace(&code, "$SECONDS", Num(seconds));
  CreatingFireLight(true);  // the invisible objects normal placeables: the clients get their light
  Run(code, self, "Dancing Lights");
  CreatingFireLight(false);
  lights->ids = ParseIds(engine::GetLocalString(caster, "n5c_dlights"));
  lights->x = x;
  lights->y = y;
  lights->z = z;
}

void EndLights(uint32_t self, const char* why) {
  const auto found = g_lights.find(self);
  if (found == g_lights.end()) return;
  Run(DestroyCode(found->second.ids), self, "Dancing Lights out");
  g_lights.erase(found);
  TellPlayer(self, std::string("nwn5e: Dancing Lights go out - ") + why + ".");
  if (LogSlot()) Log("cantrips: 0x%08X Dancing Lights out - %s", self, why);
}

void PlaceLights(uint32_t self, void* caster, int row) {
  Lights lights;
  const auto old = g_lights.find(self);
  if (old != g_lights.end()) {
    lights.ids = old->second.ids;  // destroyed as the new ones come
    g_lights.erase(old);
  }
  lights.row = row;
  lights.ends = GameMs() + static_cast<uint64_t>(rules::kDancingLightsSeconds) * 1000;
  Run("vector v = GetPositionFromLocation(GetLocalLocation(OBJECT_SELF, \"n5c_at\"));"
      "SetLocalString(OBJECT_SELF, \"n5c_xyz\", FloatToString(v.x, 0, 2) + \" \" + FloatToString(v.y, 0, 2) + \" \" +"
      " FloatToString(v.z, 0, 2));",
      self, "the point");
  float x = 0.0f, y = 0.0f, z = 0.0f;
  if (sscanf_s(engine::GetLocalString(caster, "n5c_xyz").c_str(), "%f %f %f", &x, &y, &z) != 3) return;
  LightsAt(self, caster, &lights, x, y, z);
  g_lights[self] = lights;
  TellPlayer(self, "nwn5e: Dancing Lights - four lights for 1 minute (Concentration); its Move takes them up to 60 feet "
                   "(a Bonus Action).");
  if (LogSlot()) Log("cantrips: 0x%08X Dancing Lights (row %d): %zu lights at %.1f, %.1f", self, row, lights.ids.size(), x, y);
}

float Feet(float dx, float dy) { return std::sqrt(dx * dx + dy * dy) / kMetresPerFoot; }

// The Move: a Bonus Action (bonus_actions.h), up to 60 feet and within 120 of the caster; no cast.
bool DancingLightsMoveGuard(void* creature, const SpellOrder& order) {
  if (!creature || order.fake || static_cast<int>(order.spell) != rules::kDancingLightsMove) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto found = g_lights.find(id);
  if (found == g_lights.end()) {
    TellPlayer(id, "nwn5e: Dancing Lights - no lights of yours to move: cast its Lights first.");
    return true;
  }
  float x = order.x, y = order.y, z = order.z;
  if (order.target != ee::kObjectInvalid && order.target != id) {
    if (void* object = GameObjectById(order.target)) {
      x = Field<float>(object, ee::kObjectPositionX);
      y = Field<float>(object, ee::kObjectPositionY);
      z = Field<float>(object, ee::kObjectPositionY + 4);
    }
  }
  Lights& lights = found->second;
  const float from_lights = Feet(x - lights.x, y - lights.y);
  const float from_caster =
      Feet(x - Field<float>(creature, ee::kObjectPositionX), y - Field<float>(creature, ee::kObjectPositionY));
  if (!rules::DancingLightsMoveAllowed(from_lights, from_caster)) {
    TellPlayer(id, "nwn5e: Dancing Lights - they move up to 60 feet, and stay within 120 feet of you.");
    return true;
  }
  if (g_config.actions == rules::RuleVariant::kFaithful && !SpendBonusAction(creature, "Dancing Lights: Move")) return true;
  LightsAt(id, creature, &lights, x, y, z);
  TellPlayer(id, "nwn5e: Dancing Lights - they move.");
  if (LogSlot()) Log("cantrips: 0x%08X Dancing Lights moved %.0f ft (%.0f ft from the caster)", id, from_lights, from_caster);
  return true;
}

bool HasEffectOfSpellOn(uint32_t object_id, int spell) {
  void* object = GameObjectById(object_id);
  if (!object) return false;
  void* const* list = Field<void* const*>(object, ee::kObjectEffects);
  const int count = Field<int32_t>(object, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return false;
  for (int i = 0; i < count; ++i) {
    if (list[i] && Field<int32_t>(list[i], ee::kEffectSpellId) == spell) return true;
  }
  return false;
}

// Their minute's end, Concentration ending (its effects gone from them), or 120 feet away: out.
void LightsUpdate(void* creature) {
  if (g_lights.empty() || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto found = g_lights.find(id);
  if (found == g_lights.end()) return;
  const Lights& lights = found->second;
  if (GameMs() >= lights.ends) return EndLights(id, "their minute is over");
  if (lights.ids.empty() || !HasEffectOfSpellOn(lights.ids.front(), lights.row)) {
    return EndLights(id, "your Concentration ended");
  }
  const float feet = Feet(Field<float>(creature, ee::kObjectPositionX) - lights.x,
                          Field<float>(creature, ee::kObjectPositionY) - lights.y);
  if (feet > static_cast<float>(rules::kDancingLightsRangeFeet)) EndLights(id, "more than 120 feet from you");
}

// Mage Hand at a placeable that's no light: a container opens (unlocked) as if you stood by it - CNWSPlaceable::
// OpenInventory, the window closing once you move as ever; a usable object is used - its OnUsed run with you as the
// user (GetLastUsedBy: CNWSPlaceable +0x528).
const char* const kHandAt = R"nss(
int r = -1;
if (GetObjectType(OBJECT_SELF) == OBJECT_TYPE_PLACEABLE) {
  if (GetHasInventory(OBJECT_SELF)) r = GetLocked(OBJECT_SELF) ? 3 : 4;
  else if (GetUseableFlag(OBJECT_SELF)) r = 5;
}
SetLocalInt(OBJECT_SELF, "n5c_hand", r + 10);
SetLocalString(OBJECT_SELF, "n5c_name", GetName(OBJECT_SELF));
)nss";
constexpr size_t kPlaceableLastUsedBy = 0x528;  // ExecuteCommandGetLastUsedBy (+0x61)

using GetGameObjectFn = void* (*)(void* app, uint32_t id);
using CastFn = void* (*)(void* object);
using OpenInventoryFn = void (*)(void* placeable, uint32_t opener);

void* PlaceableById(uint32_t id) {
  static const auto get = reinterpret_cast<GetGameObjectFn>(EngineExport(ee::kGetGameObject));
  static void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!get || !app_manager || !*app_manager) return nullptr;
  void* app = Field<void*>(*app_manager, ee::kAppManagerServerApp);
  void* object = app ? get(app, id) : nullptr;
  if (!object) return nullptr;
  const auto cast = reinterpret_cast<CastFn>(reinterpret_cast<void* const*>(Field<void*>(object, 0))[ee::kVtableAsPlaceable / 8]);
  return cast ? cast(object) : nullptr;
}

int HandAt(uint32_t self, uint32_t id, std::string* name) {
  void* object = GameObjectById(id);
  if (!object) return -1;
  engine::SetLocalInt(object, "n5c_hand", 0);
  Run(kHandAt, id, "Mage Hand at an object");
  const int stored = engine::GetLocalInt(object, "n5c_hand");
  const int r = stored ? stored - 10 : -1;
  if (name) *name = engine::GetLocalString(object, "n5c_name");
  void* placeable = r >= 4 ? PlaceableById(id) : nullptr;
  if (r == 4) {
    static const auto open = reinterpret_cast<OpenInventoryFn>(EngineExport("?OpenInventory@CNWSPlaceable@@QEAAXI@Z"));
    if (!placeable || !open) return -1;
    open(placeable, self);
  }
  if (r == 5) {
    if (!placeable) return -1;
    SetField<uint32_t>(placeable, kPlaceableLastUsedBy, self);
    Run("string s = GetEventScript(OBJECT_SELF, EVENT_SCRIPT_PLACEABLE_ON_USED); if (s != \"\") ExecuteScript(s, OBJECT_SELF);",
        id, "Mage Hand using an object");
  }
  return r;
}

// Q1 A: Beckon Water at the ground leaves a puddle for 1 minute, Beckon Earth a dust patch for 10 - looks for now
// (vfx_persistent rows 103-104: Grease's slick; a dust puff a round, kDustHeartbeat), #142 their rules.
constexpr const char* kDustHeartbeat = "n5c_dust_hb";

void PlacePatch(uint32_t self, UtilityEffect effect) {
  char code[300];
  sprintf_s(code,
            "ApplyEffectAtLocation(DURATION_TYPE_TEMPORARY, EffectAreaOfEffect(%d, \"\", \"%s\", \"\"), "
            "GetLocalLocation(OBJECT_SELF, \"n5c_at\"), %d.0);",
            rules::PatchAreaEffectOf(effect), effect == UtilityEffect::kBeckonEarth ? kDustHeartbeat : "",
            rules::PatchSecondsOf(effect));
  Run(code, self, "a patch");
  if (LogSlot()) Log("cantrips: 0x%08X a patch (area effect %d) for %d s", self, rules::PatchAreaEffectOf(effect),
                     rules::PatchSecondsOf(effect));
}

void DustHeartbeat(uint32_t self) {
  Run("ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_DUST_EXPLOSION), GetLocation(OBJECT_SELF));",
      self, "the dust patch");
}


void UtilityScript(uint32_t self) {
  void* caster = CreatureById(self);
  if (!caster) return;
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const UtilityEffect effect = rules::UtilityEffectOf(row);
  if (effect == UtilityEffect::kNone) return;
  const std::string name = SpellNameOf(row);
  std::string code = kTargets;
  char metres[32];
  sprintf_s(metres, "%.2f", rules::kElementCubeFeet * kMetresPerFoot);
  Replace(&code, "$CUBE", rules::FillsElementCube(effect) ? "TRUE" : "FALSE");
  Replace(&code, "$METRES", metres);
  Replace(&code, "$LOOK", LookOf(effect));
  Replace(&code, "$ICON", std::to_string(rules::kBoomingVoiceIcon));
  Replace(&code, "$SECONDS", std::to_string(rules::kBoomingVoiceSeconds) + ".0");
  Run(code, self, "n5_c_utility");
  if (effect == UtilityEffect::kBoomingVoice) {
    g_booming[self] = GameMs() + static_cast<uint64_t>(rules::kBoomingVoiceSeconds) * 1000;
    Log("cantrips: 0x%08X Booming Voice - Advantage on Intimidation checks for 1 minute", self);
    return;
  }
  if (effect == UtilityEffect::kMagicMark) {
    PlaceMark(self, caster);
    return;
  }
  if (IsMinorIllusion(row)) EndImage(self);  // a new cast ends the earlier illusion
  if (rules::SoundPlayOf(effect) != SoundPlay::kNone) {
    PlaySoundAt(self, caster, effect, name);
    return;
  }
  if (rules::ImageKindOf(effect) != ImageKind::kNone) {
    MakeImage(self, caster, effect, name);
    return;
  }
  if (effect == UtilityEffect::kDancingLights) {
    PlaceLights(self, caster, row);
    return;
  }
  if (effect == UtilityEffect::kDancingLightsMove) {  // the order guard moves them; a script's cast can't
    TellPlayer(self, "nwn5e: Dancing Lights - Move them from its menu (a Bonus Action).");
    return;
  }
  if (rules::PatchAreaEffectOf(effect) >= 0 && !engine::GetLocalInt(caster, "n5c_at_object")) PlacePatch(self, effect);
  const FirePlay fire = rules::FirePlayOf(effect);
  const DoorPlay door = rules::DoorPlayOf(effect);
  if (fire == FirePlay::kNone && door == DoorPlay::kNone) {
    if (LogSlot()) Log("cantrips: 0x%08X %s (row %d)", self, name.c_str(), row);
    return;
  }
  std::vector<std::string> done;
  for (const uint32_t id : ParseIds(engine::GetLocalString(caster, "n5c_targets"))) {
    std::string what;
    if (door != DoorPlay::kNone) {
      const int r = DoorAt(id, door, &what);
      if (r == 1) done.push_back(what + " flies open");
      if (r == 2) done.push_back(what + " slams shut");
      if (r == 3) done.push_back(what + " is locked");
      if (r >= 0) continue;  // a door
    }
    if (fire == FirePlay::kNone) continue;
    const int r = FirePlayAt(self, id, fire, rules::kFirePlayDimSeconds);
    void* object = GameObjectById(id);
    what = object ? engine::GetLocalString(object, "n5c_name") : std::string("it");
    if (r < 0 && effect == UtilityEffect::kMageHand) {  // build 4b: a lever or a container
      const int used = HandAt(self, id, &what);
      if (used == 3) done.push_back(what + " is locked");
      if (used == 4) done.push_back(what + " opens");
      if (used == 5) done.push_back("the hand works " + what);
      continue;
    }
    if (r == 1) done.push_back(what + " is lit");
    if (r == 2) done.push_back(what + (fire == FirePlay::kDim ? " dims out for a minute" : " goes out"));
    if (r == 0 && fire != FirePlay::kLight && fire != FirePlay::kSnuff) done.push_back(what + " is out already");
  }
  std::string line = "nwn5e: " + name + " - ";
  if (done.empty()) {
    line += NothingLine(effect);
  } else {
    for (size_t i = 0; i < done.size(); ++i) line += (i == 0 ? "" : i + 1 == done.size() ? " and " : ", ") + done[i];
  }
  TellPlayer(self, line + ".");
  if (LogSlot()) Log("cantrips: 0x%08X %s (row %d): %s", self, name.c_str(), row, line.c_str() + 7);
}

}  // namespace

// cantrip_effects.h: Booming Voice's Advantage on Intimidation checks.
bool BoomingVoiceAdvantage(void* creature, int nwn_skill) {
  if (g_booming.empty() || !creature) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto found = g_booming.find(id);
  if (found == g_booming.end()) return false;
  if (GameMs() >= found->second) {
    g_booming.erase(found);
    return false;
  }
  const std::optional<rules::Skill2024> skill = rules::Skill2024ForNwnSkill(nwn_skill);
  if (!skill || *skill != rules::Skill2024::kIntimidation) return false;
  if (g_once.First(LogOnce::Key(id, nwn_skill, static_cast<int>(found->second / 1000)))) {
    Log("cantrips: 0x%08X Booming Voice: Advantage on the Intimidation check", id);
  }
  return true;
}

void InstallUtilityCantrips() {
  const bool scripts = AddScriptHandler("n5_c_utility", &UtilityScript) &&
                       AddScriptHandler(kDustHeartbeat, &DustHeartbeat);
  const bool hooks = AddSpellOrderGuard(&DancingLightsMoveGuard) && AddCreatureUpdateListener(&LightsUpdate);
  Log("cantrips: build 4a - Druidcraft, Elementalism, Prestidigitation, Thaumaturgy, Mage Hand (Fire Play, the doors, "
      "Booming Voice, Magic Mark) (#39 Part 1)%s",
      scripts ? "" : " - SOME hooks missing");
  Log("cantrips: build 4b - Minor Illusion, Phantom Sound, Animal Sound (sounds that draw or scare), the images, "
      "Dancing Lights (its Move a Bonus Action), Mage Hand at levers and containers, the patches (#39 Part 1)%s",
      scripts && hooks ? "" : " - SOME hooks missing");
  const bool images = AddScriptHandler(kStudyScript, &OnStudyScript) && AddScriptHandler(kStrikeScript, &OnStrikeScript) &&
                      AddScriptHandler(kBackScript, &OnBackScript) && AddScriptHandler(kLateScript, &OnLateScript) &&
                      AddScriptHandler(kDoneScript, &OnDoneScript) && AddAttackRollSkipper(&BlowAtImage) &&
                      AddCreatureUpdateListener(&StruckUpdate);
  Log("cantrips: Minor Illusion's image - those that see it study it, flee it or attack it, by what they are; seen "
      "through or hit, it vanishes and they search for its caster (#152)%s",
      images ? "" : " - SOME hooks missing");
}
