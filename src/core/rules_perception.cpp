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
//   markers, rings and cones go with it (stealth_watch.cpp). found_ends_hiding;
// - light (G; owner, 2026-10-25: "explore if we can make it so that
//   environmental light sources can increase detection" - "do the sensible
//   distances"): the light where a hiding player character stands - bright, dim
//   or dark, worked out once a second by stealth_watch.cpp (local
//   nwn5e_light_level, +1) - is 2024's: bright, nothing; dim (lightly obscured),
//   -5 to an observer's sight; dark (heavily obscured), unseen - but an observer
//   with darkvision (feat 228, or the darkvision visual) within 18 m sees it as
//   dim. Up close still finds. EE's own night terms (SPOT_NO_LIGHT_MODIFIER,
//   HIDING_LIGHT_MODIFIER) are zeroed then (data/rows/ruleset.if_light2024.txt):
//   this replaces them. stealth_light. A player's henchman hiding counts too
//   (0.35.0, HS3): stealth_watch.cpp works out its light as well;
// - henchmen (0.35.0; owner, 2026-10-25: "how do henchmen deal with stealth
//   mode? If they're following the PC they should also enter stealth mode" -
//   "go with all your recommendations"): a player's henchman found while hiding
//   (F) is taken out of stealth mode on its own - the player is told - and stays
//   out until the player hides again (local nwn5e_found_out; stealth_watch.cpp's
//   follow); the player character found takes its followers out with it;
// - Reliable Talent (0.35.0, RB2; SRD 5.2 Rogue 7): a rogue of level 7+
//   proficient in Stealth treats a Stealth d20 of 9 or lower as a 10
//   (class_features faithful).
// - a light the hider carries (0.36.0; owner, 2026-10-25: "the brighter the
//   weapon & equipment the greater penalty"): by its reach (the item property
//   Light puts a light effect 153-180 on the wearer: 5 / 10 / 15 / 20 m) a
//   watcher spots it at +2 / +4 / +6 / +8 - the brightest one counts - and it
//   stands in dim light (5 m) or bright (10 m+; stealth_watch.cpp). This
//   replaces EE's flat +10 for a lit hider at night (zeroed with stealth_light).
// Distance, area, movement, facing and light terms stay (NWN's real-time
// stealth needs them). A deafened observer can't hear and a blinded one can't
// see: EE's own checks already fail them (SK11). NWN mode: NWN's.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <intrin.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chat.h"
#include "cast_action.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "feats.h"
#include "hooks.h"
#include "log.h"
#include "grenades.h"
#include "rules/class_features.h"
#include "rules/grenades.h"
#include "rules/skills.h"
#include "data/twoda.h"
#include "rules_loaded.h"
#include "skill_rank.h"

bool StealthParts(void* creature, int* extra, bool* proficient);  // rules_skills.cpp
int ReliableTalentRoll(void* creature, int nwn_skill, int d20);    // rules_skills.cpp (Rogue R9)
bool InFogCloud(void* creature);                                  // rules_spells2024.cpp (#39 build 2c)
extern "C" __declspec(dllimport) unsigned short __stdcall RtlCaptureStackBackTrace(unsigned long skip, unsigned long count,
                                                                                 void** frames, unsigned long* hash);
extern "C" __declspec(dllimport) unsigned long long __stdcall GetTickCount64();

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

// #129 B: an attack from hiding (the 2024 Hide).
using ActivityManagerFn = void (*)(void* creature, uint32_t action);
using ResolveAttackFn = void (*)(void* creature, uint32_t target, int attacks, int animation);
using SetActivityFn = void (*)(void* creature, int activity, int on);
constexpr const char* kActivityManager = "?ActivityManager@CNWSCreature@@QEAAXI@Z";
constexpr const char* kResolveAttack = "?ResolveAttack@CNWSCreature@@IEAAXIHH@Z";
constexpr const char* kSetActivity = "?SetActivity@CNWSCreature@@QEAAXHH@Z";
constexpr size_t kCreatureActivityBits = 0x8e0;   // uint32: bit 0 stealth (SetActivity)
constexpr size_t kCreatureActivitiesLeft = 0x8e4; // uint32: activities ActivityManager / SetActivity leave alone
constexpr uint32_t kActivityStealth = 1;
constexpr uint32_t kActivityForAttack = 4;        // ResolveAttack's ActivityManager(4)
constexpr uint32_t kActivityForCast = 1;          // AIActionCastSpell's and AIActionItemCastSpell's ActivityManager(1)
ActivityManagerFn g_activity_manager = nullptr;
// #132: CNWSCreature::AIActionItemCastSpell (a grenade is a spell cast from an item) - a throw running now.
using AIActionItemCastSpellFn = uint32_t (*)(void* creature, void* node);
constexpr const char* kAIActionItemCastSpell = "?AIActionItemCastSpell@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
AIActionItemCastSpellFn g_item_cast_spell = nullptr;
thread_local bool t_grenade_throw = false;
// 2026-11-09 (A): the spells without a Verbal component (the served spells.2da's VS, read at the first rules load)
// - cast from hiding they keep the hide.
std::unordered_set<int> g_spells_without_verbal;
bool g_spell_components_read = false;
std::unordered_map<uint32_t, int64_t> g_cast_logged;  // id -> ms: once a cast (its ActivityManager(1) each tick)
std::unordered_map<uint32_t, int64_t> g_throw_logged;  // id -> ms: ActivityManager(1) comes each tick of a cast (0.71.1)
// #133: where each creature last heard each hider ((observer << 32) | hider -> x, y, z), the search's goal; and when
// a creature last turned an attack into a search (id -> ms).
struct HeardAt {
  float x, y, z;
};
std::unordered_map<uint64_t, HeardAt> g_heard_at;
std::unordered_map<uint32_t, int64_t> g_hunt_at;
std::unordered_map<uint32_t, int64_t> g_hunt_logged;
// #136: where each creature's own search last sent it (id -> the spot, when) - that move isn't turned again.
struct SearchMove {
  float x, y, z;
  int64_t at;
};
std::unordered_map<uint32_t, SearchMove> g_search_move;
int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
ResolveAttackFn g_resolve_attack = nullptr;
std::unordered_set<uint32_t> g_hide_ends_after_attack;  // ids: a swing started from hiding, its roll to come

