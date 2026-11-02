// Decision D2 (PLAN.md): opportunity attacks (port of 1.69's
// rules_opportunity.cpp; docs/EE-PORT.md E2). In 2024 only leaving a
// creature's reach provokes, once per round (one reaction). NWN gives each
// creature one AoO per round and has no Combat Reflexes (EE neither), but it
// also provokes on casting, drinking potions, ranged shots, unarmed attacks
// and Disarm, and Spring Attack shields movement. Faithful turns those off;
// moving away still provokes and Tumble still avoids it. NWN mode keeps NWN's.
//
// In EE one hook does what 1.69's seven patches did: the AoO broadcast has
// five callers (ee_layout.h) and faithful lets through only the movement one,
// in CNWSCreature::AIUpdate. Two more answers apply only there:
// - Spring Attack: AIUpdate's HasFeat(Spring Attack) answers 0.
// - Running away (D2 follow-up): during the broadcast, the provoker's
//   GetAIStateReaction toward each candidate says "not an enemy" when the
//   running provoker is not moving away from it (its facing points toward or
//   past the candidate), so only those it runs away from take the AoO.
//
// The Fighter's Tactical Shift (F7, class_features faithful, in either AoO
// mode): after Second Wind from Fighter 5, moving provokes nothing for the
// rest of the round — the movement broadcast is skipped while the provoker
// carries nwn5e_tactical_shift (nwn5e_secwind sets it for 6 s). The Rogue's
// Cunning Strike: Withdraw (R6, 0.50.0) does the same for the round of its hit.
//
// Phase 6 part 3 (#54; owner, 2026-11-02: "go with your recommendations"):
// - The Disengage action (disengage.cpp; everyone's, 3 B) skips the movement
//   broadcast the same way, for the round it's taken.
// - The reaction switches: a player character whose "Opportunity attacks" is off
//   isn't an enemy to the one leaving its reach, for the broadcast only - its
//   reaction stays for Uncanny Dodge, Deflect Attacks, Retaliation (each asks
//   ReactionAllowed first). The record: nwn5e_reactions (rules::ReactionOn).

#include <stdint.h>
#include <string.h>

#include <intrin.h>
#include <windows.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "feats.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "reactions.h"
#include "records.h"
#include "rules/combat.h"

bool StaggeredNoOpportunityAttack(uint32_t id);  // rules_barbarian_strikes.cpp (B10 Staggering Blow)
bool CunningWithdrawing(uint32_t id);            // rules_cunning_strike.cpp (R6 Withdraw)
bool MonkDisengaged(uint32_t id);                // rules_monk.cpp (M5 Patient Defense, Step of the Wind)
bool OpenHandAddled(uint32_t id);                // rules_monk.cpp (OH1 Open Hand Technique's Addle)

