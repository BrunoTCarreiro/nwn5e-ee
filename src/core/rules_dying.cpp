// Phase 5b (docs/PLAN.md "Phase 5 — rests, healing, death", decided by the
// owner 2026-10-23), `dying` faithful:
// - D1 Death saves. A player character dropping to 0 HP with a henchman
//   standing by it (in its party and its area, not down) lies unconscious at
//   0 HP in the engine's dying state - the module's OnPlayerDying (the
//   campaigns' instant death, nw_o0_dying) is skipped for it - and makes a
//   Death Saving Throw every 6 s (rules/death_saves.h): 10+ a success, a
//   natural 1 two failures, a natural 20 wakes it at 1 HP; 3 successes Stable,
//   3 failures dead (the module's death panel and respawn). Damage at 0 HP is a
//   failure, two from a critical hit; damage past 0 - or taken at 0 - of at
//   least the HP maximum kills outright. It wakes at 1 HP when the fight is
//   over (FightWatch: no living enemy within 30 m for a round, 6 s) and dies
//   when no henchman is left standing, Stable or not. Any healing wakes it
//   (the engine's own revive).
//   Without a henchman: NWN's (the module's OnPlayerDying).
// - D2 Henchmen, SoU's handling in every campaign: at 0 HP a player's
//   henchman lies at 1 HP - plot, the looping dead animation, uncommandable -
//   and gets up only when healed above 1 HP: a spell, a healer's kit, a Short
//   Rest (its Hit Dice) - the player gets it up after the fight (owner,
//   2026-10-23: "the player needs to get them up manually after the fight").
//   The campaign's OnDeath (OC's temple, HotU's corpse) no longer runs for
//   damage; death magic still kills.
// - Temple: on respawn (the module's OnPlayerRespawn) every henchman joins the
//   player at full health once it has landed; a Long Rest in a temple's area
//   (the campaigns' respawn points NW_DEATH_TEMPLE, NW_DEATH_PLACE,
//   X1_TPORT_LOC, SoU's X0_RESPAWN_LOC, or where the player last respawned)
//   brings every henchman who is down, dead or left behind, at full health.
// - D5 Exhaustion: a level each time the player comes back from 0 HP; each
//   level -2 to attack rolls, saves and checks (one linked supernatural
//   effect - attack, save and skill decreases, which the 2024 rolls keep) and
//   -5 ft speed; level 6 is death. A Long Rest removes every level, and so
//   does a respawn. The level is the record nwn5e_exhaustion.
// - D4 Defy Death (Champion 18, class_features faithful): advantage on death
//   saves, an 18-20 counts as a 20.
// `!dying` shows the state; with [debug] test_commands, `!dying roll <d20>`,
// `!dying hurt <HP> [henchman]`, `!dying exhaustion <level>` and
// `!dying henchman [blueprint]` (one joins the party - e.g. in the Prelude,
// which has none) set up tests.
// NWN mode: NWN's dying (the module's scripts), no exhaustion.

#include <windows.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "attack_roll.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "rest.h"
#include "rules/death_saves.h"

