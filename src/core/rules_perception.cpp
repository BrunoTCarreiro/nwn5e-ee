// SK5 (port of 1.69's rules_perception.cpp; docs/EE-PORT.md E3d), `perception`
// faithful: Stealth against Perception the 2024 way. EE rolls a hider's Hide
// / Move Silently d20 and every creature's Spot / Listen die each 6 s
// (DoStealthDetection) and compares them in DoListenDetection / DoSpotDetection.
// Faithful:
// - the hider rolls Stealth once, when it starts hiding (SetStealthMode) —
//   one d20 for Hide and Move Silently, both Stealth (SK1) — and keeps it until
//   it stops hiding (the 6 s re-rolls answer the kept roll); with
//   hiding_needs_15 a total below 15 is no hide (A);
// - an observer uses passive Perception (10 + its modifier; 5 with
//   disadvantage) unless it searches (detect mode, or in combat with
//   alert_creatures_search: one d20 for Spot and Listen, both Perception);
//   NWN gave an observer that isn't searching half its skill — the checks are
//   told it searches, so the skill is whole;
// - no size modifier (the term the engine adds to the observer's score is
//   added to the hider's skill too), no -10 while in combat, no Favored Enemy
//   bonus, and a Stealth total of 0 no longer means "can't hide";
// - found_up_close (B): an observer whose Spot check got as far as its score
//   finds a hider in front of it within 2 m with a clear line between them;
// - hiders_need_search (D; owner, 2026-10-25: "hiding should succeed by
//   default, enemy investigation rolls should test it"): an observer that isn't
//   searching can't pick a hider out by sight - it can hear one, and then comes
//   to look (stealth_watch.cpp), searching: its Perception rolls (a d20) are
//   what find the hider. A searcher's roll against a hiding player character is
//   told to it (once, and again when the result changes). With it, hiding needs
//   no 15 (hiding_needs_15 is off by default);
// - vision cones (E; owner, 2026-10-25: "Go with option 2"): a calm observer
//   does see a hider inside its vision cone - vision_cone degrees centred on its
//   facing (120), rules::WithinVisionCone - its passive Perception against the
//   Stealth total (EE's own terms still apply: -5 when the hider is 135 degrees
//   or more off a computer observer's facing, -5 for a still hider, +5 for a
//   still observer); outside the cone, not at all. Searching observers see all
//   round. A calm look that sees or misses a hiding player character is told
//   to it too. vision_cone=0 is D as it was (calm observers see no hider);
// - found ends hiding (F; owner, 2026-10-25: "once discovered the PC leaves
//   stealth mode"): a hostile that sees a hiding player character - a search,
//   a look inside its cone, or up close - takes it out of stealth mode (on the
//   next script tick, not inside the engine's detection), told why; its
//   markers, rings and cones go with it (stealth_watch.cpp). found_ends_hiding.
// Distance, area, movement, facing and light terms stay (NWN's real-time
// stealth needs them). A deafened observer can't hear and a blinded one can't
// see: EE's own checks already fail them (SK11). NWN mode: NWN's.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <intrin.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "rules/skills.h"
#include "skill_rank.h"

