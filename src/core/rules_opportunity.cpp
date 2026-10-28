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
#include "hooks.h"
#include "log.h"

bool StaggeredNoOpportunityAttack(uint32_t id);  // rules_barbarian_strikes.cpp (B10 Staggering Blow)
bool CunningWithdrawing(uint32_t id);            // rules_cunning_strike.cpp (R6 Withdraw)

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

bool Faithful() { return g_config.opportunity_attacks == rules::RuleVariant::kFaithful; }
bool TacticalShiftOn() { return g_config.class_features == rules::RuleVariant::kFaithful; }

// Fighter F7: the provoker used Second Wind this round (Fighter 5+); Rogue R6:
// its Sneak Attack paid for Withdraw this round.
bool TacticalShift(void* creature) {
  if (!TacticalShiftOn()) return false;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const bool shift = engine::GetLocalInt(creature, "nwn5e_tactical_shift") != 0;
  if (!shift && !CunningWithdrawing(id)) return false;
  const DWORD now = GetTickCount();
  DWORD& last = g_shift_logged[id];
  if (g_config.log_attacks && now - last > 6000) {
    last = now;
    Log("opportunity attacks: 0x%08X %s - moving provokes nothing this round", id,
        shift ? "Tactical Shift" : "Cunning Strike: Withdraw");
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
  if (movement && TacticalShift(creature)) return;
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
  if (StaggeredNoOpportunityAttack(other_id)) return 1;  // Brutal Strike's Staggering Blow: no reaction
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

}  // namespace

void InstallOpportunityRules() {
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