namespace {

using BroadcastFn = void (*)(void* creature, uint32_t target, int flag);
using ReactionFn = uint8_t (*)(void* object, uint32_t other_id);

BroadcastFn g_broadcast = nullptr;
ReactionFn g_get_ai_state_reaction = nullptr;
CodeRange g_ai_update_code;
thread_local void* t_broadcasting = nullptr;  // the provoker, while the movement broadcast runs
int g_log_lines = 0;
std::unordered_map<uint32_t, DWORD> g_shift_logged;  // by provoker: last Tactical Shift line

bool LogSlot() { return g_config.log_attacks && g_log_lines < 200 && ++g_log_lines; }
constexpr const char* kVarReactions = "nwn5e_reactions";  // #54: rules::ReactionOn's record
constexpr size_t kRoundRunning = 0x2f04;                    // CNWSCombatRound: RoundStarted (#51's recon)

bool Faithful() { return g_config.opportunity_attacks == rules::RuleVariant::kFaithful; }
bool TacticalShiftOn() { return g_config.class_features == rules::RuleVariant::kFaithful; }

// Fighter F7: the provoker used Second Wind this round (Fighter 5+); Rogue R6:
// its Sneak Attack paid for Withdraw this round; Monk M5: it Disengaged with
// Patient Defense or Step of the Wind.
bool TacticalShift(void* creature) {
  if (!TacticalShiftOn()) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const bool shift = engine::GetLocalInt(creature, "nwn5e_tactical_shift") != 0;
  const bool monk = !shift && MonkDisengaged(id);
  if (!shift && !monk && !CunningWithdrawing(id)) return false;
  const DWORD now = GetTickCount();
  DWORD& last = g_shift_logged[id];
  if (g_config.log_attacks && now - last > 6000) {
    last = now;
    Log("opportunity attacks: 0x%08X %s - moving provokes nothing this round", id,
        shift ? "Tactical Shift" : monk ? "Disengage (Patient Defense / Step of the Wind)" : "Cunning Strike: Withdraw");
  }
  return true;
}

// #54 (3 B): it took the Disengage action this round.
bool Disengaged(void* creature) {
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (!Disengaging(id)) return false;
  const DWORD now = GetTickCount();
  DWORD& last = g_shift_logged[id];
  if (now - last > 6000) {
    last = now;
    Log("opportunity attacks: 0x%08X Disengage - moving provokes nothing this round", id);
  }
  return true;
}

bool Running(void* creature) {
  const int animation = Field<int32_t>(creature, ee::kCreatureAnimation);
  return animation == 0x04 || animation == 0x56 || animation == 0x57;
}

const char* Provocation(uintptr_t caller) {
  uintptr_t offset = 0;
  const char* name = ExportContaining(caller, &offset);
  if (!name) return "unknown";
  if (strstr(name, "AIActionItemCastSpell")) return "using an item (potion)";
  if (strstr(name, "AIActionCastSpell")) return "casting";
  if (strstr(name, "EventHandler")) return "an attack (ranged, unarmed or Disarm)";
  return name;
}

void BroadcastAttackOfOpportunity(void* creature, uint32_t target, int flag) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  const bool movement = g_ai_update_code.Contains(caller);
  if (movement && (Disengaged(creature) || TacticalShift(creature))) return;
  if (!Faithful()) {
    g_broadcast(creature, target, flag);  // NWN's provocations
    return;
  }
  if (!movement) {
    if (LogSlot()) {
      Log("opportunity attacks: 0x%08X provokes nothing by %s (faithful)", Field<uint32_t>(creature, ee::kObjectId),
          Provocation(caller));
    }
    return;
  }
  void* outer = t_broadcasting;
  t_broadcasting = creature;
  g_broadcast(creature, target, flag);
  t_broadcasting = outer;
}

uint8_t GetAIStateReaction(void* object, uint32_t other_id) {
  const uint8_t reaction = g_get_ai_state_reaction(object, other_id);
  if (object != t_broadcasting || reaction != ee::kReactionEnemy) return reaction;
  // #54: its "Opportunity attacks" switched off - the reaction kept for the others.
  if (void* observer = CreatureById(other_id); observer && !ReactionAllowed(observer, rules::Reaction::kOpportunityAttack)) {
    if (LogSlot()) {
      Log("opportunity attacks: 0x%08X leaves 0x%08X's reach - no AoO (its Opportunity attacks switch is off)",
          Field<uint32_t>(object, ee::kObjectId), other_id);
    }
    return 1;
  }
  if (StaggeredNoOpportunityAttack(other_id)) return 1;  // Brutal Strike's Staggering Blow: no reaction
  if (OpenHandAddled(other_id)) return 1;                // Open Hand Technique's Addle: no reaction
  if (!Running(object)) return reaction;
  void* candidate = CreatureById(other_id);
  if (!candidate) return reaction;
  const float dx = Field<float>(object, ee::kObjectPositionX) - Field<float>(candidate, ee::kObjectPositionX);
  const float dy = Field<float>(object, ee::kObjectPositionY) - Field<float>(candidate, ee::kObjectPositionY);
  const float away = dx * Field<float>(object, ee::kObjectOrientationX) + dy * Field<float>(object, ee::kObjectOrientationY);
  if (away > 0.0f) return reaction;  // running away from the candidate: it may take the AoO
  if (LogSlot()) {
    Log("opportunity attacks: 0x%08X runs toward 0x%08X, not away: no AoO (faithful)",
        Field<uint32_t>(object, ee::kObjectId), other_id);
  }
  return 1;  // not an enemy, for this check only
}

