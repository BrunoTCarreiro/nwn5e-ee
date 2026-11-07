// #39 Part 1, build 4a: the utility cantrips - Fire Play and the doors (docs/PLAN.md "Phase 4"; owner, 2026-11-07:
// "1A, 2A, 3A, 4 not too about the uses but A, 5 let's look into these deeper, 6 A ...", Q5's closer look, Q8 A, then
// "start building the utility cantrips"), `spellcasting` faithful. One impact script, n5_c_utility, for Druidcraft,
// Elementalism, Prestidigitation and Thaumaturgy (each a radial menu of its effects: rules::UtilityEffectOf) and Mage
// Hand:
// - The target: the object it was cast at, or a point; Elementalism fills a 5-foot Cube (every light and door within
//   1.5 m of the point). Ranges 30 ft, Prestidigitation's 10 ft (rules::CantripRangeFeet; rules_cantrips.cpp's
//   ComputeSpellRange hook).
// - Lights (light_sources.cpp's FirePlayAt - as a click, a hit or the Light spell: the deactivate animation, the
//   area's lighting, the stealth light rule, the guards who see one go out): Fire Play (Druidcraft's,
//   Prestidigitation's) and Mage Hand light or snuff one; Beckon Fire's embers light those in the Cube, Beckon Water's
//   mist snuffs them; Thaumaturgy's Fire Play dims one out for 1 minute (Q8 A), then it burns again.
// - Doors: Invisible Hand and Mage Hand open an unlocked shut door or shut an open one; Beckon Air's breeze shuts the
//   open ones in the Cube.
// - Booming Voice: Advantage on Intimidation checks for 1 minute (cantrip_effects.h: rules_skill_dcs.cpp's
//   GetIsSkillSuccessful d20, !skill), its icon on the effect bar.
// - Magic Mark: a blue glow at the point (an invisible placeable) for an hour of play, three at a time (the oldest
//   goes).
// - The rest a visual and a line: Weather Sensor (the area's weather now - NWN keeps no forecast; indoors or
//   underground, no sky), Bloom, Sensory Effect, Beckon Earth.
// Build 4b: Dancing Lights, Minor Illusion, Thaumaturgy's Phantom Sound (row 1121), Druidcraft's animal sound, Mage
// Hand's levers and containers. NWN mode: none (they aren't NWN's spells).

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cantrip_effects.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "light_sources.h"
#include "log.h"
#include "log_once.h"
#include "script_hooks.h"
#include "spell_impact.h"
#include "rules/cantrips.h"
#include "rules/skills.h"

namespace {

using rules::DoorPlay;
using rules::FirePlay;
using rules::UtilityEffect;

constexpr float kMetresPerFoot = 0.3048f;
constexpr float kMarkSeconds = 3600.0f;  // Magic Mark: an hour of play

int g_lines = 0;
bool LogSlot() {
  if (g_lines >= 400) return false;
  ++g_lines;
  return true;
}
LogOnce g_once(500);

std::unordered_map<uint32_t, uint64_t> g_booming;            // Booming Voice: until (game ms), by caster
std::unordered_map<uint32_t, std::deque<uint32_t>> g_marks;  // Magic Mark: the marks' ids, oldest first

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
$LOOK
)nss";