namespace {

using SetStealthModeFn = void (*)(void* creature, uint8_t mode);
using StealthDetectionFn = int (*)(void* creature, void* target, int spot, int* seen, int* heard, int flag);
using DetectionFn = int (*)(void* observer, void* target, int flag);
using DetectModeFn = uint8_t (*)(void* creature);
using FavoredEnemyFn = int (*)(void* stats, void* creature);
using SkillRankFn = char (*)(void* stats, uint8_t skill, void* vs, int base_only);
using WalkableLineFn = int (*)(void* area, float x1, float y1, float x2, float y2);
using StaticLineFn = int (*)(void* area, float x1, float y1, float x2, float y2, float z1, float z2, float radius,
                             uint32_t exclude, uint32_t* blocker, int flag, void* exclude_list, int doors);

SetStealthModeFn g_set_stealth_mode = nullptr;
StealthDetectionFn g_stealth_detection = nullptr;
DetectionFn g_listen_detection = nullptr;
DetectionFn g_spot_detection = nullptr;
DetectModeFn g_detect_mode = nullptr;
FavoredEnemyFn g_favored_enemy = nullptr;

struct Api {
  SkillRankFn skill_rank = reinterpret_cast<SkillRankFn>(EngineExport(ee::kGetSkillRank));  // through the hooks
  WalkableLineFn walkable_line = reinterpret_cast<WalkableLineFn>(EngineExport(ee::kTestLineWalkable));
  StaticLineFn static_line = reinterpret_cast<StaticLineFn>(EngineExport(ee::kStaticObjectsOnLine));
};

const Api& E() {
  static const Api api;
  return api;
}

// The engine's call sites, found at load (D: 4 GetSkillRank calls, 2
// GetDetectMode, 1 GetFavoredEnemyBonus in each detection check; 5 RollDice in
// DoStealthDetection).
CodeRange g_listen_code;
CodeRange g_spot_code;
uintptr_t g_listen_zero_test = 0;  // the hider's Move Silently: 0 = can't hide
uintptr_t g_listen_hider_skill = 0;
uintptr_t g_listen_skill[2] = {};
uintptr_t g_spot_hider_skill = 0;
uintptr_t g_spot_zero_test = 0;
uintptr_t g_spot_skill[2] = {};
uintptr_t g_stealth_dice[5] = {};  // Hide, Move Silently, Spot d10, Spot d20, Listen

std::unordered_map<uint32_t, int> g_stealth_roll;  // hiding creature -> its d20 for this hide
std::unordered_set<uint32_t> g_not_hidden;         // ... whose Stealth total missed 15
std::unordered_map<uint64_t, int> g_noticed;       // (observer, hider, sense) -> last result, for the log
std::unordered_map<uint64_t, int> g_told;          // (searcher, hiding player) -> last result told to the player
int g_log_lines = 0;

thread_local void* t_roller = nullptr;    // DoStealthDetection's creature
thread_local int t_die = -1;              // its Spot / Listen die this 6 s
thread_local void* t_observer = nullptr;  // the detection check's observer
thread_local bool t_scored = false;       // that check got as far as its score

bool LogSlot() { return g_config.log_attacks && g_log_lines < 400 && ++g_log_lines; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}
bool Hiding(const void* creature) { return Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1; }
bool InCombat(const void* creature) { return Field<int32_t>(creature, ee::kCreatureInCombat) != 0; }
bool Searching(const void* creature) {
  return Field<uint8_t>(creature, ee::kCreatureDetectMode) != 0 || (g_config.alert_creatures_search && InCombat(creature));
}

int RollStealth(void* creature, const char* when) {
  const uint32_t id = IdOf(creature);
  const int roll = RollEngineDice(1, 20);
  SetField<uint8_t>(creature, ee::kCreatureHideRoll, static_cast<uint8_t>(roll));
  SetField<uint8_t>(creature, ee::kCreatureMoveSilentlyRoll, static_cast<uint8_t>(roll));
  g_stealth_roll[id] = roll;
  void* stats = StatsOf(creature);
  const int skill = stats && E().skill_rank ? static_cast<int8_t>(E().skill_rank(stats, ee::kSkillHide, nullptr, 0)) : 0;
  const int total = roll + skill;
  const bool hidden = !g_config.hiding_needs_15 || total >= 15;
  if (hidden) {
    g_not_hidden.erase(id);
  } else {
    g_not_hidden.insert(id);
  }
  if (IsPlayer(creature)) {
    char line[160];
    if (!g_config.hiding_needs_15 && g_config.hiders_need_search) {
      sprintf_s(line, "nwn5e: hidden - Stealth %d (d20 %d %+d); enemies that search roll Perception against it", total,
                roll, skill);
    } else if (!g_config.hiding_needs_15) {
      sprintf_s(line, "nwn5e: Stealth %d (d20 %d %+d), kept while you stay hidden", total, roll, skill);
    } else if (hidden) {
      sprintf_s(line, "nwn5e: Stealth %d (d20 %d %+d) - hidden; that roll holds while you stay hidden", total, roll,
                skill);
    } else {
      sprintf_s(line, "nwn5e: Stealth %d (d20 %d %+d) - below 15, you are not hidden (stop and hide again to retry)",
                total, roll, skill);
    }
    TellPlayer(id, line);
    if (LogSlot()) {
      Log("stealth: 0x%08X %s: Stealth d20 %d %+d = %d%s, kept until it stops hiding", id, when, roll, skill, total,
          g_config.hiding_needs_15 ? (hidden ? " (hidden)" : " (below 15: not hidden)") : "");
    }
  }
  return roll;
}

void SetStealthMode(void* creature, uint8_t mode) {
  const bool starts = creature && mode == 1 && !Hiding(creature);
  g_set_stealth_mode(creature, mode);
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  if (starts && engine::AreaOf(creature)) {
    RollStealth(creature, "starts hiding");
  } else if (starts || mode != 1) {
    // Stopped hiding, or being placed (a loaded game restores the mode before
    // the creature is in its area): the next 6 s update rolls if it hides.
    g_stealth_roll.erase(id);
    if (mode != 1) g_not_hidden.erase(id);
  }
  if (mode != 1) {
    for (auto it = g_told.begin(); it != g_told.end();) it = static_cast<uint32_t>(it->first) == id ? g_told.erase(it) : ++it;
  }
}

int StealthDetection(void* creature, void* target, int spot, int* seen, int* heard, int flag) {
  void* outer_roller = t_roller;
  const int outer_die = t_die;
  t_roller = creature;
  t_die = -1;
  const int result = g_stealth_detection(creature, target, spot, seen, heard, flag);
  t_roller = outer_roller;
  t_die = outer_die;
  return result;
}

// The 6 s dice: the kept Stealth roll; one Perception die for Spot and Listen.
bool StealthDice(uintptr_t caller, int count, int sides, int* result) {
  if (!t_roller || count != 1) return false;
  int index = -1;
  for (int i = 0; i < 5; ++i) {
    if (g_stealth_dice[i] && caller == g_stealth_dice[i]) index = i;
  }
  if (index < 0) return false;
  void* creature = t_roller;
  if (index <= 1) {  // Hide / Move Silently
    const auto kept = g_stealth_roll.find(IdOf(creature));
    *result = kept != g_stealth_roll.end() ? kept->second : RollStealth(creature, "is hiding without a roll (loaded)");
    return true;
  }
  if (t_die < 0) {
    const bool worse = !CheckDisadvantage(creature).empty();  // SK11: passive 5, or the lower of two d20s
    t_die = rules::PassiveCheckBase(worse);
    if (Searching(creature)) {
      const int first = RollEngineDice(1, 20);
      t_die = rules::KeptCheckD20(first, worse ? RollEngineDice(1, 20) : 0, 0, worse ? 1 : 0);
    }
  }
  (void)sides;
  *result = t_die;
  return true;
}

// The detection checks ask whether the observer searches only to halve its
// skill when it doesn't: 2024's passive Perception is the whole modifier.
uint8_t GetDetectMode(void* creature) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  if (g_listen_code.Contains(caller) || g_spot_code.Contains(caller)) return 1;
  return g_detect_mode(creature);
}

