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
// - Heard, not seen (0.27.0; owner: "for the hearing detection, do option 2"):
//   2024 rules (perception=faithful) - finding a hider by hearing is finding it,
//   so a hostile that hears it but doesn't see it investigates: it walks to where
//   the sound came from in search mode (detect: a rolled Perception, SK5), looks
//   about, waits, walks back. Not one in combat, in conversation, plot, someone's
//   associate or not commandable; again after 25 s at most. NWN's own AI never
//   acts on hearing ("DO NOT RELY ON HEARING FOR ENEMY DETECTION",
//   nw_c2_default2) beyond a head scratch within 7 m. `heard_investigates`.
// All from the engine's own perception (GetObjectSeen / GetObjectHeard), so what
// decides it shows: the 2024 Stealth against passive Perception, line of sight,
// EE's night light terms (at night a watcher without a light or darkvision
// visual spots at -5, a hider carrying a light at +10) and the area's Spot
// modifier. Markers and rings go when it stops hiding, and an area's leftovers
// when it is entered.

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

namespace {

constexpr int kSeenRow = 1571;   // NWN5E_STEALTH_SEEN: an eye
constexpr int kHeardRow = 1572;  // NWN5E_STEALTH_HEARD: an ear
constexpr int kSightRingRow = 1573;
constexpr int kHearRingRow = 1583;
constexpr float kRingRadii[] = {5, 10, 15, 20, 25, 28, 30, 35, 40, 50};  // tools/data/stealth_rings.py
constexpr size_t kSightRange = 0x7b0;  // float, metres; +4 in combat (DoSpotDetection)
constexpr size_t kHearRange = 0x7b8;   // float, metres; +4 in combat (DoListenDetection)
constexpr uint64_t kCheckMs = 1000;
constexpr const char* kVarSightRing = "nwn5e_sight_ring";
constexpr const char* kVarHearRing = "nwn5e_hear_ring";

struct Watch {
  bool on = false;  // markers or rings may be up in the area
  void* area = nullptr;
  uint64_t next = 0;
};
std::unordered_map<uint32_t, Watch> g_watches;  // by player character
std::unordered_map<uint32_t, int> g_rings;      // by creature: the ring rows written on it (sight * 1000 + hearing)
bool g_investigate = false;

int RingRow(int first, float range) {
  if (!(range > 0.0f)) return 0;
  int best = 0;
  for (int i = 1; i < static_cast<int>(sizeof(kRingRadii) / sizeof(kRingRadii[0])); ++i) {
    if (fabsf(kRingRadii[i] - range) < fabsf(kRingRadii[best] - range)) best = i;
  }
  return first + best;
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

// The pass, on the hiding character.
void Pass(uint32_t pc) {
  const int rings = g_config.stealth_rings;
  std::string code =
      "object pc = OBJECT_SELF; location at = GetLocation(pc);"
      "int pass = GetLocalInt(pc, \"nwn5e_watch_pass\") + 1; SetLocalInt(pc, \"nwn5e_watch_pass\", pass);"
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
        "  int ring = 0;"
        "  if (GetLocalInt(o, \"nwn5e_ring_pass\") == pass && sight > 0 && hear > 0) ring = sight * 1000 + hear;"
        "  if (ring != GetLocalInt(o, \"nwn5e_ring\")) {"
        "   effect e = GetFirstEffect(o);"
        "   while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"nwn5e_ring\") RemoveEffect(o, e); e = GetNextEffect(o); }"
        "   if (ring) {"
        "    ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect(sight)), \"nwn5e_ring\"), o);"
        "    ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectVisualEffect(hear)), \"nwn5e_ring\"), o);"
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
        "   AssignCommand(o, ActionWait(4.0));"
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
  Run("object area = GetArea(OBJECT_SELF); object o = GetFirstObjectInArea(area, OBJECT_TYPE_CREATURE);"
      "while (GetIsObjectValid(o)) {"
      " if (GetLocalInt(o, \"nwn5e_noticed\") || GetLocalInt(o, \"nwn5e_ring\")) {"
      "  effect e = GetFirstEffect(o);"
      "  while (GetIsEffectValid(e)) {"
      "   string t = GetEffectTag(e); if (t == \"nwn5e_noticed\" || t == \"nwn5e_ring\") RemoveEffect(o, e);"
      "   e = GetNextEffect(o);"
      "  }"
      "  DeleteLocalInt(o, \"nwn5e_noticed\"); DeleteLocalInt(o, \"nwn5e_ring\");"
      " }"
      " o = GetNextObjectInArea(area, OBJECT_TYPE_CREATURE);"
      "}",
      pc, "clearing");
}

// Every creature, each AI pass: its ring rows as locals, written when they change.
void WriteRanges(void* creature, uint32_t id) {
  const bool fighting = Field<int32_t>(creature, ee::kCreatureInCombat) != 0;
  const float sight = Field<float>(creature, kSightRange + (fighting ? 4 : 0));
  const float hear = Field<float>(creature, kHearRange + (fighting ? 4 : 0));
  const int sight_row = RingRow(kSightRingRow, sight), hear_row = RingRow(kHearRingRow, hear);
  const int key = sight_row * 1000 + hear_row;
  auto it = g_rings.find(id);
  if (it != g_rings.end() && it->second == key) return;
  g_rings[id] = key;
  engine::SetLocalInt(creature, kVarSightRing, sight_row);
  engine::SetLocalInt(creature, kVarHearRing, hear_row);
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
  if (Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  Watch& watch = g_watches[id];
  const uint64_t now = GetTickCount64();
  if (now < watch.next) return;
  watch.next = now + kCheckMs;
  void* area = engine::AreaOf(creature);
  if (!area) return;
  const bool hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  if (area != watch.area) {
    // A new area (or a loaded game): markers and rings left in it from before go.
    watch.area = area;
    Clear(id);
    watch.on = false;
  }
  if (hiding) {
    if (!watch.on) Log("stealth watch: 0x%08X hides - watching who notices it", id);
    Pass(id);
    watch.on = true;
  } else if (watch.on) {
    Clear(id);
    watch.on = false;
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