// #129 (0.66.6): a hide checked for at once.
using PerceptionUpdateFn = void (*)(void* observer, void* target, int flag);
constexpr const char* kDoPerceptionUpdateOnCreature = "?DoPerceptionUpdateOnCreature@CNWSCreature@@QEAAXPEAV1@H@Z";
constexpr int kFeatHideInPlainSight = 433;  // 0x1B1: what keeps DoPerceptionUpdateOnCreature's flag
CodeRange g_perception_update_code;
thread_local void* t_rechecked_hider = nullptr;  // PerceiveHiderNow's hider (its stats) while its checks run
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
thread_local void* t_target = nullptr;    // ... and its target
thread_local bool t_scored = false;       // that check got as far as its score

bool LogSlot() { return g_config.log_attacks && g_log_lines < 400 && ++g_log_lines; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}
bool Hiding(const void* creature) { return Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1; }
// A player character's henchman.
bool PlayersHenchman(void* creature) {
  if (!creature || Field<uint16_t>(creature, ee::kCreatureAssociateType) != ee::kAssociateTypeHenchman) return false;
  const uint32_t master = Field<uint32_t>(creature, ee::kCreatureMaster);
  return master && master != ee::kObjectInvalid && IsPlayer(CreatureById(master));
}
// A hider whose finding is told and ends its hiding (F): a player character or a player's henchman.
bool PartyHider(void* creature) { return IsPlayer(creature) || PlayersHenchman(creature); }
bool InCombat(const void* creature) { return Field<int32_t>(creature, ee::kCreatureInCombat) != 0; }
bool Searching(const void* creature) {
  return Field<uint8_t>(creature, ee::kCreatureDetectMode) != 0 || (g_config.alert_creatures_search && InCombat(creature));
}

int RollStealth(void* creature, const char* when) {
  const uint32_t id = IdOf(creature);
  // RB2 Reliable Talent: a rogue 7+ proficient in Stealth treats 9 or lower as 10.
  const int rolled = RollEngineDice(1, 20);
  void* rogue_stats = StatsOf(creature);
  const int rogue = rogue_stats && g_config.class_features == rules::RuleVariant::kFaithful
                        ? ClassLevel2024(rogue_stats, rules::CharacterClass::kRogue)
                        : 0;
  int extra = 0;
  bool proficient = false;
  const bool parts = StealthParts(creature, &extra, &proficient);
  const int roll = rules::ReliableTalentD20(rolled, rogue, parts && proficient);
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
    char line[256];
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
    if (roll != rolled) {
      char reliable[80];
      sprintf_s(reliable, " - Reliable Talent: your d20 of %d counts as 10", rolled);
      strcat_s(line, reliable);
    }
    TellPlayer(id, line);
  }
  if (LogSlot() && PartyHider(creature)) {
    Log("stealth: 0x%08X %s: Stealth d20 %d%s %+d = %d%s, kept until it stops hiding", id, when, roll,
        roll != rolled ? " (Reliable Talent)" : "", skill, total,
        g_config.hiding_needs_15 ? (hidden ? " (hidden)" : " (below 15: not hidden)") : "");
  }
  return roll;
}

// Whether this stealth change comes through CNWSCreature::ToggleMode - the
// player's stealth button, or a script's SetActionMode (a find, F) - looked for
// in the frames above SetActivity.
bool FromToggleMode() {
  static const CodeRange toggle = FunctionAt(EngineExport("?ToggleMode@CNWSCreature@@QEAAHE@Z"));
  void* frames[5] = {};
  const unsigned short count = RtlCaptureStackBackTrace(0, 5, frames, nullptr);
  for (unsigned short i = 1; i < count; ++i) {
    if (toggle.Valid() && toggle.Contains(reinterpret_cast<uintptr_t>(frames[i]))) return true;
  }
  return false;
}

// A ranged Snuff's mark (light_sources.cpp: the second it was set): good for 30 s.
bool Snuffing(void* creature) {
  const int mark = engine::GetLocalInt(creature, "nwn5e_snuffing");
  const long long now = static_cast<long long>((GetTickCount64() / 1000) & 0x7FFFFFFF);
  return mark > 0 && mark <= now && now - mark <= 30;
}