int GetFavoredEnemyBonus(void* stats, void* creature) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  if (g_listen_code.Contains(caller) || g_spot_code.Contains(caller)) {
    t_scored = true;  // the check got as far as its score
    return 0;
  }
  return g_favored_enemy(stats, creature);
}

int ListenSizeTerm(int hider_size) {
  switch (hider_size) {
    case 1: return -8;
    case 2: return -4;
    case 4: return 4;
    case 5: return 8;
    default: return 0;
  }
}

int SpotSizeTerm(int difference) {
  if (difference <= -2) return -8;
  if (difference == -1) return -4;
  if (difference == 1) return 4;
  if (difference >= 2) return 8;
  return 0;
}

int PerceptionSkill(const SkillQuery& query, int value) {
  const uintptr_t caller = query.caller;
  if (!caller || query.base_only) return value;
  void* creature = CreatureOf(query.stats);
  if (!creature) return value;
  if (caller == g_listen_zero_test || caller == g_spot_zero_test) {  // the hider's stealth
    if (g_config.hiding_needs_15 && g_not_hidden.count(IdOf(creature))) return 0;  // no hide: noticed
    return value == 0 ? 1 : value;  // a total of 0 can hide (a 3e ranks rule)
  }
  if (caller == g_listen_hider_skill) return value + ListenSizeTerm(Field<int32_t>(creature, ee::kCreatureSize));
  if (caller == g_spot_hider_skill && t_observer) {
    return value + SpotSizeTerm(Field<int32_t>(creature, ee::kCreatureSize) - Field<int32_t>(t_observer, ee::kCreatureSize));
  }
  if (caller == g_listen_skill[0] || caller == g_listen_skill[1] || caller == g_spot_skill[0] ||
      caller == g_spot_skill[1]) {
    return InCombat(creature) ? value + 10 : value;  // the engine takes 10 off in combat
  }
  return value;
}

