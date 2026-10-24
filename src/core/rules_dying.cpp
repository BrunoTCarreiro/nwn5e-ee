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
//   over (FightWatch: no henchman in combat, no one attacking the party) and dies
//   when no henchman is left standing, Stable or not. Any healing wakes it
//   (the engine's own revive).
//   Without a henchman: NWN's (the module's OnPlayerDying).
// - D2 Henchmen, SoU's handling in every campaign: at 0 HP a player's
//   henchman lies at 1 HP - plot, the looping dead animation, uncommandable -
//   and gets up only when healed above 1 HP: a spell, a healer's kit, a Short
//   Rest (its Hit Dice) - the player gets it up after the fight (owner,
//   2026-10-23: "the player needs to get them up manually after the fight").
//   While down, enemies don't see it (an ethereal effect), so they turn on the
//   rest of the party. Death magic from an enemy (Finger of Death, Wail of the
//   Banshee, Power Word Kill...) lays it down the same way (OnApplyDeath, as
//   the engine treats an immortal creature); a death from anything else - a
//   script's, a trap's - still kills, and the campaign's OnDeath has it
//   (owner, 2026-10-24: the 5b recommendations accepted).
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
// - What the player sees (0.20.0, owner: "some on screen indicator of how many
//   failures and successes have happened, a vfx while the PC is doing the rolls
//   and a status icon indicating how many levels of exhaustion"): a marker
//   over the down player, always facing the camera - an icon with three thin
//   bars each side, successes left (green, blue when Stable), failures right
//   (red); the owner's icon is the Pale Master's Bone Skin, half face half
//   skull, in the private layer (visualeffects 1560-1569, nwn5e's own models,
//   tools/data/death_save_marker.py); an aura while rolling (red) or Stable
//   (blue); a flash per roll (success, failure, natural 1, natural 20); and
//   Exhaustion as one effect-bar icon with its level (effecticons 200-204,
//   or 210-214 with the owner's numbered art), the generic penalty icons
//   hidden. Each visual is an ini setting (death_save_*_vfx, 0 = none).
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
#include "data_serving.h"
#include "records.h"
#include "rest.h"
#include "rules/death_saves.h"

int RelentlessRage(void* creature);  // rules_barbarian.cpp (Barbarian B11)