// Each effect's visual and line (the lights' and doors' lines come from what they did).
std::string LookOf(UtilityEffect effect) {
  switch (effect) {
    case UtilityEffect::kWeatherSensor:
      return "object a = GetArea(OBJECT_SELF); string w;"
             "if (GetIsAreaInterior(a) || !GetIsAreaAboveGround(a)) w = \"no sky here (indoors or underground)\";"
             "else { int k = GetWeather(a);"
             " if (k == WEATHER_RAIN) { w = \"rain\";"
             "  ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_WATER), OBJECT_SELF); }"
             " else if (k == WEATHER_SNOW) { w = \"snow\";"
             "  ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_FROST_S), OBJECT_SELF); }"
             " else { w = \"clear skies\";"
             "  ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect(VFX_DUR_GLOW_LIGHT_YELLOW), OBJECT_SELF, "
             "6.0); } }"
             "SendMessageToPC(OBJECT_SELF, \"nwn5e: Druidcraft - Weather Sensor: \" + w + "
             "\" (the weather now: NWN keeps no forecast).\");";
    case UtilityEffect::kBloom:
      return "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_NATURE), l);"
             "SendMessageToPC(OBJECT_SELF, \"nwn5e: Druidcraft - Bloom: a flower blossoms there.\");";
    case UtilityEffect::kSensoryEffect:
      return "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(VFX_IMP_PULSE_WIND), l);"
             "SendMessageToPC(OBJECT_SELF, \"nwn5e: Druidcraft - Sensory Effect: leaves fall on a gentle breeze.\");";
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

// Magic Mark: a blue glow at the point for an hour of play; the caster's fourth takes its oldest away.
const char* const kMark = R"nss(
location l = GetLocalLocation(OBJECT_SELF, "n5c_at");
object m = CreateObject(OBJECT_TYPE_PLACEABLE, "plc_invisobj", l, FALSE, "n5c_mark");
ApplyEffectToObject(DURATION_TYPE_PERMANENT, EffectVisualEffect(VFX_DUR_GLOW_LIGHT_BLUE), m);
DestroyObject(m, $SECONDS);
SetLocalString(OBJECT_SELF, "n5c_mark", ObjectToString(m));
SendMessageToPC(OBJECT_SELF, "nwn5e: Prestidigitation - Magic Mark: a glowing mark there for an hour (three at a time).");
)nss";

void PlaceMark(uint32_t self, void* caster) {
  std::string code = kMark;
  char seconds[32];
  sprintf_s(seconds, "%.1f", kMarkSeconds);
  Replace(&code, "$SECONDS", seconds);
  CreatingFireLight(true);  // light_sources.cpp: the invisible object a normal placeable, so the client gets its glow
  Run(code, self, "Magic Mark");
  CreatingFireLight(false);
  const std::string mark = engine::GetLocalString(caster, "n5c_mark");
  if (mark.empty()) return;
  std::deque<uint32_t>& marks = g_marks[self];
  marks.push_back(static_cast<uint32_t>(strtoul(mark.c_str(), nullptr, 16)));
  while (marks.size() > static_cast<size_t>(rules::kMagicMarksAtOnce)) {
    char destroy[96];
    sprintf_s(destroy, "DestroyObject(StringToObject(\"%x\"));", marks.front());
    Run(destroy, self, "Magic Mark's oldest");
    marks.pop_front();
  }
  if (LogSlot()) Log("cantrips: 0x%08X Magic Mark %s (%zu up)", self, mark.c_str(), marks.size());
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
    case UtilityEffect::kMageHand: return "nothing there the hand can work - a door or a light";
    default: return "nothing happens";
  }
}

void UtilityScript(uint32_t self) {
  void* caster = CreatureById(self);
  if (!caster) return;
  const int row = Field<int32_t>(caster, ee::kObjectLastSpellId);
  const UtilityEffect effect = rules::UtilityEffectOf(row);
  if (effect == UtilityEffect::kNone) return;
  const std::string name = SpellNameOf(row);
  if (effect == UtilityEffect::kPhantomSound) {
    TellPlayer(self, "nwn5e: " + name + " - not here yet: it comes with the next build.");
    return;
  }
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
  const bool scripts = AddScriptHandler("n5_c_utility", &UtilityScript);
  Log("cantrips: build 4a - Druidcraft, Elementalism, Prestidigitation, Thaumaturgy, Mage Hand (Fire Play, the doors, "
      "Booming Voice, Magic Mark) (#39 Part 1)%s",
      scripts ? "" : " - SOME hooks missing");
}