// SK5 B: a hider in front of the observer within 2 m, nothing between them.
constexpr float kUpCloseMetres = 2.0f;

bool ClearLine(void* from, void* to) {
  void* area = engine::AreaOf(from);
  if (!area || engine::AreaOf(to) != area || !E().walkable_line || !E().static_line) return false;
  const float ax = Field<float>(from, ee::kObjectPositionX);
  const float ay = Field<float>(from, ee::kObjectPositionY);
  const float az = Field<float>(from, ee::kObjectPositionZ);
  const float bx = Field<float>(to, ee::kObjectPositionX);
  const float by = Field<float>(to, ee::kObjectPositionY);
  if (E().walkable_line(area, ax, ay, bx, by) != 1) return false;
  uint32_t blocker = ee::kObjectInvalid;
  return E().static_line(area, ax, ay, bx, by, az + 0.5f, az + 1.6f, 0.05f, ee::kObjectInvalid, &blocker, 1, nullptr,
                         0) == 1;
}

bool FoundUpClose(void* observer, void* hider, float* metres) {
  const float dx = Field<float>(hider, ee::kObjectPositionX) - Field<float>(observer, ee::kObjectPositionX);
  const float dy = Field<float>(hider, ee::kObjectPositionY) - Field<float>(observer, ee::kObjectPositionY);
  const float fx = Field<float>(observer, ee::kObjectOrientationX);
  const float fy = Field<float>(observer, ee::kObjectOrientationY);
  *metres = sqrtf(dx * dx + dy * dy);
  return rules::InFrontWithin(dx, dy, fx, fy, kUpCloseMetres) && ClearLine(observer, hider);
}

// E: is the hider inside the observer's vision cone?
bool InVisionCone(void* observer, void* hider) {
  const float dx = Field<float>(hider, ee::kObjectPositionX) - Field<float>(observer, ee::kObjectPositionX);
  const float dy = Field<float>(hider, ee::kObjectPositionY) - Field<float>(observer, ee::kObjectPositionY);
  return rules::WithinVisionCone(dx, dy, Field<float>(observer, ee::kObjectOrientationX),
                                 Field<float>(observer, ee::kObjectOrientationY), static_cast<float>(g_config.vision_cone));
}