// #129 (0.66.6; the owner on 0.66.5: "check the logs, the enemy never lost sight of me" - a creature checks the
// hiders it sees at its perception update, every 6 s, and both hides were attacked from within 5.4 s): a hide takes
// effect at once, as the 2024 Hide does - every creature in the hider's area checks for it now
// (CNWSCreature::DoPerceptionUpdateOnCreature(observer, hider, 1): the Stealth and Perception rules above, then its
// visible list), as SetStealthMode itself does for Hide in Plain Sight (feat 433) - not at its next update. One who
// still finds it (up close, a look in its cone, a search) keeps seeing it, and a hostile takes it out of stealth
// mode (F).
// 0.66.7 (the owner on 0.66.6: "no matter where I am, even if I'm behind them and outside the passive perception
// range (blue dashed line) the enemy will still have me as seen"): DoPerceptionUpdateOnCreature skips the stealth
// checks for a target its observer both sees and hears (visibility node +4, bits 0 and 1) unless its flag holds,
// and it keeps the flag only for a target with Hide in Plain Sight (HasFeat(433) at its start) - so the 0.66.6 call
// changed nothing. A HasFeat filter answers yes there, for the hider, while these checks run: the engine's own
// re-check (its detection, its visible list, its perception events).
bool ForcedRecheck(void* stats, int feat, uintptr_t caller, int* answer) {
  if (feat != kFeatHideInPlainSight || !t_rechecked_hider || stats != t_rechecked_hider) return false;
  if (!g_perception_update_code.Valid() || !g_perception_update_code.Contains(caller)) return false;
  *answer = 1;
  return true;
}

int PerceiveHiderNow(void* hider) {
  static const auto update = reinterpret_cast<PerceptionUpdateFn>(EngineExport(kDoPerceptionUpdateOnCreature));
  void* area = engine::AreaOf(hider);
  if (!update || !area) return 0;
  const uint32_t* ids = Field<uint32_t*>(area, ee::kAreaObjectIds);
  const int count = Field<int32_t>(area, ee::kAreaObjectCount);
  if (!ids || count <= 0) return 0;
  const std::vector<uint32_t> others(ids, ids + count);  // copied: the updates may touch the area's lists
  const uint32_t self = IdOf(hider);
  void* outer = t_rechecked_hider;
  t_rechecked_hider = StatsOf(hider);
  int checked = 0;
  for (uint32_t id : others) {
    if (id == self) continue;
    void* observer = CreatureById(id);
    if (!observer) continue;
    update(observer, hider, 1);
    ++checked;
  }
  t_rechecked_hider = outer;
  return checked;
}