namespace {

constexpr const char* kVarExhaustion = "nwn5e_exhaustion";
constexpr const char* kVarDown = "nwn5e_down";  // a local: a down player or henchman, kept by a saved game
constexpr const char* kVarInTemple = "nwn5e_in_temple";
constexpr float kFightRange = 30.0f;            // the Rest button's REST_ENEMY_CHECK_DISTANCE
constexpr uint64_t kDeathSaveMs = 6000;         // one round
constexpr uint64_t kPassMs = 100;
constexpr uint64_t kLongestStepMs = 1000;       // a longer gap is a pause or a load: not counted
constexpr uint64_t kCriticalWindowMs = 3000;    // an attack's critical roll to its damage
constexpr uint64_t kRetryMs = 3000;             // a heal or a death on its way that didn't land
constexpr uint64_t kSummonDelayMs = 4000;       // after a respawn: the campaign's own jump (HotU's: 2 s) first
constexpr uint64_t kSummonGiveUpMs = 60000;
constexpr uint64_t kUpkeepMs = 2000;
constexpr uint64_t kFightOverMs = 6000;    // no enemy within 30 m for a round
constexpr uint64_t kUnengagedMs = 12000;   // enemies near, none in touch with the party
constexpr int kUpkeepsPerExhaustionCheck = 3;
constexpr int kMaxHenchmen = 16;

using DoDamageFn = void (*)(void* creature, int damage);
using RunEventScriptFn = int (*)(void* module, int script, void* name);
using AssociateIdFn = uint32_t (*)(void* creature, uint16_t type, int nth);
using MaxHitPointsFn = int16_t (*)(void* creature, int include_temporary);

DoDamageFn g_do_damage = nullptr;
RunEventScriptFn g_run_event_script = nullptr;
AssociateIdFn g_associate_id = nullptr;
MaxHitPointsFn g_max_hit_points = nullptr;

// The fight around someone down (owner, 2026-10-23: "no living hostile within
// 30 m"). It goes on while a living enemy is within 30 m, and is over after a
// round (6 s) with none - so one still closing in, or not yet spotted, keeps it
// going (0.19.1 woke a player 36 ms after the drop: the enemy coming had not
// seen anyone yet). Enemies near that never touch the party - none sees one of
// it or is seen - end it after 12 s, so a caged or sleeping monster nearby
// can't keep anyone down for good. Players only: a down henchman waits to be
// healed. And it goes on while someone of the party
// still up is attacking a living creature, however far (owner, 2026-10-23: "a
// good check is 'if the henchman is actively fighting' as well").
struct FightWatch {
  uint64_t last_near = 0;
  uint64_t last_engaged = 0;
  void Start(uint64_t now) { last_near = last_engaged = now; }
};

struct DownPlayer {
  rules::DeathSaves saves;
  FightWatch fight;
  void* area = nullptr;
  uint64_t last_tick = 0;
  uint64_t elapsed_ms = 0;
  uint64_t pending_at = 0;  // a heal (waking) or a death on its way
  bool pending_death = false;
};
struct DownHenchman {
  bool laid_down = false;
};
struct PlayerUpkeep {
  uint64_t next = 0;
  int count = 0;
};

std::unordered_map<uint32_t, DownPlayer> g_down_players;
std::unordered_map<uint32_t, DownHenchman> g_down_henchmen;
std::unordered_map<uint32_t, uint64_t> g_critical_hits;  // a down player -> when a critical hit last struck it
std::unordered_map<uint32_t, uint64_t> g_respawned;      // player -> when it respawned (its henchmen to summon)
// player -> when it last dropped into death saves: its OnPlayerDying (a queued
// module event) is skipped even if it is already back up when the event runs.
std::unordered_map<uint32_t, uint64_t> g_recent_drops;
constexpr uint64_t kRecentDropMs = 5000;
std::unordered_map<uint32_t, PlayerUpkeep> g_upkeep;
std::vector<std::pair<uint32_t, std::string>> g_lines;   // told at the next pass (not inside the damage)
uint64_t g_last_pass = 0;
int g_forced_roll = 0;  // !dying roll

bool Faithful() { return g_config.dying == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
int HitPoints(const void* creature) { return Field<int16_t>(creature, ee::kObjectCurrentHitPoints); }
void SetHitPoints(void* creature, int value) { SetField<int32_t>(creature, ee::kObjectCurrentHitPoints, value); }
int MaxHitPoints(void* creature) { return g_max_hit_points ? g_max_hit_points(creature, 1) : HitPoints(creature); }
bool IsPlayer(const void* creature) { return Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0; }

void Tell(uint32_t player, const std::string& line) { TellPlayer(player, "nwn5e: " + line); }
void Later(uint32_t player, const std::string& line) { g_lines.emplace_back(player, "nwn5e: " + line); }

// The player character a henchman serves (0 if it is no player's henchman).
uint32_t PlayerMasterOf(void* creature) {
  if (Field<uint16_t>(creature, ee::kCreatureAssociateType) != ee::kAssociateTypeHenchman) return 0;
  const uint32_t master_id = Field<uint32_t>(creature, ee::kCreatureMaster);
  if (!master_id || master_id == ee::kObjectInvalid) return 0;
  void* master = CreatureById(master_id);
  return master && IsPlayer(master) ? master_id : 0;
}

std::vector<uint32_t> HenchmenOf(void* player) {
  std::vector<uint32_t> ids;
  for (int nth = 1; g_associate_id && nth <= kMaxHenchmen; ++nth) {
    const uint32_t id = g_associate_id(player, static_cast<uint16_t>(ee::kAssociateTypeHenchman), nth);
    if (!id || id == ee::kObjectInvalid) break;
    ids.push_back(id);
  }
  return ids;
}

bool IsDownHenchman(uint32_t id, void* henchman) {
  return g_down_henchmen.count(id) || engine::GetLocalInt(henchman, kVarDown) != 0;
}

// A henchman still fighting at the player's side: in its area, alive, not down.
int StandingHenchmen(void* player) {
  void* area = engine::AreaOf(player);
  int standing = 0;
  for (uint32_t id : HenchmenOf(player)) {
    void* henchman = CreatureById(id);
    if (henchman && engine::AreaOf(henchman) == area && !engine::IsDeadOrDying(henchman) &&
        HitPoints(henchman) > 0 && !IsDownHenchman(id, henchman)) {
      ++standing;
    }
  }
  return standing;
}

// The living enemies within 30 m of `center`, and whether one of them is in
// touch with the party (the player and its henchmen): it sees one, or one sees it.
struct FightScan {
  int enemies = 0;  // living enemies within 30 m
  bool engaged = false;
  uint32_t nearest = 0;
  float nearest_distance = 0.0f;
  uint32_t fighter = 0;  // a party member still up attacking a living creature
  uint32_t fighting = 0; // its target
};

// Someone of the party (not `center`) up and attacking a living creature: its
// attack target (GetAttackTarget's field), alive and in its sight. Down
// henchmen and the dead keep a stale target, so they don't count; nor does a
// target that fled out of sight (the field may outlive the attack).
void LookForFighter(FightScan& scan, uint32_t center_id, uint32_t id, void* member) {
  if (scan.fighter || id == center_id || !member || engine::IsDeadOrDying(member) || HitPoints(member) < 1) return;
  if (!IsPlayer(member) && IsDownHenchman(id, member)) return;
  const uint32_t target_id = Field<uint32_t>(member, ee::kCreatureAttackTarget);
  if (!target_id || target_id == ee::kObjectInvalid) return;
  void* target = CreatureById(target_id);
  if (!target || engine::IsDeadOrDying(target) || !engine::Sees(member, target_id)) return;
  scan.fighter = id;
  scan.fighting = target_id;
}

struct FightSearch {
  void* center;
  uint32_t center_id;
  std::vector<std::pair<uint32_t, void*>> party;
  FightScan scan;
};

void LookForFight(void* context, uint32_t id, void* other) {
  auto* search = static_cast<FightSearch*>(context);
  if (!other || engine::IsDeadOrDying(other)) return;
  if (!engine::IsEnemy(search->center, id) && !engine::IsEnemy(other, search->center_id)) return;
  const float distance = engine::EdgeDistance(search->center, other);
  if (distance > kFightRange) return;
  FightScan& scan = search->scan;
  if (!scan.enemies || distance < scan.nearest_distance) {
    scan.nearest = id;
    scan.nearest_distance = distance;
  }
  ++scan.enemies;
  if (scan.engaged) return;
  for (const auto& member : search->party) {
    if (engine::Sees(other, member.first) || (member.second && engine::Sees(member.second, id))) {
      scan.engaged = true;
      return;
    }
  }
}

FightScan ScanFight(void* center, void* player) {
  FightSearch search{center, IdOf(center), {}, {}};
  for (uint32_t id : HenchmenOf(player)) search.party.emplace_back(id, CreatureById(id));
  search.party.emplace_back(IdOf(player), player);
  engine::ForEachOtherCreatureInArea(center, &LookForFight, &search);
  for (const auto& member : search.party) LookForFighter(search.scan, search.center_id, member.first, member.second);
  return search.scan;
}

std::string ScanText(const FightScan& scan) {
  char text[240];
  if (!scan.enemies) {
    sprintf_s(text, "no enemy within 30 m");
  } else {
    sprintf_s(text, "%d enemies within 30 m (nearest 0x%08X at %.1f m), %s", scan.enemies, scan.nearest,
              scan.nearest_distance, scan.engaged ? "in touch with the party" : "none in touch with the party");
  }
  std::string line = text;
  if (scan.fighter) {
    sprintf_s(text, "; 0x%08X is attacking 0x%08X", scan.fighter, scan.fighting);
    line += text;
  }
  return line;
}

// Watches the fight around `center`; why it is over, or nullptr while it goes on.
const char* FightOver(void* center, void* player, FightWatch& watch, uint64_t now, FightScan* scan_out) {
  const FightScan scan = ScanFight(center, player);
  if (scan.enemies || scan.fighter) watch.last_near = now;
  if (scan.engaged || scan.fighter) watch.last_engaged = now;
  if (scan_out) *scan_out = scan;
  if (now - watch.last_near >= kFightOverMs) return "no enemy within 30 m for 6 s";
  if (now - watch.last_engaged >= kUnengagedMs) return "the enemies near haven't been in touch with the party for 12 s";
  return nullptr;
}

void Kill(uint32_t id) { ApplyEffectAs(id, id, "DURATION_TYPE_INSTANT", "EffectDeath()", 0.0f, 0.0f); }
void Heal(uint32_t id, int hit_points) {
  ApplyEffectAs(id, id, "DURATION_TYPE_INSTANT", ("EffectHeal(" + std::to_string(hit_points) + ")").c_str(), 0.0f,
                0.0f);
}

// --- Exhaustion -------------------------------------------------------------------

int ExhaustionOf(void* creature) {
  EnsureRecords(creature);  // C6: an exported character's, from its database
  return std::clamp(engine::GetLocalInt(creature, kVarExhaustion), 0, 6);
}

std::string ExhaustionText(int level) {
  return "-" + std::to_string(2 * level) + " to attack rolls, saving throws and checks, -" + std::to_string(5 * level) +
         " ft speed";
}

// The one effect that carries a level (tagged nwn5e_exhaustion_<level>): any
// other level's removed, this one applied if missing - so a script or a spell
// that strips it only takes it away until the next check.
void SyncExhaustionEffect(uint32_t id, int level) {
  const int penalty = -rules::ExhaustionD20Penalty(level);
  char code[1400];
  sprintf_s(code,
            "string want = \"nwn5e_exhaustion_%d\"; effect e = GetFirstEffect(OBJECT_SELF);"
            "while (GetIsEffectValid(e)) { string t = GetEffectTag(e);"
            " if (GetStringLeft(t, 16) == \"nwn5e_exhaustion\" && t != want) RemoveEffect(OBJECT_SELF, e);"
            " e = GetNextEffect(OBJECT_SELF); }"
            "if (%d <= 0) return; int have = FALSE; e = GetFirstEffect(OBJECT_SELF);"
            "while (GetIsEffectValid(e)) { if (GetEffectTag(e) == want) have = TRUE; e = GetNextEffect(OBJECT_SELF); }"
            "if (have) return;"
            "effect x = EffectLinkEffects(EffectAttackDecrease(%d), EffectSavingThrowDecrease(SAVING_THROW_ALL, %d));"
            "x = EffectLinkEffects(x, EffectSkillDecrease(SKILL_ALL_SKILLS, %d));"
            "x = EffectLinkEffects(x, EffectMovementSpeedDecrease(%d));"
            "ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(x), want), OBJECT_SELF);",
            level, level, penalty, penalty, penalty, rules::ExhaustionSpeedDecreasePercent(level));
  RunScriptChunk(code, id);
}

void SetExhaustion(void* player, int level) {
  SetRecord(player, kVarExhaustion, level);
  SyncExhaustionEffect(IdOf(player), rules::IsDeadFromExhaustion(level) ? 0 : level);
}

void ClearExhaustion(void* player, const char* why) {
  const int level = ExhaustionOf(player);
  if (level == 0) return;
  SetExhaustion(player, 0);
  Log("dying: 0x%08X %s - Exhaustion %d removed", IdOf(player), why, level);
  Tell(IdOf(player), std::string(why) + ": your Exhaustion is gone.");
}

void GainExhaustion(void* player) {
  const uint32_t id = IdOf(player);
  const int level = std::min(ExhaustionOf(player) + 1, 6);
  SetExhaustion(player, level);
  Log("dying: 0x%08X back from 0 HP - Exhaustion %d", id, level);
  if (rules::IsDeadFromExhaustion(level)) {
    Tell(id, "Coming back from 0 HP once more: Exhaustion 6 - you die.");
    Kill(id);
    return;
  }
  Tell(id, "Coming back from 0 HP leaves you exhausted: Exhaustion " + std::to_string(level) + " (" +
               ExhaustionText(level) + "; 6 is death). A Long Rest removes it.");
}

// --- A player at 0 HP --------------------------------------------------------------

std::string Counts(const rules::DeathSaves& saves) {
  if (saves.stable) return "Stable";
  return std::to_string(saves.successes) + (saves.successes == 1 ? " success, " : " successes, ") +
         std::to_string(saves.failures) + (saves.failures == 1 ? " failure" : " failures");
}

class SaveDice : public rules::Dice {
 public:
  explicit SaveDice(int forced) : forced_(forced) {}
  int Roll(int sides) override { return forced_ >= 1 && forced_ <= sides ? forced_ : RollEngineDice(1, sides); }

 private:
  int forced_;
};

bool HasDefyDeath(void* player) {
  void* stats = StatsOf(player);
  return stats && g_config.class_features == rules::RuleVariant::kFaithful &&
         rules::HasDefyDeath(ClassLevel2024(stats, rules::CharacterClass::kFighter));
}

void RollSave(void* player, uint32_t id, DownPlayer& state, uint64_t now) {
  const bool defy = HasDefyDeath(player);
  const int exhaustion = ExhaustionOf(player);
  SaveDice dice(g_forced_roll);
  g_forced_roll = 0;
  const rules::DeathSave save =
      rules::RollDeathSave(state.saves, defy ? rules::RollMode::kAdvantage : rules::RollMode::kNormal,
                           rules::ExhaustionD20Penalty(exhaustion), defy ? rules::kDefyDeathTwentyFrom : 20, dice);
  std::string line = "Death saving throw: d20 " + std::to_string(save.roll.kept);
  if (defy) line += " (" + std::to_string(save.roll.first) + " / " + std::to_string(save.roll.second) + ", Defy Death)";
  if (exhaustion > 0) {
    line += " -" + std::to_string(2 * exhaustion) + " (Exhaustion " + std::to_string(exhaustion) + ") = " +
            std::to_string(save.total);
  }
  line += " - ";
  switch (save.result) {
    case rules::DeathSaveResult::kSuccess:
      line += "a success (" + Counts(state.saves) + ").";
      break;
    case rules::DeathSaveResult::kFailure:
      line += std::string(save.roll.kept == 1 ? "a natural 1, two failures" : "a failure") + " (" +
              Counts(state.saves) + ").";
      break;
    case rules::DeathSaveResult::kStable:
      line += "a third success: you are Stable - unconscious until you're healed or the fight is over.";
      break;
    case rules::DeathSaveResult::kDead:
      line += std::string(save.roll.kept == 1 ? "a natural 1: " : "") + "a third failure - you die.";
      Kill(id);
      state.pending_at = now;
      state.pending_death = true;
      break;
    case rules::DeathSaveResult::kWake:
      line += std::string(save.roll.kept >= 20 ? "a natural 20" : "18-20 with Defy Death") +
              ": you regain 1 HP and wake up.";
      Heal(id, 1);
      state.pending_at = now;
      state.pending_death = false;
      break;
  }
  Log("dying: 0x%08X death save %d%s (total %d) -> %s", id, save.roll.kept, defy ? " (advantage)" : "", save.total,
      Counts(state.saves).c_str());
  Tell(id, line);
}

// One down player's pass. False when its time at 0 HP is over.
bool UpdateDownPlayer(uint32_t id, DownPlayer& state, uint64_t now) {
  void* player = CreatureById(id);
  if (!player || engine::AreaOf(player) != state.area) {  // gone, or a saved game loaded
    if (player) engine::SetLocalInt(player, kVarDown, 0);
    return false;
  }
  const int hit_points = HitPoints(player);
  if (hit_points <= ee::kHitPointsDead) {  // dead (a third failure, the last henchman, massive damage)
    engine::SetLocalInt(player, kVarDown, 0);
    Log("dying: 0x%08X dead", id);
    return false;
  }
  if (hit_points > 0) {  // healed, a natural 20, the fight's end
    engine::SetLocalInt(player, kVarDown, 0);
    Log("dying: 0x%08X back up at %d HP", id, hit_points);
    Tell(id, "You're back up at " + std::to_string(hit_points) + " HP.");
    GainExhaustion(player);
    return false;
  }
  if (state.pending_at) {
    if (now - state.pending_at < kRetryMs) return true;
    state.pending_at = 0;  // it didn't land: decide again
    if (state.pending_death) {
      state.pending_death = false;
      Kill(id);
      state.pending_at = now;
      state.pending_death = true;
      return true;
    }
  }
  if (StandingHenchmen(player) == 0) {
    Tell(id, "No henchman is left standing - you die.");
    Log("dying: 0x%08X no henchman standing - dies", id);
    Kill(id);
    state.pending_at = now;
    state.pending_death = true;
    return true;
  }
  FightScan scan;
  if (const char* over = FightOver(player, player, state.fight, now, &scan)) {
    Tell(id, "The fight is over - you come to at 1 HP.");
    Log("dying: 0x%08X the fight is over (%s; now %s) - wakes", id, over, ScanText(scan).c_str());
    Heal(id, 1);
    state.pending_at = now;
    return true;
  }
  const uint64_t step = now - state.last_tick;
  state.last_tick = now;
  if (step <= kLongestStepMs) state.elapsed_ms += step;
  if (state.saves.stable || state.elapsed_ms < kDeathSaveMs) return true;
  state.elapsed_ms -= kDeathSaveMs;
  RollSave(player, id, state, now);
  return true;
}

// --- Henchmen ----------------------------------------------------------------------

// Down: plot (no more damage; its own flag kept), the "dead" pose, uncommandable.
constexpr const char* kLayDownScript =
    "SetLocalInt(OBJECT_SELF, \"nwn5e_down\", 1);"
    "SetLocalInt(OBJECT_SELF, \"nwn5e_down_plot\", GetPlotFlag(OBJECT_SELF));"
    "SetPlotFlag(OBJECT_SELF, TRUE); SetCommandable(TRUE, OBJECT_SELF); ClearAllActions(TRUE);"
    "PlayVoiceChat(VOICE_CHAT_HEALME, OBJECT_SELF);"
    "ActionPlayAnimation(ANIMATION_LOOPING_DEAD_FRONT, 1.0, 100000.0); SetCommandable(FALSE, OBJECT_SELF);"
    "object m = GetMaster(OBJECT_SELF); if (GetIsPC(m)) SendMessageToPC(m, \"nwn5e: \" + GetName(OBJECT_SELF) + "
    "\" is down - heal them (a spell, a healer's kit) or take a Short Rest after the fight to get them up.\");";

constexpr const char* kGetUpScript =
    "SetCommandable(TRUE, OBJECT_SELF);"
    "SetPlotFlag(OBJECT_SELF, GetLocalInt(OBJECT_SELF, \"nwn5e_down_plot\"));"
    "DeleteLocalInt(OBJECT_SELF, \"nwn5e_down\"); DeleteLocalInt(OBJECT_SELF, \"nwn5e_down_plot\");"
    "ClearAllActions(TRUE);"
    "object m = GetMaster(OBJECT_SELF); if (GetIsPC(m)) SendMessageToPC(m, \"nwn5e: \" + GetName(OBJECT_SELF) + "
    "\" gets back up.\");";

// To the player at full health: up if down, raised if dead, jumped if elsewhere.
constexpr const char* kSummonScript =
    "object pc = StringToObject(\"%x\");"
    "SetCommandable(TRUE, OBJECT_SELF);"
    "if (GetLocalInt(OBJECT_SELF, \"nwn5e_down\")) { SetPlotFlag(OBJECT_SELF, GetLocalInt(OBJECT_SELF, "
    "\"nwn5e_down_plot\")); DeleteLocalInt(OBJECT_SELF, \"nwn5e_down\"); DeleteLocalInt(OBJECT_SELF, "
    "\"nwn5e_down_plot\"); }"
    "if (GetIsDead(OBJECT_SELF)) ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectResurrection(), OBJECT_SELF);"
    "ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectHeal(GetMaxHitPoints(OBJECT_SELF)), OBJECT_SELF);"
    "ClearAllActions(TRUE);"
    "if (GetArea(OBJECT_SELF) != GetArea(pc) || GetDistanceBetween(OBJECT_SELF, pc) > 10.0) JumpToObject(pc);"
    "SendMessageToPC(pc, \"nwn5e: \" + GetName(OBJECT_SELF) + \" joins you at full health.\");";

// One down henchman's pass. False when it is up (or gone): healed above 1 HP
// only - the fight's end doesn't get it up.
bool UpdateDownHenchman(uint32_t id, DownHenchman& state) {
  void* henchman = CreatureById(id);
  if (!henchman) return false;
  if (engine::IsDeadOrDying(henchman)) {  // death magic: the campaign's OnDeath has it
    Log("dying: henchman 0x%08X died while down", id);
    return false;
  }
  if (!state.laid_down) {
    RunScriptChunk(kLayDownScript, id);
    state.laid_down = true;
    return true;
  }
  const uint32_t master_id = PlayerMasterOf(henchman);
  void* master = master_id ? CreatureById(master_id) : nullptr;
  const char* why = !master ? "no longer a player's henchman" : HitPoints(henchman) > 1 ? "healed" : nullptr;
  if (!why) return true;
  RunScriptChunk(kGetUpScript, id);
  Log("dying: henchman 0x%08X up at %d HP (%s)", id, HitPoints(henchman), why);
  return false;
}

// The player's henchmen to it at full health: all of them, or (all false)
// those who are down, dead or in another area. How many came.
int SummonHenchmen(void* player, bool all) {
  const uint32_t player_id = IdOf(player);
  void* area = engine::AreaOf(player);
  int summoned = 0;
  for (uint32_t id : HenchmenOf(player)) {
    void* henchman = CreatureById(id);
    if (!henchman) continue;
    if (!all && !IsDownHenchman(id, henchman) && !engine::IsDeadOrDying(henchman) &&
        engine::AreaOf(henchman) == area) {
      continue;
    }
    g_down_henchmen.erase(id);
    char code[1400];
    sprintf_s(code, kSummonScript, player_id);
    RunScriptChunk(code, id);
    Log("dying: henchman 0x%08X summoned to 0x%08X at full health", id, player_id);
    ++summoned;
  }
  return summoned;
}

bool InTempleArea(void* player) {
  RunScriptChunk(
      "object a = GetArea(OBJECT_SELF);"
      "int t = GetIsObjectValid(a) && (GetArea(GetWaypointByTag(\"NW_DEATH_TEMPLE\")) == a ||"
      " GetArea(GetObjectByTag(\"NW_DEATH_PLACE\")) == a || GetArea(GetObjectByTag(\"X1_TPORT_LOC\")) == a ||"
      " GetAreaFromLocation(GetLocalLocation(OBJECT_SELF, \"X0_RESPAWN_LOC\")) == a ||"
      " GetLocalObject(OBJECT_SELF, \"nwn5e_respawn_area\") == a);"
      "SetLocalInt(OBJECT_SELF, \"nwn5e_in_temple\", t);",
      IdOf(player));
  return engine::GetLocalInt(player, kVarInTemple) != 0;
}

// --- The engine's hooks --------------------------------------------------------------

bool TookCriticalHit(uint32_t id) {
  const auto it = g_critical_hits.find(id);
  if (it == g_critical_hits.end()) return false;
  const bool recent = GetTickCount64() - it->second <= kCriticalWindowMs;
  g_critical_hits.erase(it);
  return recent;
}

void AfterAttackRoll(AttackRoll& roll) {
  if (g_down_players.empty() || !roll.data || !roll.target) return;
  if (Field<uint8_t>(roll.data, ee::kAttackResult) != ee::kAttackResultCritical) return;
  const uint32_t target = IdOf(roll.target);
  if (g_down_players.count(target)) g_critical_hits[target] = GetTickCount64();
}

void PlayerDamaged(void* player, int before, int after) {
  const uint32_t id = IdOf(player);
  const int max_hit_points = MaxHitPoints(player);
  const auto down = g_down_players.find(id);
  if (down != g_down_players.end()) {  // hit at 0 HP
    const int damage = before - after;
    const bool critical = TookCriticalHit(id);
    rules::DeathSaves& saves = down->second.saves;
    if (rules::DamageAtZeroHitPoints(saves, damage, max_hit_points, critical)) {
      SetHitPoints(player, ee::kHitPointsDead);  // OnApplyDamage applies the death
      Log("dying: 0x%08X %d damage at 0 HP%s - dead", id, damage, critical ? " (critical)" : "");
      Later(id, damage >= max_hit_points ? "Massive damage while down - you die."
                                         : "Hit while down: a third failure - you die.");
      return;
    }
    SetHitPoints(player, 0);
    Log("dying: 0x%08X %d damage at 0 HP%s -> %s", id, damage, critical ? " (critical)" : "", Counts(saves).c_str());
    Later(id, std::string("Hit while down: ") + (critical ? "a critical hit, two failures" : "a failure") + " (" +
                  Counts(saves) + ").");
    return;
  }
  if (before < 1 || after > 0) return;  // not a drop to 0
  if (StandingHenchmen(player) == 0) {
    Log("dying: 0x%08X drops to %d HP with no henchman standing - NWN's (the module's OnPlayerDying)", id, after);
    return;
  }
  if (rules::IsMassiveDamage(-after, max_hit_points)) {
    if (after > ee::kHitPointsDead) SetHitPoints(player, ee::kHitPointsDead);
    Log("dying: 0x%08X massive damage (%d past 0, maximum %d) - dead", id, -after, max_hit_points);
    Later(id, "Massive damage (" + std::to_string(-after) + " past 0 HP, your maximum is " +
                  std::to_string(max_hit_points) + ") - you die.");
    return;
  }
  SetHitPoints(player, 0);
  DownPlayer state;
  state.area = engine::AreaOf(player);
  state.last_tick = GetTickCount64();
  state.fight.Start(state.last_tick);
  g_down_players[id] = state;
  g_recent_drops[id] = state.last_tick;
  engine::SetLocalInt(player, kVarDown, 1);
  Log("dying: 0x%08X drops to 0 HP (%d past 0) - death saves; %s", id, -after,
      ScanText(ScanFight(player, player)).c_str());
  Later(id, "You drop to 0 HP and fall unconscious. While a henchman fights on you make a death saving throw every "
            "6 seconds (3 successes: Stable; 3 failures: dead); healing, or the fight's end, wakes you.");
}

void HenchmanDamaged(void* henchman) {
  const uint32_t master = PlayerMasterOf(henchman);
  if (!master) return;
  SetHitPoints(henchman, 1);
  const uint32_t id = IdOf(henchman);
  if (g_down_henchmen.count(id)) return;
  g_down_henchmen[id] = DownHenchman{};
  Log("dying: henchman 0x%08X of 0x%08X down at 1 HP", id, master);
}

void DoDamage(void* creature, int damage) {
  if (!creature || !Faithful()) {
    g_do_damage(creature, damage);
    return;
  }
  const int before = HitPoints(creature);
  g_do_damage(creature, damage);
  const int after = HitPoints(creature);
  if (after >= before) return;
  if (IsPlayer(creature)) {
    PlayerDamaged(creature, before, after);
  } else if (after < 1) {
    HenchmanDamaged(creature);
  }
}

void Respawned(uint32_t id) {
  void* player = CreatureById(id);
  if (!player) return;
  g_down_players.erase(id);
  engine::SetLocalInt(player, kVarDown, 0);
  g_respawned[id] = GetTickCount64();
  ClearExhaustion(player, "Respawn");
  Log("dying: 0x%08X respawned - its henchmen join it once it has landed", id);
}

int RunEventScript(void* module, int script, void* name) {
  if (module && Faithful() && script == ee::kModuleEventPlayerDying) {
    const uint32_t id = Field<uint32_t>(module, ee::kModuleLastPlayerDying);
    const auto drop = g_recent_drops.find(id);
    const bool recent = drop != g_recent_drops.end() && GetTickCount64() - drop->second <= kRecentDropMs;
    if (g_down_players.count(id) || recent) {
      Log("dying: 0x%08X - the module's OnPlayerDying skipped (death saves)", id);
      return 0;
    }
  }
  const int result = g_run_event_script(module, script, name);
  if (module && Faithful() && script == ee::kModuleEventPlayerRespawn) {
    Respawned(Field<uint32_t>(module, ee::kModuleLastRespawner));
  }
  return result;
}

// --- Over time -----------------------------------------------------------------------

// A player's own update: a saved game's down state taken up again, and its
// Exhaustion effect kept on.
void UpkeepPlayer(void* player, uint64_t now) {
  const uint32_t id = IdOf(player);
  PlayerUpkeep& upkeep = g_upkeep[id];
  if (now < upkeep.next) return;
  upkeep.next = now + kUpkeepMs;
  if (!g_down_players.count(id) && engine::GetLocalInt(player, kVarDown)) {
    const int hit_points = HitPoints(player);
    if (hit_points <= 0 && hit_points > ee::kHitPointsDead) {
      DownPlayer state;
      state.area = engine::AreaOf(player);
      state.last_tick = now;
      state.fight.Start(now);
      g_down_players[id] = state;
      Log("dying: 0x%08X down (a saved game) - death saves again", id);
    } else {
      engine::SetLocalInt(player, kVarDown, 0);
    }
  }
  for (uint32_t henchman_id : HenchmenOf(player)) {
    void* henchman = CreatureById(henchman_id);
    if (henchman && !g_down_henchmen.count(henchman_id) && engine::GetLocalInt(henchman, kVarDown)) {
      g_down_henchmen[henchman_id] = DownHenchman{true};
      Log("dying: henchman 0x%08X down (a saved game)", henchman_id);
    }
  }
  if (++upkeep.count % kUpkeepsPerExhaustionCheck == 0 && HitPoints(player) > 0) {
    const int level = ExhaustionOf(player);
    if (level > 0 && !rules::IsDeadFromExhaustion(level)) SyncExhaustionEffect(id, level);
  }
}

void Pass(uint64_t now) {
  std::vector<std::pair<uint32_t, std::string>> lines;
  lines.swap(g_lines);
  for (const auto& line : lines) TellPlayer(line.first, line.second);

  std::vector<uint32_t> ids;
  for (const auto& entry : g_down_players) ids.push_back(entry.first);
  for (uint32_t id : ids) {
    const auto it = g_down_players.find(id);
    if (it != g_down_players.end() && !UpdateDownPlayer(id, it->second, now)) g_down_players.erase(id);
  }
  ids.clear();
  for (const auto& entry : g_down_henchmen) ids.push_back(entry.first);
  for (uint32_t id : ids) {
    const auto it = g_down_henchmen.find(id);
    if (it != g_down_henchmen.end() && !UpdateDownHenchman(id, it->second)) g_down_henchmen.erase(id);
  }
  ids.clear();
  for (const auto& entry : g_respawned) ids.push_back(entry.first);
  for (uint32_t id : ids) {
    const uint64_t since = g_respawned[id];
    void* player = CreatureById(id);
    if (!player || now - since > kSummonGiveUpMs) {
      g_respawned.erase(id);
      continue;
    }
    if (now - since < kSummonDelayMs || HitPoints(player) < 1 || !engine::AreaOf(player)) continue;
    g_respawned.erase(id);
    RunScriptChunk("SetLocalObject(OBJECT_SELF, \"nwn5e_respawn_area\", GetArea(OBJECT_SELF));", id);
    SummonHenchmen(player, true);
  }
}

void Update(void* creature) {
  if (!creature || !Faithful()) return;
  const uint64_t now = GetTickCount64();
  if (IsPlayer(creature)) UpkeepPlayer(creature, now);
  if (now - g_last_pass < kPassMs) return;
  g_last_pass = now;
  Pass(now);
}

// The end of a Long Rest: every level of Exhaustion gone; in a temple's area,
// the henchmen who are down, dead or left behind come.
void AfterLongRest(void* stats) {
  void* player = CreatureOf(stats);
  if (!player || !Faithful() || !IsPlayer(player)) return;
  ClearExhaustion(player, "Long Rest");
  if (InTempleArea(player)) {
    const int summoned = SummonHenchmen(player, false);
    Log("dying: 0x%08X Long Rest in a temple's area - %d henchmen summoned", IdOf(player), summoned);
  }
}

// --- !dying --------------------------------------------------------------------------

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::vector<std::string> DyingCommand(void* creature, const std::string& args) {
  if (!Faithful()) return {"nwn5e: death saves and Exhaustion come with dying faithful."};
  const uint32_t id = IdOf(creature);
  char word[32] = {};
  int number = 0;
  char extra[32] = {};
  const int fields = sscanf_s(Lower(args).c_str(), "%31s %d %31s", word, 32u, &number, extra, 32u);
  const std::string verb = fields >= 1 ? word : "";
  const bool tests = g_config.debug_test_commands;
  if (tests && verb == "roll" && fields >= 2) {
    g_forced_roll = std::clamp(number, 1, 20);
    return {"nwn5e: the next death saving throw's d20 is " + std::to_string(g_forced_roll) + "."};
  }
  if (tests && verb == "hurt" && fields >= 2 && number > 0) {
    uint32_t target = id;
    if (fields >= 3 && std::string(extra) == "henchman") {
      const std::vector<uint32_t> henchmen = HenchmenOf(creature);
      if (henchmen.empty()) return {"nwn5e: you have no henchman."};
      target = henchmen.front();
    }
    ApplyEffectAs(target, target, "DURATION_TYPE_INSTANT",
                  ("EffectDamage(" + std::to_string(number) + ", DAMAGE_TYPE_MAGICAL)").c_str(), 0.0f, 0.0f);
    return {"nwn5e: " + std::to_string(number) + " damage to " + (target == id ? "you." : "your first henchman.")};
  }
  if (tests && verb == "henchman") {  // a henchman where the campaign has none yet (the Prelude)
    std::string resref = Lower(args);
    resref.erase(0, resref.find("henchman") + 8);
    resref.erase(0, resref.find_first_not_of(' '));
    resref.erase(resref.find_last_not_of(' ') + 1);
    if (resref.empty()) resref = "nw_hen_dae_01";  // Daelan, Barbarian 4 (nw_hen_lin_01: Linu, Cleric 4)
    const bool valid = resref.size() <= 16 && std::all_of(resref.begin(), resref.end(), [](char c) {
                         return isalnum(static_cast<unsigned char>(c)) || c == '_';
                       });
    if (!valid) return {"nwn5e: \"" + resref + "\" is not a blueprint name (letters, digits, _; at most 16)."};
    char code[900];
    sprintf_s(code,
              "object h = CreateObject(OBJECT_TYPE_CREATURE, \"%s\", GetLocation(OBJECT_SELF));"
              "if (!GetIsObjectValid(h)) { SendMessageToPC(OBJECT_SELF, \"nwn5e: no creature blueprint %s.\"); "
              "return; }"
              "int n = 1; while (GetIsObjectValid(GetHenchman(OBJECT_SELF, n))) n++;"
              "if (GetMaxHenchmen() < n) SetMaxHenchmen(n);"
              "AddHenchman(OBJECT_SELF, h);"
              "if (GetMaster(h) == OBJECT_SELF) SendMessageToPC(OBJECT_SELF, \"nwn5e: \" + GetName(h) + "
              "\" joins you as a henchman (a test).\");"
              "else SendMessageToPC(OBJECT_SELF, \"nwn5e: \" + GetName(h) + \" could not join you.\");",
              resref.c_str(), resref.c_str());
    RunScriptChunk(code, id);
    return {};
  }
  if (tests && verb == "exhaustion" && fields >= 2) {
    const int level = std::clamp(number, 0, 6);
    SetExhaustion(creature, level);
    if (rules::IsDeadFromExhaustion(level)) Kill(id);
    return {"nwn5e: Exhaustion set to " + std::to_string(level) + (level == 6 ? " - you die." : ".")};
  }
  std::vector<std::string> lines;
  const int exhaustion = ExhaustionOf(creature);
  lines.push_back("nwn5e: Exhaustion " + std::to_string(exhaustion) +
                  (exhaustion ? " (" + ExhaustionText(exhaustion) + "; a Long Rest removes it)." : "."));
  const auto down = g_down_players.find(id);
  if (down != g_down_players.end()) lines.push_back("nwn5e: at 0 HP - " + Counts(down->second.saves) + ".");
  int standing = 0, down_count = 0, elsewhere = 0;
  void* area = engine::AreaOf(creature);
  for (uint32_t henchman_id : HenchmenOf(creature)) {
    void* henchman = CreatureById(henchman_id);
    if (!henchman) continue;
    if (IsDownHenchman(henchman_id, henchman) || engine::IsDeadOrDying(henchman)) {
      ++down_count;
    } else if (engine::AreaOf(henchman) != area) {
      ++elsewhere;
    } else {
      ++standing;
    }
  }
  lines.push_back("nwn5e: henchmen standing " + std::to_string(standing) + ", down " + std::to_string(down_count) +
                  ", elsewhere " + std::to_string(elsewhere) + ". At 0 HP with one standing you make death saves; " +
                  (HasDefyDeath(creature) ? "Defy Death: advantage, 18-20 counts as 20." : "without one, NWN's death."));
  if (tests) {
    lines.push_back("nwn5e: tests - !dying roll <1-20> (the next death save's d20), !dying hurt <HP> [henchman], "
                    "!dying exhaustion <0-6>, !dying henchman [blueprint] (default nw_hen_dae_01 Daelan; "
                    "nw_hen_lin_01 Linu, a healer).");
  }
  return lines;
}

}  // namespace

// Exhaustion's level, for !rest (0 with dying off).
int ExhaustionLevel(void* creature) { return creature && Faithful() ? ExhaustionOf(creature) : 0; }

void InstallDyingRules() {
  if (!Faithful()) return;
  RegisterRecords({kVarExhaustion});
  g_associate_id = reinterpret_cast<AssociateIdFn>(EngineExport(ee::kGetAssociateId));
  g_max_hit_points = reinterpret_cast<MaxHitPointsFn>(EngineExport(ee::kGetMaxHitPoints));
  if (!g_associate_id) {
    Log("SKIP dying: GetAssociateId missing - NWN's dying stays");
    return;
  }
  if (!HookExport("CNWSCreature::DoDamage (dying: death saves, henchmen down)", ee::kCreatureDoDamage,
                  reinterpret_cast<void*>(&DoDamage), reinterpret_cast<void**>(&g_do_damage))) {
    Log("SKIP dying: CNWSCreature::DoDamage not hooked - NWN's dying stays");
    return;
  }
  if (!HookExport("CNWSModule::RunEventScript (dying: OnPlayerDying, OnPlayerRespawn)", ee::kModuleRunEventScript,
                  reinterpret_cast<void*>(&RunEventScript), reinterpret_cast<void**>(&g_run_event_script))) {
    Log("dying: CNWSModule::RunEventScript not hooked - a down player would also get the module's OnPlayerDying");
  }
  if (!AddCreatureUpdateListener(&Update)) Log("dying: no creature update - death saves never roll");
  if (!AddAttackRollListeners(nullptr, &AfterAttackRoll)) Log("dying: no attack-roll hook - crits at 0 HP count once");
  if (!AddRestListener(&AfterLongRest)) Log("dying: the end of a rest is not hooked - Exhaustion stays");
  AddChatCommand("!dying", &DyingCommand);
  Log("dying: death saves with a henchman standing, henchmen lie at 1 HP, the temple summons, Exhaustion "
      "(dying faithful)");
}