namespace {

constexpr const char* kVarExhaustion = "nwn5e_exhaustion";
constexpr const char* kVarDown = "nwn5e_down";  // a local: a down player or henchman, kept by a saved game
constexpr const char* kVarInTemple = "nwn5e_in_temple";
constexpr uint64_t kDeathSaveMs = 6000;         // one round
constexpr uint64_t kPassMs = 100;
constexpr uint64_t kLongestStepMs = 1000;       // a longer gap (a load, a hitch) is not counted
constexpr uint64_t kCriticalWindowMs = 3000;    // an attack's critical roll to its damage
constexpr uint64_t kRetryMs = 3000;             // a heal or a death on its way that didn't land
constexpr uint64_t kSummonDelayMs = 4000;       // after a respawn: the campaign's own jump (HotU's: 2 s) first
constexpr uint64_t kSummonGiveUpMs = 60000;
constexpr uint64_t kUpkeepMs = 2000;
constexpr uint64_t kFightOverMs = 3000;    // no one fighting: a moment for the next target
constexpr int kUpkeepsPerExhaustionCheck = 3;
constexpr int kMaxHenchmen = 16;
// The visuals (data/rows/visualeffects.txt, effecticons.txt).
constexpr int kMarkerRow = 1560;        // + 3 x successes + failures
constexpr int kMarkerStableRow = 1569;
constexpr int kExhaustionIconRow = 200;     // + level - 1: the game's Fatigue icon
constexpr int kExhaustionArtIconRow = 210;  // + level - 1: the owner's numbered art
constexpr const char* kTagDownAura = "nwn5e_down_aura";
constexpr const char* kTagDownMarker = "nwn5e_down_marker";

using DoDamageFn = void (*)(void* creature, int damage);
using OnApplyDeathFn = int (*)(void* handler, void* object, void* effect, int load_game);
OnApplyDeathFn g_on_apply_death = nullptr;
using RunEventScriptFn = int (*)(void* module, int script, void* name);
using AssociateIdFn = uint32_t (*)(void* creature, uint16_t type, int nth);
using MaxHitPointsFn = int16_t (*)(void* creature, int include_temporary);

DoDamageFn g_do_damage = nullptr;
RunEventScriptFn g_run_event_script = nullptr;
AssociateIdFn g_associate_id = nullptr;
MaxHitPointsFn g_max_hit_points = nullptr;

// The fight around a down player is the engine's own "in combat" (owner,
// 2026-10-23: "a good check is 'if the henchman is actively fighting'"): it
// goes on while a henchman still up is in combat (GetIsInCombat, +0x700) with
// a living attack target, or while a creature in combat has someone of the
// party as its attack target (one that struck the player before the henchman
// joined in); it is over 3 s after neither. Distance is no test: 0.19.3-0.19.5
// ("a living enemy within 30 m") never ended a fight in the Prelude, with 10
// hostiles in range that weren't in it. The living target: the engine keeps a
// creature "in combat" 8 s past its last blow, which kept 0.19.6's player down
// ~10 s after the last enemy fell.
struct FightWatch {
  uint64_t last_fight = 0;
  void Start(uint64_t now) { last_fight = now; }
};

struct DownPlayer {
  rules::DeathSaves saves;
  FightWatch fight;
  void* area = nullptr;
  uint64_t next_save = 0;   // game time (GameMs)
  uint64_t pending_at = 0;  // game time: a heal (waking) or a death on its way
  // What the engine shows (owner, 2026-10-23: the player "got up after the 15
  // roll" while still at 0 HP): posture and animation logged on each change,
  // the posture put back to lying if anything stands it up.
  int posture = -1;
  int animation = -1;
  int changes_logged = 0;
  bool pending_death = false;
  int aura_shown = 0;    // the visualeffects row on now (0 none)
  int marker_shown = 0;
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
// Game time: what the passes saw the game run, unpaused (GetActivePauseState)
// - the clock of the death saves, the fight watch and their retries. The
// engine keeps updating creatures while paused (0.19.4 rolled death saves
// through a pause: owner, 2026-10-23).
uint64_t g_game_ms = 0;
bool g_was_paused = false;
using PauseStateFn = uint8_t (*)(void* app);
PauseStateFn g_pause_state = nullptr;
void** g_app_manager = nullptr;
using SetAnimationFn = void (*)(void* creature, int animation);
SetAnimationFn g_set_animation = nullptr;

bool Paused() {
  if (!g_pause_state || !g_app_manager || !*g_app_manager) return false;
  void* app = Field<void*>(*g_app_manager, ee::kAppManagerServerApp);
  return app && g_pause_state(app) != 0;
}
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

// Who is still fighting around a down player: a party member up and in
// combat, or a creature in combat whose attack target is one of the party.
struct FightScan {
  uint32_t fighter = 0;   // the party member in combat ...
  uint32_t fighting = 0;  // ... with this living target
  uint32_t attacker = 0;  // the creature attacking the party ...
  uint32_t attacked = 0;  // ... and its target
};

bool InCombat(const void* creature) { return Field<int32_t>(creature, ee::kCreatureInCombat) != 0; }

struct FightSearch {
  uint32_t center_id;
  std::vector<std::pair<uint32_t, void*>> party;
  FightScan scan;
};

bool InParty(const FightSearch& search, uint32_t id) {
  for (const auto& member : search.party) {
    if (member.first == id) return true;
  }
  return false;
}

void LookForAttacker(void* context, uint32_t id, void* other) {
  auto* search = static_cast<FightSearch*>(context);
  if (search->scan.attacker || !other || InParty(*search, id)) return;
  if (engine::IsDeadOrDying(other) || !InCombat(other)) return;
  const uint32_t target = Field<uint32_t>(other, ee::kCreatureAttackTarget);
  if (!InParty(*search, target)) return;
  search->scan.attacker = id;
  search->scan.attacked = target;
}

FightScan ScanFight(void* center, void* player) {
  FightSearch search{IdOf(center), {}, {}};
  for (uint32_t id : HenchmenOf(player)) search.party.emplace_back(id, CreatureById(id));
  search.party.emplace_back(IdOf(player), player);
  for (const auto& member : search.party) {
    void* creature = member.second;
    if (member.first == search.center_id || !creature || engine::IsDeadOrDying(creature) || HitPoints(creature) < 1) {
      continue;
    }
    if (!IsPlayer(creature) && IsDownHenchman(member.first, creature)) continue;
    if (!InCombat(creature)) continue;
    const uint32_t target_id = Field<uint32_t>(creature, ee::kCreatureAttackTarget);
    void* target = target_id && target_id != ee::kObjectInvalid ? CreatureById(target_id) : nullptr;
    if (target && !engine::IsDeadOrDying(target)) {
      search.scan.fighter = member.first;
      search.scan.fighting = target_id;
      break;
    }
  }
  engine::ForEachOtherCreatureInArea(center, &LookForAttacker, &search);
  return search.scan;
}

std::string ScanText(const FightScan& scan) {
  if (!scan.fighter && !scan.attacker) return "no one fighting";
  char text[160];
  std::string line;
  if (scan.fighter) {
    sprintf_s(text, "0x%08X fighting 0x%08X", scan.fighter, scan.fighting);
    line = text;
  }
  if (scan.attacker) {
    sprintf_s(text, "%s0x%08X attacking 0x%08X", line.empty() ? "" : ", ", scan.attacker, scan.attacked);
    line += text;
  }
  return line;
}

// Watches the fight around `center`; why it is over, or nullptr while it goes on.
const char* FightOver(void* center, void* player, FightWatch& watch, uint64_t now, FightScan* scan_out) {
  const FightScan scan = ScanFight(center, player);
  if (scan.fighter || scan.attacker) watch.last_fight = now;
  if (scan_out) *scan_out = scan;
  if (now - watch.last_fight >= kFightOverMs) return "no one of the party fighting, no one attacking it, for 3 s";
  return nullptr;
}

void Kill(uint32_t id) { ApplyEffectAs(id, id, "DURATION_TYPE_INSTANT", "EffectDeath()", 0.0f, 0.0f); }
void Flash(uint32_t id, int row) {
  if (row > 0) {
    ApplyEffectAs(id, id, "DURATION_TYPE_INSTANT", ("EffectVisualEffect(" + std::to_string(row) + ")").c_str(), 0.0f,
                  0.0f);
  }
}
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
// The effect-bar icon of a level: the owner's numbered art (n5_ief_exhN.tga,
// effecticons 210-214) when served, else the game's Fatigue icon (200-204).
int ExhaustionIconRow(int level) {
  static int rows[6] = {};
  if (level < 1 || level > 5) return 0;
  if (!rows[level]) {
    const std::wstring art = L"n5_ief_exh" + std::to_wstring(level) + L".tga";
    rows[level] = (ServedFileExists(art.c_str()) ? kExhaustionArtIconRow : kExhaustionIconRow) + level - 1;
  }
  return rows[level];
}

void SyncExhaustionEffect(uint32_t id, int level) {
  const int penalty = -rules::ExhaustionD20Penalty(level);
  const int icon = ExhaustionIconRow(level);
  char code[1600];
  sprintf_s(code,
            "string want = \"nwn5e_exhaustion_%d_%d\"; effect e = GetFirstEffect(OBJECT_SELF);"
            "while (GetIsEffectValid(e)) { string t = GetEffectTag(e);"
            " if (GetStringLeft(t, 16) == \"nwn5e_exhaustion\" && t != want) RemoveEffect(OBJECT_SELF, e);"
            " e = GetNextEffect(OBJECT_SELF); }"
            "if (%d <= 0) return; int have = FALSE; e = GetFirstEffect(OBJECT_SELF);"
            "while (GetIsEffectValid(e)) { if (GetEffectTag(e) == want) have = TRUE; e = GetNextEffect(OBJECT_SELF); }"
            "if (have) return;"
            "effect x = EffectLinkEffects(EffectAttackDecrease(%d), EffectSavingThrowDecrease(SAVING_THROW_ALL, %d));"
            "x = EffectLinkEffects(x, EffectSkillDecrease(SKILL_ALL_SKILLS, %d));"
            "x = EffectLinkEffects(x, EffectMovementSpeedDecrease(%d));"
            "if (%d > 0) x = EffectLinkEffects(EffectIcon(%d), HideEffectIcon(x));"
            "ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(x), want), OBJECT_SELF);",
            level, icon, level, penalty, penalty, penalty, rules::ExhaustionSpeedDecreasePercent(level), icon, icon);
  RunScriptChunk(code, id);
}

// The exhausted look (owner, 2026-10-24: "exhausted levels 1 and 2 the PC
// should have the pausetrd animation when idle. exhaustion levels 3,4 and 5
// should add the injured walk"): EE's ReplaceObjectAnimation swaps the idle
// (pause1, pause2) for the tired one and, from 3, the walk for the limp. Sent
// only on a change; after a load (nothing sent yet) the whole state goes, so
// a swap the saved game kept is undone too. `exhaustion_look`.
struct Look {
  int idle = -1;  // -1: not sent yet
  int walk = -1;
};
std::unordered_map<uint32_t, Look> g_looks;

void SyncExhaustionLook(uint32_t id, int level) {
  if (!g_config.exhaustion_look || rules::IsDeadFromExhaustion(level)) level = 0;
  const int idle = level >= 1 ? 1 : 0;
  const int walk = level >= 3 ? 1 : 0;
  Look& look = g_looks[id];
  if (look.idle == idle && look.walk == walk) return;
  std::string code;
  if (look.idle != idle) {
    code += idle ? "ReplaceObjectAnimation(OBJECT_SELF, \"pause1\", \"pausetrd\");"
                   "ReplaceObjectAnimation(OBJECT_SELF, \"pause2\", \"pausetrd\");"
                 : "ReplaceObjectAnimation(OBJECT_SELF, \"pause1\"); ReplaceObjectAnimation(OBJECT_SELF, \"pause2\");";
  }
  if (look.walk != walk) {
    code += walk ? "ReplaceObjectAnimation(OBJECT_SELF, \"walk\", \"walkinj\");"
                 : "ReplaceObjectAnimation(OBJECT_SELF, \"walk\");";
  }
  const bool first = look.idle < 0;
  look.idle = idle;
  look.walk = walk;
  RunScriptChunk(code.c_str(), id);
  if (!first || idle || walk) {
    Log("dying: 0x%08X exhausted look - %s idle, %s walk (Exhaustion %d)", id, idle ? "tired" : "normal",
        walk ? "injured" : "normal", level);
  }
}

void SetExhaustion(void* player, int level) {
  SetRecord(player, kVarExhaustion, level);
  SyncExhaustionEffect(IdOf(player), rules::IsDeadFromExhaustion(level) ? 0 : level);
  SyncExhaustionLook(IdOf(player), level);
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

void RollSave(void* player, uint32_t id, DownPlayer& state, uint64_t now, const std::string& fight) {
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
  Log("dying: 0x%08X death save %d%s (total %d) -> %s; %s", id, save.roll.kept, defy ? " (advantage)" : "",
      save.total, Counts(state.saves).c_str(), fight.c_str());
  Tell(id, line);
  int flash = g_config.death_save_failure_vfx;
  if (save.result == rules::DeathSaveResult::kSuccess || save.result == rules::DeathSaveResult::kStable) {
    flash = g_config.death_save_success_vfx;
  } else if (save.result == rules::DeathSaveResult::kWake) {
    flash = g_config.death_save_nat20_vfx;
  } else if (save.roll.kept == 1) {
    flash = g_config.death_save_nat1_vfx;
  }
  Flash(id, flash);
}

// --- What the player sees while down ------------------------------------------------

// A permanent visual effect tagged `tag` on the creature, replaced by `row`
// (0: just removed).
void ReplaceTaggedVisual(uint32_t id, const char* tag, int row) {
  char code[600];
  sprintf_s(code,
            "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == \"%s\") "
            "RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }"
            "if (%d > 0) ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect("
            "EffectVisualEffect(%d)), \"%s\"), OBJECT_SELF);",
            tag, row, row, tag);
  RunScriptChunk(code, id);
}

int MarkerRow(const rules::DeathSaves& saves) {
  if (saves.stable) return kMarkerStableRow;
  return kMarkerRow + 3 * std::min(saves.successes, 2) + std::min(saves.failures, 2);
}

// The aura (rolling or Stable) and the death-save marker, as the saves stand.
void SyncDownVisuals(uint32_t id, DownPlayer& state) {
  const int aura = state.saves.stable ? g_config.death_save_stable_vfx : g_config.death_save_aura_vfx;
  if (aura != state.aura_shown) {
    ReplaceTaggedVisual(id, kTagDownAura, aura);
    state.aura_shown = aura;
  }
  const int marker = g_config.death_save_marker ? MarkerRow(state.saves) : 0;
  if (marker != state.marker_shown) {
    ReplaceTaggedVisual(id, kTagDownMarker, marker);
    state.marker_shown = marker;
  }
}

void ClearDownVisuals(uint32_t id) {
  ReplaceTaggedVisual(id, kTagDownAura, 0);
  ReplaceTaggedVisual(id, kTagDownMarker, 0);
}

// A down player keeps lying: its posture and animation logged when they change
// (the first 20 per drop), the posture put back if something stands it up.
void WatchPosture(void* player, uint32_t id, DownPlayer& state) {
  const int posture = Field<uint8_t>(player, ee::kCreaturePosture);
  const int animation = Field<int32_t>(player, ee::kObjectAnimation);
  if (posture != state.posture || animation != state.animation) {
    if (state.changes_logged < 20) {
      ++state.changes_logged;
      Log("dying: 0x%08X down - posture %d -> %d, animation %d -> %d", id, state.posture, posture, state.animation,
          animation);
    }
    state.posture = posture;
    state.animation = animation;
  }
  if (posture == ee::kPostureLyingBack || posture == ee::kPostureLyingFront) return;
  SetField<uint8_t>(player, ee::kCreaturePosture, ee::kPostureLyingFront);
  if (g_set_animation) g_set_animation(player, 0);  // "stand" = the posture's lying animation
  Log("dying: 0x%08X down but posture %d - put back to lying (animation now %d)", id, posture,
      Field<int32_t>(player, ee::kObjectAnimation));
}

// One down player's pass. False when its time at 0 HP is over.
bool UpdateDownPlayer(uint32_t id, DownPlayer& state, uint64_t now) {
  void* player = CreatureById(id);
  if (!player || engine::AreaOf(player) != state.area) {  // gone, or a saved game loaded
    if (player) {
      engine::SetLocalInt(player, kVarDown, 0);
      ClearDownVisuals(id);
    }
    return false;
  }
  const int hit_points = HitPoints(player);
  if (hit_points <= ee::kHitPointsDead) {  // dead (a third failure, the last henchman, massive damage)
    engine::SetLocalInt(player, kVarDown, 0);
    ClearDownVisuals(id);
    Log("dying: 0x%08X dead", id);
    return false;
  }
  if (hit_points > 0) {  // healed, a natural 20, the fight's end
    engine::SetLocalInt(player, kVarDown, 0);
    ClearDownVisuals(id);
    Log("dying: 0x%08X back up at %d HP", id, hit_points);
    Tell(id, "You're back up at " + std::to_string(hit_points) + " HP.");
    GainExhaustion(player);
    return false;
  }
  WatchPosture(player, id, state);
  SyncDownVisuals(id, state);
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
  if (state.saves.stable || now < state.next_save) return true;
  state.next_save = now + kDeathSaveMs;
  RollSave(player, id, state, now, ScanText(scan));
  return true;
}

// --- Henchmen ----------------------------------------------------------------------

// Down: plot (no more damage; its own flag kept), the "dead" pose, uncommandable.
constexpr const char* kLayDownScript =
    "SetLocalInt(OBJECT_SELF, \"nwn5e_down\", 1);"
    "SetLocalInt(OBJECT_SELF, \"nwn5e_down_plot\", GetPlotFlag(OBJECT_SELF));"
    "SetPlotFlag(OBJECT_SELF, TRUE); SetCommandable(TRUE, OBJECT_SELF); ClearAllActions(TRUE);"
    "PlayVoiceChat(VOICE_CHAT_HEALME, OBJECT_SELF);"
    "ApplyEffectToObject(DURATION_TYPE_PERMANENT, TagEffect(SupernaturalEffect(EffectEthereal()), "
    "\"nwn5e_down_unseen\"), OBJECT_SELF);"
    "ActionPlayAnimation(ANIMATION_LOOPING_DEAD_FRONT, 1.0, 100000.0); SetCommandable(FALSE, OBJECT_SELF);"
    "object m = GetMaster(OBJECT_SELF); if (GetIsPC(m)) SendMessageToPC(m, \"nwn5e: \" + GetName(OBJECT_SELF) + "
    "\" is down - heal them (a spell, a healer's kit) or take a Short Rest after the fight to get them up.\");";

constexpr const char* kGetUpScript =
    "SetCommandable(TRUE, OBJECT_SELF);"
    "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "
    "\"nwn5e_down_unseen\") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }"
    "SetPlotFlag(OBJECT_SELF, GetLocalInt(OBJECT_SELF, \"nwn5e_down_plot\"));"
    "DeleteLocalInt(OBJECT_SELF, \"nwn5e_down\"); DeleteLocalInt(OBJECT_SELF, \"nwn5e_down_plot\");"
    "ClearAllActions(TRUE);"
    "object m = GetMaster(OBJECT_SELF); if (GetIsPC(m)) SendMessageToPC(m, \"nwn5e: \" + GetName(OBJECT_SELF) + "
    "\" gets back up.\");";

// To the player at full health: up if down, raised if dead, jumped if elsewhere.
constexpr const char* kSummonScript =
    "object pc = StringToObject(\"%x\");"
    "SetCommandable(TRUE, OBJECT_SELF);"
    "effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { if (GetEffectTag(e) == "
    "\"nwn5e_down_unseen\") RemoveEffect(OBJECT_SELF, e); e = GetNextEffect(OBJECT_SELF); }"
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
  state.next_save = g_game_ms + kDeathSaveMs;
  state.fight.Start(g_game_ms);
  g_down_players[id] = state;
  g_recent_drops[id] = GetTickCount64();
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

// Death magic from an enemy on a player's henchman: down at 1 HP, as damage
// would leave it; the handler's own immortal path returns 1 (done, no death).
int OnApplyDeath(void* handler, void* object, void* effect, int load_game) {
  if (Faithful() && object && effect && !load_game) {
    void* creature = CreatureById(IdOf(object));
    if (creature && !engine::IsDeadOrDying(creature) && PlayerMasterOf(creature)) {
      const uint32_t creator = effects::Creator(effect);
      if (creator && creator != ee::kObjectInvalid && engine::IsEnemy(creature, creator)) {
        Log("dying: henchman 0x%08X - death magic from 0x%08X lays it down instead", IdOf(creature), creator);
        HenchmanDamaged(creature);
        return 1;
      }
    }
  }
  return g_on_apply_death(handler, object, effect, load_game);
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
  // Barbarian B11 Relentless Rage: dropped to 0 (not killed outright) while raging.
  if (before >= 1 && after < 1 && !rules::IsMassiveDamage(-after, MaxHitPoints(creature))) {
    const int relentless = RelentlessRage(creature);
    if (relentless > 0) {
      SetHitPoints(creature, relentless);
      return;
    }
  }
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
      state.next_save = g_game_ms + kDeathSaveMs;
      state.fight.Start(g_game_ms);
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
  const int level = ExhaustionOf(player);
  SyncExhaustionLook(id, level);
  if (++upkeep.count % kUpkeepsPerExhaustionCheck == 0 && HitPoints(player) > 0) {
    if (level > 0 && !rules::IsDeadFromExhaustion(level)) SyncExhaustionEffect(id, level);
  }
}

// `now` is game time (the down players' clocks), `real_now` the wall clock.
void Pass(uint64_t now, uint64_t real_now) {
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
    if (!player || real_now - since > kSummonGiveUpMs) {
      g_respawned.erase(id);
      continue;
    }
    if (real_now - since < kSummonDelayMs || HitPoints(player) < 1 || !engine::AreaOf(player)) continue;
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
  const uint64_t step = now - g_last_pass;
  g_last_pass = now;
  const bool paused = Paused();
  if (paused != g_was_paused) {
    g_was_paused = paused;
    if (!g_down_players.empty()) Log("dying: the game %s", paused ? "is paused - the death saves wait" : "runs again");
  }
  if (!paused && step <= kLongestStepMs) g_game_ms += step;
  Pass(g_game_ms, now);
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

// !anim's Stop puts the walk back: the exhausted look is sent again at the next upkeep.
void ResendExhaustionLook(uint32_t id) { g_looks.erase(id); }

void InstallDyingRules() {
  if (!Faithful()) return;
  RegisterRecords({kVarExhaustion});
  g_associate_id = reinterpret_cast<AssociateIdFn>(EngineExport(ee::kGetAssociateId));
  g_max_hit_points = reinterpret_cast<MaxHitPointsFn>(EngineExport(ee::kGetMaxHitPoints));
  g_pause_state = reinterpret_cast<PauseStateFn>(EngineExport(ee::kGetActivePauseState));
  g_set_animation = reinterpret_cast<SetAnimationFn>(EngineExport(ee::kCreatureSetAnimation));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!g_pause_state || !g_app_manager) Log("dying: the pause state is unknown - death saves go on through a pause");
  if (!g_associate_id) {
    Log("SKIP dying: GetAssociateId missing - NWN's dying stays");
    return;
  }
  if (!HookExport("CNWSCreature::DoDamage (dying: death saves, henchmen down)", ee::kCreatureDoDamage,
                  reinterpret_cast<void*>(&DoDamage), reinterpret_cast<void**>(&g_do_damage))) {
    Log("SKIP dying: CNWSCreature::DoDamage not hooked - NWN's dying stays");
    return;
  }
  if (!HookExport("CNWSEffectListHandler::OnApplyDeath (dying: death magic lays a henchman down)", ee::kOnApplyDeath,
                  reinterpret_cast<void*>(&OnApplyDeath), reinterpret_cast<void**>(&g_on_apply_death))) {
    Log("dying: OnApplyDeath not hooked - death magic still kills henchmen");
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