void SetStealthMode(void* creature, uint8_t mode) {
  // A ranged Snuff (light_sources.cpp, 0.39.1; owner: "would it be possible with
  // 'snuff' to maintain stealth if it is performed while in stealth?"): its
  // attack doesn't take the hider out of stealth mode. 0.39.2 (owner: "I can't
  // turn off stealth mode now" - a mark kept in a saved game held every stealth
  // change back): only the engine's own attack path is held back - never one
  // through ToggleMode (the player's button, a script's find) - and the mark
  // lasts 30 s. Who tried is logged (the first few). 0.39.3 (owner: "once a
  // light source is snuffed while in stealth I can no longer toggle off the
  // stealth"): CNWSCreature::SetActivity clears the stealth activity bit (+0x8e0
  // bit 0) before it asks for stealth off, so a kept hider was hiding with the
  // activity off - and the stealth button, finding it off, had nothing to turn
  // off. The bit is set back with it.
  if (creature && mode != 1 && Hiding(creature) && Snuffing(creature) && !FromToggleMode()) {
    constexpr size_t kCreatureActivities = 0x8e0;  // uint32: bit 0 stealth (SetActivity / ActivityManager)
    SetField<uint32_t>(creature, kCreatureActivities, Field<uint32_t>(creature, kCreatureActivities) | 1u);
    static int told = 0;
    if (told < 6) {
      ++told;
      uintptr_t offset = 0;
      const char* where = ExportContaining(reinterpret_cast<uintptr_t>(_ReturnAddress()), &offset);
      Log("stealth: 0x%08X snuffing a light - kept hidden (stealth off asked from %s+0x%llx)", IdOf(creature),
          where ? where : "?", static_cast<unsigned long long>(offset));
    }
    return;
  }
  if (creature && mode == 1 && !Hiding(creature)) {  // a new hide: no find, no snuff mark yet
    engine::SetLocalInt(creature, "nwn5e_found_out", 0);
    engine::SetLocalInt(creature, "nwn5e_snuffing", 0);
  }
  const bool starts = creature && mode == 1 && !Hiding(creature);
  g_set_stealth_mode(creature, mode);
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  if (starts && engine::AreaOf(creature)) {
    RollStealth(creature, "starts hiding");
    const int checked = PerceiveHiderNow(creature);
    if (LogSlot()) {
      Log("stealth: 0x%08X hides - %d creature(s) in its area check for it now (#129: as Hide in Plain Sight)", id,
          checked);
    }
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

// #129 B (the owner, 2026-11-03: "do A and B now before I continue with the matrix"): an attack from hiding is
// rolled hidden, and the hide ends after its roll - the 2024 Hide (rules::WhenAttackEndsHide). Each swing's
// CNWSCreature::ResolveAttack calls ActivityManager(4) before its roll, which turns the stealth activity off
// (SetActivity -> SetStealthMode(0)) - NWN's way, the roll then made in plain view. Here that call leaves the stealth
// activity (marked left alone for the call: +0x8E4 bit 0, as ActivityManager reads it), and once ResolveAttack is
// done - its roll (Advantage while the target can't see the hider: rules_advantage.cpp) and damage - the hide ends
// by SetActivity(stealth, 0).
void ActivityManager(void* creature, uint32_t action) {
  // #132 (owner, 2026-11-04: "go with your recommendations" - A): a grenade thrown from hiding keeps the hide (2024:
  // its target saves, no attack roll). The item cast's ActivityManager(1) turns the stealth activity off unless it is
  // left alone (+0x8E4 bit 0, as above for an attack) - left alone for that call; the rest (combat modes) as NWN's.
  if (creature && action == kActivityForCast && t_grenade_throw &&
      rules::ThrowKeepsHiding(Hiding(creature), true, true) &&
      (Field<uint32_t>(creature, kCreatureActivityBits) & kActivityStealth)) {
    const uint32_t left = Field<uint32_t>(creature, kCreatureActivitiesLeft);
    SetField<uint32_t>(creature, kCreatureActivitiesLeft, left | kActivityStealth);
    g_activity_manager(creature, action);
    SetField<uint32_t>(creature, kCreatureActivitiesLeft, left);
    // Once a throw (the call comes each tick while the cast runs): 3 s apart per thrower.
    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t& last = g_throw_logged[IdOf(creature)];
    if (now - last > 3000 && LogSlot()) {
      last = now;
      Log("stealth: 0x%08X throws a grenade from hiding - still hidden (2024: a save, no attack roll; #132)%s",
          IdOf(creature), Hiding(creature) ? "" : " - NOT hidden: the engine ended it anyway");
    }
    return;
  }
  // 2026-11-09 (owner: "go with A"): a spell without a Verbal component cast from hiding keeps the hide - its
  // cast's ActivityManager(1) leaves the stealth activity alone, as a grenade's.
  const int spell = action == kActivityForCast && !t_grenade_throw ? CastingSpellNow() : -1;
  if (creature && spell >= 0 && g_spell_components_read &&
      rules::CastKeepsHiding(Hiding(creature), g_spells_without_verbal.count(spell) == 0, true) &&
      (Field<uint32_t>(creature, kCreatureActivityBits) & kActivityStealth)) {
    const uint32_t left = Field<uint32_t>(creature, kCreatureActivitiesLeft);
    SetField<uint32_t>(creature, kCreatureActivitiesLeft, left | kActivityStealth);
    g_activity_manager(creature, action);
    SetField<uint32_t>(creature, kCreatureActivitiesLeft, left);
    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t& last = g_cast_logged[IdOf(creature)];
    if (now - last > 3000 && LogSlot()) {
      last = now;
      Log("stealth: 0x%08X casts spell %d from hiding - no Verbal component, still hidden (2024's Hide)%s",
          IdOf(creature), spell, Hiding(creature) ? "" : " - NOT hidden: the engine ended it anyway");
    }
    return;
  }
  if (!creature || action != kActivityForAttack ||
      rules::WhenAttackEndsHide(Hiding(creature), true) != rules::HideAtAttack::kEndsAfterRoll ||
      !(Field<uint32_t>(creature, kCreatureActivityBits) & kActivityStealth)) {
    g_activity_manager(creature, action);
    return;
  }
  const uint32_t left = Field<uint32_t>(creature, kCreatureActivitiesLeft);
  SetField<uint32_t>(creature, kCreatureActivitiesLeft, left | kActivityStealth);
  g_activity_manager(creature, action);
  SetField<uint32_t>(creature, kCreatureActivitiesLeft, left);
  g_hide_ends_after_attack.insert(IdOf(creature));
}

// 2026-11-09 (A): the served spells.2da's spells (UserType 1 - not a feat's or a creature's power, NWN's for those)
// without a Verbal component (rules::SpellHasVerbal on VS), once.
void ReadSpellComponents(void* /*rules*/, bool /*reload*/) {
  if (g_spell_components_read) return;
  std::string bytes, why, error;
  data::TwoDA table;
  if (!ReadGameResource("spells", ee::kResourceType2da, &bytes, &why) || !data::ParseTwoDA(bytes, &table, &error)) {
    Log("SKIP stealth: spells.2da not read (%s%s) - every cast ends stealth mode", why.c_str(), error.c_str());
    return;
  }
  size_t vs = table.columns.size(), user_type = table.columns.size();
  for (size_t i = 0; i < table.columns.size(); ++i) {
    if (table.columns[i] == "VS") vs = i;
    if (table.columns[i] == "UserType") user_type = i;
  }
  if (vs == table.columns.size() || user_type == table.columns.size()) {
    Log("SKIP stealth: spells.2da has no VS or UserType column - every cast ends stealth mode");
    return;
  }
  for (size_t row = 0; row < table.rows.size(); ++row) {
    const std::vector<std::string>& values = table.rows[row];
    if (vs < values.size() && user_type < values.size() && values[user_type] == "1" &&
        !rules::SpellHasVerbal(values[vs])) {
      g_spells_without_verbal.insert(static_cast<int>(row));
    }
  }
  g_spell_components_read = true;
  Log("stealth: %zu of %zu spells.2da rows are spells with no Verbal component - cast from hiding, they keep the "
      "hide",
      g_spells_without_verbal.size(), table.rows.size());
}

// #132: the item cast marks a grenade's throw for ActivityManager (above) while it runs.
uint32_t AIActionItemCastSpell(void* creature, void* node) {
  const bool outer = t_grenade_throw;
  t_grenade_throw = GrenadesFaithful() && IsGrenadeCast(node);
  const uint32_t result = g_item_cast_spell(creature, node);
  t_grenade_throw = outer;
  return result;
}

void ResolveAttack(void* creature, uint32_t target, int attacks, int animation) {
  g_resolve_attack(creature, target, attacks, animation);
  if (!creature || g_hide_ends_after_attack.erase(IdOf(creature)) == 0 || !Hiding(creature)) return;
  static const auto set_activity = reinterpret_cast<SetActivityFn>(EngineExport(kSetActivity));
  if (set_activity) set_activity(creature, static_cast<int>(kActivityStealth), 0);
  if (LogSlot()) {
    Log("stealth: 0x%08X attacked 0x%08X from hiding - the roll made hidden, out of stealth mode after it (the 2024 "
        "Hide)%s", IdOf(creature), target, Hiding(creature) ? " - STILL hiding: the engine kept it" : "");
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
      t_die = ReliableTalentRoll(creature, ee::kSkillSpot, t_die);  // Rogue R9: Perception
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

// G: the light where a hider stands - 0 dark, 1 dim, 2 bright (also: unknown).
constexpr const char* kVarLightLevel = "nwn5e_light_level";  // stealth_watch.cpp writes it, + 1
constexpr int kFeatDarkvision = 228;
constexpr float kDarkvisionMetres = 18.0f;  // 60 feet

bool LightRule() { return g_config.stealth_light; }

int HiderLight(void* hider) {
  const int stored = engine::GetLocalInt(hider, kVarLightLevel) - 1;
  return stored < 0 ? 2 : stored;
}

bool HasDarkvisionVisual(void* object) {
  const int count = Field<int32_t>(object, 0x168);
  const uint8_t* list = Field<const uint8_t*>(object, 0x160);
  if (!list || count <= 0 || count > 256) return false;
  for (int i = 0; i < count; ++i) {
    if (*reinterpret_cast<const uint16_t*>(list + 0x20 * i) == 182) return true;
  }
  return false;
}

// Darkvision reaching the hider: it sees the dark as dim.
bool SeesInDark(void* observer, void* hider) {
  void* stats = StatsOf(observer);
  if (!(stats && engine::HasFeat(stats, kFeatDarkvision)) && !HasDarkvisionVisual(observer)) return false;
  const float dx = Field<float>(hider, ee::kObjectPositionX) - Field<float>(observer, ee::kObjectPositionX);
  const float dy = Field<float>(hider, ee::kObjectPositionY) - Field<float>(observer, ee::kObjectPositionY);
  return dx * dx + dy * dy <= kDarkvisionMetres * kDarkvisionMetres;
}

// The reach of the brightest light it carries itself (its light effects 153-180:
// 5 x (1 + (row - 153) % 4) m), 0 if none.
int CarriedLightRadiusOf(void* object) {
  const int count = Field<int32_t>(object, 0x168);
  const uint8_t* list = Field<const uint8_t*>(object, 0x160);
  if (!list || count <= 0 || count > 256) return 0;
  int radius = 0;
  for (int i = 0; i < count; ++i) {
    const int id = *reinterpret_cast<const uint16_t*>(list + 0x20 * i);
    if (id >= 153 && id <= 180) radius = (std::max)(radius, 5 * (1 + (id - 153) % 4));
  }
  return radius;
}

// What the hider's carried light gives a watcher: +2 per 5 m of its reach.
int CarriedLightTerm(void* hider) {
  if (!LightRule() || !hider || !Hiding(hider)) return 0;
  return rules::CarriedLightSpotBonus(CarriedLightRadiusOf(hider));
}

// What the hider's light does to this observer's sight: 0, -5 (dim), or unseen.
constexpr int kUnseen = -1000;
int LightTerm(void* observer, void* hider) {
  if (!LightRule() || !hider || !Hiding(hider)) return 0;
  // #39 build 2c (Q2 A): Fog Cloud is Heavily Obscured - a hider in it, or an observer in it, unseen.
  if (InFogCloud(hider) || (observer && InFogCloud(observer))) return kUnseen;
  const int level = HiderLight(hider);
  if (level >= 2) return 0;
  if (level == 1) return -5;
  return SeesInDark(observer, hider) ? -5 : kUnseen;
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
  if (caller == g_spot_skill[0] || caller == g_spot_skill[1]) {
    const int light = t_target ? LightTerm(creature, t_target) : 0;  // G: dim light, -5
    const int carried = t_target ? CarriedLightTerm(t_target) : 0;   // its own light: +2 per 5 m
    return (InCombat(creature) ? value + 10 : value) + (light == kUnseen ? 0 : light) + carried;
  }
  if (caller == g_listen_skill[0] || caller == g_listen_skill[1]) {
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
  const bool player = IsPlayer(hider);
  if (!player && !found) return;  // a henchman: told only when it is found
  void* stats = StatsOf(observer);
  const int skill = stats && E().skill_rank ? static_cast<int8_t>(E().skill_rank(stats, ee::kSkillSpot, nullptr, 0)) : 0;
  const int die = Field<uint8_t>(observer, 0x7c6);
  char how[64];
  if (up_close) {
    sprintf_s(how, player ? "is right in front of you" : "is right in front of it");
  } else if (searching) {
    sprintf_s(how, "searches (Perception d20 %d %+d)", die, skill);
  } else {
    sprintf_s(how, player ? "looks your way (passive Perception %d)" : "looks its way (passive Perception %d)",
              die + skill);
  }
  const bool ends = found && g_config.found_ends_hiding;
  const char* leave = ends ? "DelayCommand(0.0, SetActionMode(OBJECT_SELF, ACTION_MODE_STEALTH, FALSE));"
                            " SetLocalInt(OBJECT_SELF, \"nwn5e_found_exit\", 1);"
                            " SetLocalInt(OBJECT_SELF, \"nwn5e_found_out\", 1);"
                          : "";
  char code[700];
  if (player) {
    sprintf_s(code,
              "object o = StringToObject(\"%x\");"
              "if (GetIsEnemy(o)) {"
              " SendMessageToPC(OBJECT_SELF, \"nwn5e: \" + GetName(o) + \" %s: %s\");"
              " %s"
              "}",
              IdOf(observer), how,
              found ? (ends ? "you are found - you are no longer hidden" : "you are found") : "you stay hidden", leave);
  } else {
    // HS2: a henchman found on its own - out of stealth until the player hides again.
    sprintf_s(code,
              "object o = StringToObject(\"%x\");"
              "if (GetIsEnemy(o)) {"
              " SendMessageToPC(GetMaster(OBJECT_SELF), \"nwn5e: \" + GetName(o) + \" %s: \" + GetName(OBJECT_SELF)"
              "  + \"%s\");"
              " %s"
              "}",
              IdOf(observer), how, ends ? " is found - out of hiding" : " is found", leave);
  }
  RunScriptChunk(code, IdOf(hider));
  if (ends) {
    const bool hostile = engine::GetLocalInt(hider, "nwn5e_found_exit") != 0;
    engine::SetLocalInt(hider, "nwn5e_found_exit", 0);
    if (LogSlot()) {
      Log("stealth: 0x%08X seen by 0x%08X - %s", IdOf(hider), IdOf(observer),
          hostile ? "a hostile: out of stealth mode (found_ends_hiding)" : "not hostile to it: stays hidden");
    }
  }
}

int Detection(DetectionFn original, bool spot, void* observer, void* target, int flag) {
  void* outer_observer = t_observer;
  void* outer_target = t_target;
  const bool outer_scored = t_scored;
  t_observer = observer;
  t_target = target;
  t_scored = false;
  int result = original(observer, target, flag);
  const bool scored = t_scored;
  t_observer = outer_observer;
  t_target = outer_target;
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
  if (spot && result && LightTerm(observer, target) == kUnseen) {  // G: darkness, no darkvision
    result = 0;
    sprintf_s(up_close, "; in darkness: unseen");
  }
  bool close = false;
  if (spot && !result && scored && g_config.found_up_close && FoundUpClose(observer, target, &metres)) {
    result = 1;
    close = true;
    sprintf_s(up_close, "; found up close: %.1f m in front, clear line", metres);
  }
  if (spot && g_config.hiders_need_search && (searching || in_cone || result) && PartyHider(target)) {
    TellCheck(observer, target, result, searching, close);
  }
  // #133: where it heard a hider - a search goes there (UnseenHiderSearch).
  if (!spot && result && Hiding(target)) {
    g_heard_at[static_cast<uint64_t>(IdOf(observer)) << 32 | IdOf(target)] = {
        Field<float>(target, ee::kObjectPositionX), Field<float>(target, ee::kObjectPositionY),
        Field<float>(target, ee::kObjectPositionZ)};
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

// #136: CNWSCreature::AddMoveToPointAction - a script's ActionMoveToLocation (ExecuteCommandMoveToPoint's call) onto a
// hider the mover doesn't see becomes #133's search (UnseenHiderMove, below). The Vector comes by pointer.
using AddMoveToPointFn = int (*)(void* creature, uint16_t group, const float* position, uint32_t area, uint32_t target,
                                 int run, float range, float timeout, int a, int b, int c, int d, int e, int f);
constexpr const char* kAddMoveToPointAction = "?AddMoveToPointAction@CNWSCreature@@QEAAHGVVector@@IIHMMHHHHHH@Z";
constexpr const char* kExecuteCommandMoveToPoint = "?ExecuteCommandMoveToPoint@CNWSVirtualMachineCommands@@QEAAHHH@Z";
AddMoveToPointFn g_add_move_to_point = nullptr;
uintptr_t g_script_move_site = 0;  // ExecuteCommandMoveToPoint's call (its return address)

}  // namespace

bool UnseenHiderMove(void* creature, const float* to);  // below

namespace {

int AddMoveToPoint(void* creature, uint16_t group, const float* position, uint32_t area, uint32_t target, int run,
                   float range, float timeout, int a, int b, int c, int d, int e, int f) {
  if (reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_script_move_site && creature && position &&
      UnseenHiderMove(creature, position)) {
    return 1;  // a search instead
  }
  return g_add_move_to_point(creature, group, position, area, target, run, range, timeout, a, b, c, d, e, f);
}

}  // namespace

void InstallPerceptionRules() {
  if (g_config.perception != rules::RuleVariant::kFaithful) return;
  if (g_config.stealth_light) ServeConditionalRows("light2024");  // EE's night terms zeroed: G replaces them
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
  // #129 (0.66.7): a hide checked for at once - the engine's re-check forced for it.
  g_perception_update_code = FunctionAt(EngineExport(kDoPerceptionUpdateOnCreature));
  if (!g_perception_update_code.Valid() || !AddFeatFilter(&ForcedRecheck)) {
    Log("stealth: a hide's at-once check NOT forced (no perception update code or HasFeat filter) - "
        "it waits for each creature's next update");
  }
  // #129 B: an attack from hiding rolled hidden, the hide ending after it.
  HookExport("CNWSCreature::ActivityManager (an attack leaves stealth mode until its roll is made)", kActivityManager,
             reinterpret_cast<void*>(&ActivityManager), reinterpret_cast<void**>(&g_activity_manager));
  HookExport("CNWSCreature::ResolveAttack (an attack from hiding ends the hide after its roll)", kResolveAttack,
             reinterpret_cast<void*>(&ResolveAttack), reinterpret_cast<void**>(&g_resolve_attack));
  // 2026-11-09 (A): a spell without a Verbal component cast from hiding keeps the hide.
  if (!HookCastAction() || !OnRulesLoaded(&ReadSpellComponents)) {
    Log("SKIP stealth: the cast action or the rules load isn't hooked - every cast ends stealth mode");
  }
  // #132: a grenade thrown from hiding keeps the hide.
  HookExport("CNWSCreature::AIActionItemCastSpell (a grenade thrown from hiding keeps the hide)", kAIActionItemCastSpell,
             reinterpret_cast<void*>(&AIActionItemCastSpell), reinterpret_cast<void**>(&g_item_cast_spell));
  // #136: a script's move onto a hider the mover doesn't see (NWN's OnDamaged) becomes #133's search.
  const uintptr_t move_command = EngineExport(kExecuteCommandMoveToPoint);
  const uintptr_t add_move = EngineExport(kAddMoveToPointAction);
  const std::vector<uintptr_t> move_calls =
      move_command && add_move ? FindCallsTo(FunctionAt(move_command), add_move) : std::vector<uintptr_t>();
  if (move_calls.size() == 1) {
    g_script_move_site = move_calls[0] + 5;
    HookExport("CNWSCreature::AddMoveToPointAction (a script's move onto an unseen hider becomes a search, #136)",
               kAddMoveToPointAction, reinterpret_cast<void*>(&AddMoveToPoint),
               reinterpret_cast<void**>(&g_add_move_to_point));
  } else {
    Log("SKIP stealth #136: a script's move onto an unseen hider isn't turned (%zu calls found)", move_calls.size());
  }
  AddSkillAdjuster(&PerceptionSkill);
  AddDiceFilter(&StealthDice);
  Log("perception: Stealth once per hide, passive Perception unless searching, no size / combat / Favored Enemy "
      "terms (faithful)");
}

// #133 (owner, 2026-11-04: "go with B"; rules::UnseenHiderSearched): an AI creature's attack on a hider it doesn't
// see becomes a search - it runs to where it last heard the hider (else where the hider is as it attacks: a grenade's
// thrower, where the throw came from), in search mode, looks about and waits; NWN's AI asks again (its combat round)
// and it goes on searching until it sees the hider - found ends the hide (#129) and the attack goes ahead. True: the
// attack is dropped (action_order.cpp's AIActionAttackObject hook). The search is queued by DelayCommand, outside the
// engine's action processing; at most once a second per creature.
// #133 / #136: the search itself - `instead` names what it replaces (the log's "attack", "run to").
bool SearchForUnseenHider(void* creature, void* hider, const char* instead) {
  const uint32_t target = IdOf(hider);
  const uint32_t id = IdOf(creature);
  const int64_t now = NowMs();
  int64_t& last = g_hunt_at[id];
  if (now - last < 1000) return true;  // searching already: the attack dropped
  last = now;
  const auto heard = g_heard_at.find(static_cast<uint64_t>(id) << 32 | target);
  const HeardAt at = heard != g_heard_at.end()
                         ? heard->second
                         : HeardAt{Field<float>(hider, ee::kObjectPositionX), Field<float>(hider, ee::kObjectPositionY),
                                   Field<float>(hider, ee::kObjectPositionZ)};
  char code[900];
  sprintf_s(code,
            "location at = Location(GetArea(OBJECT_SELF), Vector(%.2f, %.2f, %.2f), GetFacing(OBJECT_SELF));"
            "DelayCommand(0.10, SetActionMode(OBJECT_SELF, ACTION_MODE_DETECT, TRUE));"
            "DelayCommand(0.11, ActionMoveToLocation(at, TRUE));"
            "DelayCommand(0.12, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_LEFT));"
            "DelayCommand(0.13, ActionPlayAnimation(ANIMATION_FIREFORGET_HEAD_TURN_RIGHT));"
            "DelayCommand(0.14, ActionWait(3.0));",
            at.x, at.y, at.z);
  g_search_move[id] = {at.x, at.y, at.z, now};  // #136: its own move
  RunScriptChunk(code, id);
  int64_t& logged = g_hunt_logged[id];
  if (now - logged > 6000 && LogSlot()) {
    logged = now;
    const float dx = at.x - Field<float>(hider, ee::kObjectPositionX);
    const float dy = at.y - Field<float>(hider, ee::kObjectPositionY);
    Log("stealth: 0x%08X would %s 0x%08X unseen - it searches where it %s instead (#133/#136; %.1f m from the "
        "hider now)", id, instead, target, heard != g_heard_at.end() ? "last heard it" : "was hit from",
        sqrtf(dx * dx + dy * dy));
  }
  return true;
}

bool UnseenHiderSearch(void* creature, void* node) {
  if (!creature || !node) return false;
  const uint32_t target = Field<uint32_t>(node, ee::kActionFirstParam);
  void* hider = target != ee::kObjectInvalid ? CreatureById(target) : nullptr;
  if (!hider || hider == creature ||
      !rules::UnseenHiderSearched(g_config.perception == rules::RuleVariant::kFaithful, IsPlayer(creature),
                                  Hiding(hider), engine::Sees(creature, target))) {
    return false;
  }
  return SearchForUnseenHider(creature, hider, "attack");
}

// #136 B (owner, 2026-11-06: "Go with your recommendations for 1 and 4."; rules::MoveOntoUnseenHider): a script's
// move order onto a hider the mover doesn't see - NWN's OnDamaged (nw_c2_default6) runs a creature that can't see its
// damager to the damager's exact location, a hider's grenade or its lingering patch - becomes #133's search. Its own
// search's move (the spot it went to, the last 3 s) is let through. True: the move is dropped.
namespace {
struct MoveCheck {
  void* mover;
  const float* to;
  void* hider;
  float best;
};

void FindUnseenHider(void* context, uint32_t id, void* other) {
  auto* check = static_cast<MoveCheck*>(context);
  if (!other || !Hiding(other) || !engine::IsEnemy(check->mover, id)) return;
  const float dx = check->to[0] - Field<float>(other, ee::kObjectPositionX);
  const float dy = check->to[1] - Field<float>(other, ee::kObjectPositionY);
  const float distance = sqrtf(dx * dx + dy * dy);
  if (distance >= check->best ||
      !rules::MoveOntoUnseenHider(g_config.perception == rules::RuleVariant::kFaithful, IsPlayer(check->mover), true,
                                  engine::Sees(check->mover, id), distance)) {
    return;
  }
  check->best = distance;
  check->hider = other;
}
}  // namespace

bool UnseenHiderMove(void* creature, const float* to) {
  if (IsPlayer(creature)) return false;
  const auto own = g_search_move.find(IdOf(creature));
  if (own != g_search_move.end() && NowMs() - own->second.at < 3000) {
    const float dx = to[0] - own->second.x;
    const float dy = to[1] - own->second.y;
    if (dx * dx + dy * dy < 0.25f) return false;  // its own search's move
  }
  MoveCheck check{creature, to, nullptr, 1e9f};
  engine::ForEachOtherCreatureInArea(creature, &FindUnseenHider, &check);
  return check.hider && SearchForUnseenHider(creature, check.hider, "run to");
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
// minus the hider's kept Stealth total. No size term (0.35.0, RB3): the engine
// adds one to the observer's score and PerceptionSkill cancels it on the
// hider's side, so the check weighs none - 0.34.0 added it on the hider's side
// only, 4 or 8 points off against a smaller or larger observer. Over 0: inside
// its cone it sees the hider. False when there's nothing to weigh.
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
  if (LightRule()) {
    const int light = LightTerm(observer, hider);  // G replaces EE's night terms (zeroed in ruleset.2da)
    if (light == kUnseen) {
      *margin = -100;
      return true;
    }
    score += light + CarriedLightTerm(hider);
  } else if (Night()) {
    if (!Lit(observer, true)) score -= 5;
    if (Lit(hider, false)) score += 10;
  }
  const int stealth = kept->second + static_cast<int8_t>(E().skill_rank(hid, ee::kSkillHide, nullptr, 0));
  *margin = score - stealth;
  return true;
}

// G (stealth_watch.cpp): a light it carries itself - a torch, a light item, the Light spell.
bool CarriesLight(void* creature) { return creature && Lit(creature, false); }
int CarriedLightRadius(void* creature) { return creature ? CarriedLightRadiusOf(creature) : 0; }

// Search's die (rules_skill_dcs.cpp), the 2024 way: a d20 when searching, else passive.
int SearchDie(void* searcher, bool worse) {
  if (Field<uint8_t>(searcher, ee::kCreatureDetectMode) == 0) return rules::PassiveCheckBase(worse);
  const int first = RollEngineDice(1, 20);
  const int kept = rules::KeptCheckD20(first, worse ? RollEngineDice(1, 20) : 0, 0, worse ? 1 : 0);
  return ReliableTalentRoll(searcher, ee::kSkillSearch, kept);  // Rogue R9
}