// A sight check against a hiding player character, told to the player (once,
// and again when the result changes): a searcher's d20 or a calm look's passive
// score (its Spot die +0x7c6: the d20, or 10 / 5) and its Perception. Only a
// hostile's; one that finds the character takes it out of stealth mode (F).
void TellCheck(void* observer, void* hider, int found, bool searching, bool up_close) {
  const uint64_t key = static_cast<uint64_t>(IdOf(observer)) << 32 | IdOf(hider);
  const auto it = g_told.find(key);
  if (it != g_told.end() && it->second == found) return;
  g_told[key] = found;
  void* stats = StatsOf(observer);
  const int skill = stats && E().skill_rank ? static_cast<int8_t>(E().skill_rank(stats, ee::kSkillSpot, nullptr, 0)) : 0;
  const int die = Field<uint8_t>(observer, 0x7c6);
  char how[64];
  if (up_close) {
    sprintf_s(how, "is right in front of you");
  } else if (searching) {
    sprintf_s(how, "searches (Perception d20 %d %+d)", die, skill);
  } else {
    sprintf_s(how, "looks your way (passive Perception %d)", die + skill);
  }
  const bool ends = found && g_config.found_ends_hiding;
  char code[600];
  sprintf_s(code,
            "object o = StringToObject(\"%x\");"
            "if (GetIsEnemy(o)) {"
            " SendMessageToPC(OBJECT_SELF, \"nwn5e: \" + GetName(o) + \" %s: %s\");"
            " %s"
            "}",
            IdOf(observer), how,
            found ? (ends ? "you are found - you are no longer hidden" : "you are found") : "you stay hidden",
            ends ? "DelayCommand(0.0, SetActionMode(OBJECT_SELF, ACTION_MODE_STEALTH, FALSE));" : "");
  RunScriptChunk(code, IdOf(hider));
  if (ends && LogSlot()) Log("stealth: 0x%08X found by 0x%08X - out of stealth mode (found_ends_hiding)", IdOf(hider), IdOf(observer));
}

int Detection(DetectionFn original, bool spot, void* observer, void* target, int flag) {
  void* outer_observer = t_observer;
  const bool outer_scored = t_scored;
  t_observer = observer;
  t_scored = false;
  int result = original(observer, target, flag);
  const bool scored = t_scored;
  t_observer = outer_observer;
  t_scored = outer_scored;
  if (!observer || !target || !Hiding(target)) return result;
  char up_close[64] = "";
  float metres = 0.0f;
  // D / E: a calm observer sees a hider only inside its vision cone (it can
  // hear one anywhere); searching ones see all round.
  const bool searching = Searching(observer);
  const bool in_cone = !searching && g_config.hiders_need_search && InVisionCone(observer, target);
  if (spot && result && g_config.hiders_need_search && !searching && !in_cone) {
    result = 0;
    sprintf_s(up_close, "; calm, outside its %d-degree cone: unseen", g_config.vision_cone);
  }
  bool close = false;
  if (spot && !result && scored && g_config.found_up_close && FoundUpClose(observer, target, &metres)) {
    result = 1;
    close = true;
    sprintf_s(up_close, "; found up close: %.1f m in front, clear line", metres);
  }
  if (spot && g_config.hiders_need_search && (searching || in_cone || result) && IsPlayer(target)) {
    TellCheck(observer, target, result, searching, close);
  }
  if (!g_config.log_attacks) return result;
  const uint64_t key = (static_cast<uint64_t>(IdOf(observer)) << 32 | IdOf(target)) ^ (spot ? 1ull << 63 : 0);
  const auto it = g_noticed.find(key);
  if (it != g_noticed.end() && it->second == result) return result;
  g_noticed[key] = result;
  if ((IsPlayer(target) || IsPlayer(observer)) && LogSlot()) {
    Log("perception: 0x%08X %s 0x%08X (%s; its Stealth d20 %d; observer %s, die %d%s)", IdOf(observer),
        result ? (spot ? "sees" : "hears") : (spot ? "does not see" : "does not hear"), IdOf(target),
        scored ? "scored" : "decided before the score", Field<uint8_t>(target, ee::kCreatureHideRoll),
        Searching(observer) ? "searching" : "passive", Field<uint8_t>(observer, spot ? 0x7c6 : 0x7c7), up_close);
  }
  return result;
}

int DoListenDetection(void* observer, void* target, int flag) {
  return Detection(g_listen_detection, false, observer, target, flag);
}

int DoSpotDetection(void* observer, void* target, int flag) {
  return Detection(g_spot_detection, true, observer, target, flag);
}

