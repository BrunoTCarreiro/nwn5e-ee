// The stealth marker (owner, 2026-10-25: "how possible is it to show enemys'
// sight ranges?" - then "build your recommendation"): while a player character
// is in stealth mode, an eye over each hostile creature within 40 m that sees it,
// an ear over one that only hears it - the game's own Spot and Listen icons by
// name, in nwn5e's models (tools/data/stealth_marker.py: visualeffects 1571
// seen, 1572 heard, on the head). It reads the engine's own perception
// (GetObjectSeen / GetObjectHeard), so whatever decides it shows: the 2024
// Stealth against passive Perception (rules_perception.cpp), line of sight,
// EE's night terms (the ruleset's SPOT_NO_LIGHT_MODIFIER / HIDING_LIGHT_MODIFIER:
// at night a watcher without a light or darkvision spots at -5, a hider
// carrying a light at +10) and the area's own Spot modifier.
// Checked once a second while hiding; the markers go when it stops hiding, and
// an area's leftovers (a load, leaving while hidden) when it is entered. Not
// sight ranges drawn on the ground (rings): next, if wanted. `stealth_marker`;
// both rule modes (it shows, it decides nothing).

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "log.h"

namespace {

constexpr int kSeenRow = 1571;   // NWN5E_STEALTH_SEEN: an eye
constexpr int kHeardRow = 1572;  // NWN5E_STEALTH_HEARD: an ear
constexpr const char* kTag = "nwn5e_noticed";
constexpr uint64_t kCheckMs = 1000;
constexpr float kRange = 40.0f;  // farther than the longest sight (ranges.2da: 35 m)

struct Watch {
  bool marking = false;  // markers may be up in the area
  void* area = nullptr;
  uint64_t next = 0;
};
std::unordered_map<uint32_t, Watch> g_watches;  // by player character, the engine's thread only

// A script chunk on the character; a failure is logged (once: it would repeat each second).
void Run(const char* code, uint32_t pc, const char* what) {
  static bool told = false;
  const int result = RunScriptChunk(code, pc);
  if (result != 0 && !told) {
    told = true;
    Log("stealth marker: the %s script failed (%d) - no markers", what, result);
  }
}

// The hostile creatures near the hiding character: an eye or an ear, or none.
void Mark(uint32_t pc) {
  char code[1400];
  sprintf_s(code,
            "object pc = OBJECT_SELF; location at = GetLocation(pc);"
            "object o = GetFirstObjectInShape(SHAPE_SPHERE, %.1f, at, FALSE, OBJECT_TYPE_CREATURE);"
            "while (GetIsObjectValid(o)) {"
            " if (o != pc) {"
            "  int want = 0;"
            "  if (!GetIsDead(o) && GetIsEnemy(pc, o) && GetDistanceBetween(pc, o) <= %.1f)"
            "   want = GetObjectSeen(pc, o) ? 2 : (GetObjectHeard(pc, o) ? 1 : 0);"
            "  if (want != GetLocalInt(o, \"%s\")) {"
            "   effect e = GetFirstEffect(o);"
            "   while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"%s\") RemoveEffect(o, e); e = GetNextEffect(o); }"
            "   if (want) ApplyEffectToObject(DURATION_TYPE_PERMANENT,"
            "    TagEffect(SupernaturalEffect(EffectVisualEffect(want == 2 ? %d : %d)), \"%s\"), o);"
            "   SetLocalInt(o, \"%s\", want);"
            "  }"
            " }"
            " o = GetNextObjectInShape(SHAPE_SPHERE, %.1f, at, FALSE, OBJECT_TYPE_CREATURE);"
            "}",
            kRange + 5.0f, kRange, kTag, kTag, kSeenRow, kHeardRow, kTag, kTag, kRange + 5.0f);
  Run(code, pc, "marking");
}

// Every marker in the character's area off.
void Clear(uint32_t pc) {
  char code[700];
  sprintf_s(code,
            "object area = GetArea(OBJECT_SELF); object o = GetFirstObjectInArea(area, OBJECT_TYPE_CREATURE);"
            "while (GetIsObjectValid(o)) {"
            " if (GetLocalInt(o, \"%s\")) {"
            "  effect e = GetFirstEffect(o);"
            "  while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"%s\") RemoveEffect(o, e); e = GetNextEffect(o); }"
            "  DeleteLocalInt(o, \"%s\");"
            " }"
            " o = GetNextObjectInArea(area, OBJECT_TYPE_CREATURE);"
            "}",
            kTag, kTag, kTag);
  Run(code, pc, "clearing");
}

void Update(void* creature) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  Watch& watch = g_watches[id];
  const uint64_t now = GetTickCount64();
  if (now < watch.next) return;
  watch.next = now + kCheckMs;
  void* area = engine::AreaOf(creature);
  if (!area) return;
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  if (area != watch.area) {
    // A new area (or a loaded game): markers left in it from before go.
    watch.area = area;
    Clear(id);
    watch.marking = false;
  }
  if (hiding) {
    if (!watch.marking) Log("stealth marker: 0x%08X hides - marking who notices it", id);
    Mark(id);
    watch.marking = true;
  } else if (watch.marking) {
    Clear(id);
    watch.marking = false;
    Log("stealth marker: 0x%08X stopped hiding - markers off", id);
  }
}

}  // namespace

void InstallStealthMarker() {
  if (!g_config.stealth_marker) return;
  if (AddCreatureUpdateListener(&Update)) {
    Log("stealth marker: a hiding player character sees who notices it - an eye (seen), an ear (heard) (stealth_marker)");
  }
}