bool SpringAttackInMovement(void*, int feat, uintptr_t caller, int* answer) {
  if (feat != ee::kFeatSpringAttack || !g_ai_update_code.Contains(caller)) return false;
  *answer = 0;  // Spring Attack no longer protects (faithful)
  return true;
}

// The reactions rules took: the creature's ended rounds then, and when (game ms).
struct ReactionTaken {
  uint32_t rounds = 0;
  uint64_t at = 0;
};
std::unordered_map<uint32_t, ReactionTaken> g_reactions_taken;  // the engine's thread

}  // namespace

bool TakeReaction(void* creature) {
  void* round = creature ? Field<void*>(creature, ee::kCreatureCombatRound) : nullptr;
  if (!round) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const uint32_t rounds = CombatRoundsEnded(id);
  const uint64_t now = GameMs();
  int left = Field<int32_t>(round, ee::kRoundOpportunitiesLeft);
  if (left <= 0) {
    const auto taken = g_reactions_taken.find(id);
    // No round of its own ended since a rule took it, and a round's time went by: not fighting - it's back.
    const bool by_rule = taken != g_reactions_taken.end();
    const bool running = Field<int32_t>(round, kRoundRunning) != 0;  // #54 (0.63.1)
    if (!rules::ReactionBackWithoutRounds(by_rule, by_rule && taken->second.rounds != rounds,
                                          by_rule ? static_cast<long long>(now - taken->second.at) : 0, running)) {
      return false;
    }
    left = 1;
  }
  SetField<int32_t>(round, ee::kRoundOpportunitiesLeft, left - 1);
  g_reactions_taken[id] = ReactionTaken{rounds, now};
  return true;
}

bool ReactionSwitchOn(void* creature, rules::Reaction reaction) {
  if (creature) EnsureRecords(creature);
  return rules::ReactionOn(creature ? engine::GetLocalInt(creature, kVarReactions) : 0, reaction);
}

void SetReactionSwitch(void* creature, rules::Reaction reaction, bool on) {
  if (!creature) return;
  EnsureRecords(creature);
  const int record = rules::SetReactionOn(engine::GetLocalInt(creature, kVarReactions), reaction, on);
  SetRecord(creature, kVarReactions, record);
  static const char* const kNames[] = {"Opportunity attacks", "Uncanny Dodge", "Deflect Attacks", "Retaliation",
                                       "Deflect Attacks' redirect"};
  Log("reactions: 0x%08X %s %s (record %d)", Field<uint32_t>(creature, ee::kObjectId),
      kNames[static_cast<int>(reaction)], on ? "on" : "off", record);
}

bool ReactionAllowed(void* creature, rules::Reaction reaction) {
  if (!creature || g_config.actions != rules::RuleVariant::kFaithful) return true;
  void* stats = StatsOf(creature);
  if (!stats || !ReadCreatureFacts(stats).is_player) return true;  // monsters, henchmen: all of theirs
  return ReactionSwitchOn(creature, reaction);
}

void InstallOpportunityRules() {
  if (g_config.actions == rules::RuleVariant::kFaithful) RegisterRecords({kVarReactions});  // #54
  if (!Faithful() && !TacticalShiftOn()) return;
  g_ai_update_code = FunctionAt(EngineExport(ee::kAIUpdate));
  if (!g_ai_update_code.Valid()) {
    Log("SKIP opportunity attacks: CNWSCreature::AIUpdate's code range is unknown");
    return;
  }
  HookExport(Faithful() ? "CNWSCreature::BroadcastAttackOfOpportunity (only moving away provokes; Tactical Shift)"
                        : "CNWSCreature::BroadcastAttackOfOpportunity (Fighter's Tactical Shift)",
             ee::kBroadcastAttackOfOpportunity, reinterpret_cast<void*>(&BroadcastAttackOfOpportunity),
             reinterpret_cast<void**>(&g_broadcast));
  if (!Faithful()) return;
  HookExport("CNWSObject::GetAIStateReaction (AoO: only creatures run away from)", ee::kGetAIStateReaction,
             reinterpret_cast<void*>(&GetAIStateReaction), reinterpret_cast<void**>(&g_get_ai_state_reaction));
  AddFeatFilter(&SpringAttackInMovement);
}