std::vector<uintptr_t> Returns(const CodeRange& code, const char* target) {
  std::vector<uintptr_t> calls = FindCallsTo(code, EngineExport(target));
  for (uintptr_t& call : calls) call += kCallLength;
  return calls;
}

}  // namespace

void InstallPerceptionRules() {
  if (g_config.perception != rules::RuleVariant::kFaithful) return;
  g_listen_code = FunctionAt(EngineExport(ee::kDoListenDetection));
  g_spot_code = FunctionAt(EngineExport(ee::kDoSpotDetection));
  const std::vector<uintptr_t> listen = Returns(g_listen_code, ee::kGetSkillRank);
  const std::vector<uintptr_t> spot = Returns(g_spot_code, ee::kGetSkillRank);
  const std::vector<uintptr_t> dice = Returns(FunctionAt(EngineExport(ee::kDoStealthDetection)), ee::kRollDice);
  if (listen.size() != 4 || spot.size() != 4 || dice.size() != 5) {
    Log("SKIP perception: the checks' calls differ from this build's (%zu / %zu skill, %zu dice)", listen.size(),
        spot.size(), dice.size());
    return;
  }
  g_listen_zero_test = listen[0];
  g_listen_hider_skill = listen[1];
  g_listen_skill[0] = listen[2];
  g_listen_skill[1] = listen[3];
  g_spot_hider_skill = spot[0];
  g_spot_zero_test = spot[1];
  g_spot_skill[0] = spot[2];
  g_spot_skill[1] = spot[3];
  for (int i = 0; i < 5; ++i) g_stealth_dice[i] = dice[i];
  HookExport("CNWSCreature::SetStealthMode (Stealth rolled when hiding starts)", ee::kSetStealthMode,
             reinterpret_cast<void*>(&SetStealthMode), reinterpret_cast<void**>(&g_set_stealth_mode));
  HookExport("CNWSCreature::DoStealthDetection (6 s rolls: kept Stealth, passive Perception)", ee::kDoStealthDetection,
             reinterpret_cast<void*>(&StealthDetection), reinterpret_cast<void**>(&g_stealth_detection));
  HookExport("CNWSCreature::DoListenDetection (log)", ee::kDoListenDetection, reinterpret_cast<void*>(&DoListenDetection),
             reinterpret_cast<void**>(&g_listen_detection));
  HookExport("CNWSCreature::DoSpotDetection (found up close, log)", ee::kDoSpotDetection,
             reinterpret_cast<void*>(&DoSpotDetection), reinterpret_cast<void**>(&g_spot_detection));
  HookExport("CNWSCreature::GetDetectMode (whole skill when not searching)", ee::kGetDetectMode,
             reinterpret_cast<void*>(&GetDetectMode), reinterpret_cast<void**>(&g_detect_mode));
  HookExport("CNWSCreatureStats::GetFavoredEnemyBonus (none on perception)", ee::kGetFavoredEnemyBonus,
             reinterpret_cast<void*>(&GetFavoredEnemyBonus), reinterpret_cast<void**>(&g_favored_enemy));
  AddSkillAdjuster(&PerceptionSkill);
  AddDiceFilter(&StealthDice);
  Log("perception: Stealth once per hide, passive Perception unless searching, no size / combat / Favored Enemy "
      "terms (faithful)");
}

// SK9 (rules_initiative.cpp) and Search: hidden = hiding with a hide that held.
bool HiddenByStealth(void* creature) {
  if (g_config.perception != rules::RuleVariant::kFaithful || !creature || !Hiding(creature)) return false;
  return !g_not_hidden.count(IdOf(creature));
}

// The wedge's colour (stealth_watch.cpp; owner, 2026-10-25: "would it be possible
// for the wedge to have different colors depending on how easy it would be for
// the enemy to spot the PC"): a calm observer's passive look at a hider, added up
// as the engine's spot check does (read from DoSpotDetection) - its passive
// Perception (10, 5 with disadvantage, + its Perception), the area's Spot
// modifier (+0x21c), +5 when it stands still, -5 when the hider does, and only at
// night (module +0x3d4 == 2, what GetIsNight reads) -5 without a light (visuals
// 153-180) or darkvision (182) on it and +10 when the hider carries a light -
// minus the hider's kept Stealth total and the size term. Over 0: inside its
// cone it sees the hider. False when there's nothing to weigh.
namespace {

constexpr size_t kAreaSpotModifier = 0x21c;  // int32, the area's Spot modifier
constexpr size_t kModuleTimeOfDay = 0x3d4;    // uint8, 2 = night (GetIsNight)
constexpr size_t kObjectAnimation = 0xd8;     // uint16, what AnimationStationary is asked about
constexpr size_t kVisuals = 0x160;            // the visual effects on an object (0x20 bytes each, id first)
constexpr size_t kVisualCount = 0x168;

using StationaryFn = int (*)(void* object, uint16_t animation);
using GetModuleFn = void* (*)(void* server_app);

bool Still(void* object) {
  static const auto stationary =
      reinterpret_cast<StationaryFn>(EngineExport("?AnimationStationary@CNWSObject@@QEAAHG@Z"));
  return stationary && stationary(object, Field<uint16_t>(object, kObjectAnimation)) != 0;
}

bool Night() {
  static void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  static const auto get_module =
      reinterpret_cast<GetModuleFn>(EngineExport("?GetModule@CServerExoApp@@QEAAPEAVCNWSModule@@XZ"));
  if (!app_manager || !*app_manager || !get_module) return false;
  void* app = Field<void*>(*app_manager, ee::kAppManagerServerApp);
  void* module = app ? get_module(app) : nullptr;
  return module && Field<uint8_t>(module, kModuleTimeOfDay) == 2;
}

// A light visual on it (153-180); with `darkvision`, the darkvision one (182) counts too.
bool Lit(void* object, bool darkvision) {
  const int count = Field<int32_t>(object, kVisualCount);
  const uint8_t* list = Field<const uint8_t*>(object, kVisuals);
  if (!list || count <= 0 || count > 256) return false;
  for (int i = 0; i < count; ++i) {
    const int id = *reinterpret_cast<const uint16_t*>(list + 0x20 * i);
    if ((id >= 153 && id <= 180) || (darkvision && id == 182)) return true;
  }
  return false;
}

}  // namespace

bool SpotMargin(void* observer, void* hider, int* margin) {
  if (g_config.perception != rules::RuleVariant::kFaithful || !observer || !hider || !Hiding(hider)) return false;
  const auto kept = g_stealth_roll.find(IdOf(hider));
  if (kept == g_stealth_roll.end() || !E().skill_rank) return false;
  void* seer = StatsOf(observer);
  void* hid = StatsOf(hider);
  if (!seer || !hid) return false;
  const bool worse = !CheckDisadvantage(observer).empty();
  int score = rules::PassiveCheckBase(worse) + static_cast<int8_t>(E().skill_rank(seer, ee::kSkillSpot, nullptr, 0));
  if (void* area = engine::AreaOf(observer)) score += Field<int32_t>(area, kAreaSpotModifier);
  if (Still(observer)) score += 5;
  if (Still(hider)) score -= 5;
  if (Night()) {
    if (!Lit(observer, true)) score -= 5;
    if (Lit(hider, false)) score += 10;
  }
  const int stealth = kept->second + static_cast<int8_t>(E().skill_rank(hid, ee::kSkillHide, nullptr, 0)) +
                      SpotSizeTerm(Field<int32_t>(hider, ee::kCreatureSize) - Field<int32_t>(observer, ee::kCreatureSize));
  *margin = score - stealth;
  return true;
}

// Search's die (rules_skill_dcs.cpp), the 2024 way: a d20 when searching, else passive.
int SearchDie(void* searcher, bool worse) {
  if (Field<uint8_t>(searcher, ee::kCreatureDetectMode) == 0) return rules::PassiveCheckBase(worse);
  const int first = RollEngineDice(1, 20);
  return rules::KeptCheckD20(first, worse ? RollEngineDice(1, 20) : 0, 0, worse ? 1 : 0);
}
