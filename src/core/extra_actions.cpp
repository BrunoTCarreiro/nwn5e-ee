// Phase 6 (#53; docs/PLAN.md "Phase 6 — Action economy"; the owner, 2026-11-03: "go with your recommendations. but
// see a lot of these automatic actions should be managed by the user, for now in the !options panel"), class
// features faithful (granted by rules_features.cpp: rules::GrantedFeats2024; data/rows/feat.fighter.txt,
// feat.rogue.txt; the radial's class sections - cls_feat rows - and the quickbar):
// - Action Surge (Fighter 2; feat 1325, n5_actionsurge; instant - rules::TakesNoAction): the Attack action again.
//   Its uses: a pool (feat_uses.cpp; rules::ActionSurgeUses - 1, 2 from 17), all back on a Short Rest, emptied by a
//   Long one. Used while the Fighter attacks in a running round: its Attack action's attacks
//   (rules::AttacksPerAttackAction) added to the round now, as swings of their own spaced with the round's pending
//   ones (round_swings.cpp: AddRoundSwings; 0.66.2, #53 - 0.65.0's AddCleaveAttack put them all at once, named
//   Cleave). Otherwise the next round the Fighter starts gets them (its extra attacks, +0x2F60, at the round's
//   start - one swing each, round_swings.cpp).
// - Thief's Reflexes (Thief 17 - the SRD's one Rogue subclass): a fight's first round (initiative) gets the Attack
//   action twice, the same way; switchable in Combat configuration (record nwn5e_thief_reflexes_off; on by default).
// - Cunning Action (Rogue 2; feats 1326-1328): Dash, Disengage, Hide as Bonus Actions (rules::TakesBonusAction:
//   instant, one a round - bonus_actions.cpp); Dash and Disengage as everyone's (staple_actions.cpp,
//   disengage.cpp); Hide: stealth mode switched on by the server (CNWSCreature::SetActivity(stealth, on) - the
//   engine's own way in, as ToggleMode's; no combat check there; the stealth rules roll Stealth as hiding starts,
//   rules_perception.cpp).
//   0.66.4 (the owner: "after cunning action hide mid round I cannot exit stealth mode even after combat is done"):
//   0.65.0's SetStealthMode alone left the stealth activity bit (+0x8E0 bit 0) off, and SetActivity - the way out
//   for the stealth button and a script's SetActionMode (ToggleMode) - finds nothing to turn off without it; and
//   ToggleMode refuses stealth changes both ways while the creature has an attack target (+0x724 / +0x728). Now the
//   Hide goes in by SetActivity, and a ToggleMode hook takes a hider out even with an attack target
//   (rules::StealthToggleAllowed), its activity bit set first (a hide from 0.65.0 - 0.66.3), SetStealthMode(0) if
//   the engine still leaves it on.
//   #129 A (0.66.5; the owner, 2026-11-03: "do A and B now before I continue with the matrix" - a Hide in the middle
//   of an attack was undone by the attack's next swing): the Hide stops your attacks - the attack order cancelled as
//   the action queue's own click cancels it (CNWSCreature::CancelAction: its round's swings go with it) and the
//   default attack off until your next attack order; you stay hidden until you attack. A waiting Hide goes off at a
//   round's start - from inside the attack step (AIActionAttackObject -> StartCombatRound), where its action can't be
//   cancelled - so the cancel runs a moment later (DelayCommand -> n5_ca_hidestop), and the round's swings are held
//   until then (round_swings.cpp asks HideHoldsAttacks). The attack you then order is rolled hidden (B,
//   rules_perception.cpp).
// - Steady Aim (Rogue 3; feat 1329): a Bonus Action refused (a UseFeat guard - the Bonus Action not spent) once the
//   Rogue moved since its round began (rules::SteadyAimAllowed: half a metre); its next attack with Advantage
//   (rules_advantage.cpp), and no moving until the round ends (EffectCutsceneImmobilize, as the grapple holds).
//   Cunning Action and Steady Aim can be the default Bonus Action (Combat configuration).

#include "extra_actions.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "action_order.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "default_attack.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "feat_use.h"
#include "feat_uses.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "rest.h"
#include "round_swings.h"
#include "script_hooks.h"
#include "staple_actions.h"
#include "rules/class_features.h"
#include "rules/combat.h"

namespace {

constexpr const char* kVarSurgeSpent = "nwn5e_surge_spent";
constexpr const char* kVarThiefOff = "nwn5e_thief_reflexes_off";
constexpr size_t kRoundStarted = 0x2f04;        // CNWSCombatRound: a round running (#51's recon)
constexpr size_t kCreatureStealthMode = 0x6ed;  // byte: SetStealthMode's
constexpr uint64_t kRoundMs = 6000;
constexpr uint64_t kFirstRoundWindowMs = 10000;  // initiative to the first round's start

using SetStealthModeFn = void (*)(void* creature, uint8_t mode);
using SetActivityFn = void (*)(void* creature, int activity, int on);
using ToggleModeFn = int (*)(void* creature, uint8_t mode);

constexpr const char* kSetActivity = "?SetActivity@CNWSCreature@@QEAAXHH@Z";
constexpr const char* kToggleMode = "?ToggleMode@CNWSCreature@@QEAAHE@Z";
constexpr size_t kCreatureActivities = 0x8e0;  // uint32: bit 0 stealth (SetActivity)
constexpr int kActivityStealth = 1;
constexpr uint8_t kModeStealth = 1;  // ToggleMode / SetMode / ACTION_MODE_STEALTH

ToggleModeFn g_toggle_mode = nullptr;

// #129 A: the Hide stops your attacks.
using AqIdFn = uint16_t (*)(void* object, uint16_t id, int feat);
using CancelActionFn = int (*)(void* creature, uint16_t action, uint16_t id);
constexpr uint64_t kHideHoldMs = 3000;  // the round's swings held at most this long for the cancel
std::unordered_map<uint32_t, uint64_t> g_hide_hold;  // id -> held until (game ms)

struct Start {
  float x = 0.0f, y = 0.0f;
  uint32_t rounds = 0;
};
struct Steady {
  uint32_t rounds = 0;
  uint64_t until = 0;
  bool advantage = true;
};
// The engine's thread.
std::unordered_map<uint32_t, int> g_surge_pending;        // attacks for the next round started
std::unordered_map<uint32_t, uint64_t> g_first_round;     // Thief's Reflexes: initiative (game ms)
std::unordered_map<uint32_t, Start> g_round_start;        // Steady Aim: where the round began
std::unordered_map<uint32_t, Steady> g_steady;
int g_lines = 0;

bool Faithful() { return g_config.class_features == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_lines < 400 && ++g_lines; }
uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, const std::string& line) {
  if (IsPlayer(creature)) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

int ClassLevel(void* stats, rules::CharacterClass which) {
  if (!stats) return 0;
  int levels = 0;
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    if (c.character_class == which) levels += c.levels;
  }
  return levels;
}

int AttackActionAttacks(void* stats) {
  const int attacks = stats ? rules::AttacksPerAttackAction(rules::GroupClassLevels(ReadClassSlots(stats))) : 1;
  return attacks > 0 ? attacks : 1;
}

uint32_t RoundsOf(uint32_t id) { return CombatRoundsEnded(id); }

float MovedSinceRoundStart(void* creature) {
  const uint32_t id = IdOf(creature);
  const auto start = g_round_start.find(id);
  if (start == g_round_start.end() || start->second.rounds != RoundsOf(id)) return 0.0f;
  const float dx = Field<float>(creature, ee::kObjectPositionX) - start->second.x;
  const float dy = Field<float>(creature, ee::kObjectPositionX + 4) - start->second.y;
  return std::sqrt(dx * dx + dy * dy);
}

bool Moving(void* creature) { return Field<uint16_t>(creature, ee::kObjectCurrentAction) == ee::kActionMoveToPoint; }

// --- Action Surge -----------------------------------------------------------------------------------------------

bool SurgeUsesTotal(void* stats, int feat, int* total) {
  if (feat != rules::kFeatActionSurge || !Faithful()) return false;
  *total = rules::ActionSurgeUses(ClassLevel(stats, rules::CharacterClass::kFighter));
  return true;
}

std::string SurgeShortRest(void* creature) {
  if (!creature || SpentFeatUses(creature, kVarSurgeSpent) <= 0) return "";
  SetSpentFeatUses(creature, kVarSurgeSpent, 0);
  return " Action Surge: back.";
}

void ActionSurgeScript(uint32_t self) {
  void* creature = CreatureById(self);
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return;
  const int attacks = AttackActionAttacks(stats);
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  const uint32_t target = Field<uint32_t>(creature, ee::kCreatureAttackTarget);
  void* victim = CreatureById(target);
  if (round && Field<int32_t>(round, kRoundStarted) != 0 && victim && !engine::IsDeadOrDying(victim) &&
      AddRoundSwings(round, target, attacks)) {
    if (LogSlot()) Log("actions: 0x%08X Action Surge - the Attack action again: %d attack(s) added to this round at "
                       "0x%08X, swings of their own", self, attacks, target);
    Tell(creature, "Action Surge - your Attack action again: " + std::to_string(attacks) + " more attack" +
                       (attacks == 1 ? "" : "s") + " this round.");
    return;
  }
  g_surge_pending[self] = attacks;
  if (LogSlot()) Log("actions: 0x%08X Action Surge - %d attack(s) added to the next round it starts", self, attacks);
  Tell(creature, "Action Surge - your next round has your Attack action twice.");
}

// --- Cunning Action, Steady Aim -------------------------------------------------------------------------------------

void CunningDashScript(uint32_t self) { StartDash(self); }
void CunningDisengageScript(uint32_t self) { StartDisengage(self); }

void CunningHideScript(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature) return;
  static const auto set_activity = reinterpret_cast<SetActivityFn>(EngineExport(kSetActivity));
  if (!set_activity) return;
  if (Field<uint8_t>(creature, kCreatureStealthMode) == 1) {
    Tell(creature, "Cunning Action: Hide - you're already in stealth mode.");
    return;
  }
  // The engine's way in: the stealth activity bit, then SetStealthMode (rules_perception.cpp's hook: Stealth rolled
  // as hiding starts) - so the stealth button can take it out again.
  set_activity(creature, kActivityStealth, 1);
  if (Field<uint8_t>(creature, kCreatureStealthMode) != 1) {
    if (LogSlot()) Log("actions: 0x%08X Cunning Action: Hide - the engine kept it out of stealth mode (activities 0x%X)",
                       self, Field<uint32_t>(creature, kCreatureActivities));
    Tell(creature, "Cunning Action: Hide - you can't go into stealth mode now.");
    return;
  }
  if (LogSlot()) Log("actions: 0x%08X Cunning Action: Hide - stealth mode on (a Bonus Action)", self);
  Tell(creature, "Cunning Action: Hide - into stealth mode (your Stealth check decides who still sees you).");
  // #129 A: the Hide stops your attacks - the default attack off now, the attack order cancelled in a moment.
  StopDefaultAttack(creature, "Cunning Action: Hide - hiding stops your attacks");
  bool attacking = false;
  for (void* node : QueueNodes(creature)) {
    if (Field<int32_t>(node, ee::kActionId) == ee::kActionAttackObject) attacking = true;
  }
  if (!attacking) return;
  g_hide_hold[self] = GameMs() + kHideHoldMs;
  RunScriptChunk("DelayCommand(0.0, ExecuteScript(\"n5_ca_hidestop\", OBJECT_SELF));", self);
}

// #129 A: the Hide's attack stop, outside the attack step - the attack order's groups cancelled.
void HideStopScript(uint32_t self) {
  g_hide_hold.erase(self);
  void* creature = CreatureById(self);
  if (!creature) return;
  static const auto aq_id = reinterpret_cast<AqIdFn>(EngineExport(ee::kGetAQActionIDByID));
  static const auto cancel = reinterpret_cast<CancelActionFn>(EngineExport(ee::kCreatureCancelAction));
  if (!aq_id || !cancel) return;
  std::vector<uint16_t> groups;
  for (void* node : QueueNodes(creature)) {
    if (Field<int32_t>(node, ee::kActionId) != ee::kActionAttackObject) continue;
    const uint16_t group = Field<uint16_t>(node, ee::kActionGroupId);
    if (std::find(groups.begin(), groups.end(), group) == groups.end()) groups.push_back(group);
  }
  const uint16_t display = aq_id(creature, static_cast<uint16_t>(ee::kActionAttackObject), 0);
  int stopped = 0;
  for (uint16_t group : groups) stopped += cancel(creature, display, group) != 0 ? 1 : 0;
  if (LogSlot()) {
    Log("actions: 0x%08X Cunning Action: Hide - its attack stopped (%d of %d attack order(s)) - hidden until it "
        "attacks", self, stopped, static_cast<int>(groups.size()));
  }
  if (stopped > 0) Tell(creature, "Cunning Action: Hide - you stop attacking and stay hidden until you attack.");
}

// The stealth button, or a script's SetActionMode (a find): out of stealth mode even with an attack target.
int ToggleMode(void* creature, uint8_t mode) {
  if (!creature || mode != kModeStealth || Field<uint8_t>(creature, kCreatureStealthMode) != 1) {
    return g_toggle_mode(creature, mode);  // into stealth mode: NWN's own rules (no attack target)
  }
  const uint32_t target = Field<uint32_t>(creature, ee::kCreatureAttackTarget);
  const uint32_t attempted = Field<uint32_t>(creature, ee::kCreatureAttemptedAttackTarget);
  const bool fighting = target != ee::kObjectInvalid || attempted != ee::kObjectInvalid;
  if (!rules::StealthToggleAllowed(false, fighting)) return g_toggle_mode(creature, mode);
  const uint32_t activities = Field<uint32_t>(creature, kCreatureActivities);
  SetField<uint32_t>(creature, kCreatureActivities, activities | kActivityStealth);  // SetActivity's to turn off
  if (fighting) {
    SetField<uint32_t>(creature, ee::kCreatureAttackTarget, ee::kObjectInvalid);
    SetField<uint32_t>(creature, ee::kCreatureAttemptedAttackTarget, ee::kObjectInvalid);
  }
  int done = g_toggle_mode(creature, mode);
  if (fighting) {
    SetField<uint32_t>(creature, ee::kCreatureAttackTarget, target);
    SetField<uint32_t>(creature, ee::kCreatureAttemptedAttackTarget, attempted);
  }
  bool forced = false;
  if (Field<uint8_t>(creature, kCreatureStealthMode) == 1) {  // still on (e.g. it can't use Hide / Move Silently)
    static const auto set_stealth = reinterpret_cast<SetStealthModeFn>(EngineExport(ee::kSetStealthMode));
    if (set_stealth) set_stealth(creature, 0);
    SetField<uint32_t>(creature, kCreatureActivities,
                       Field<uint32_t>(creature, kCreatureActivities) & ~static_cast<uint32_t>(kActivityStealth));
    forced = true;
    done = 1;
  }
  if ((fighting || !(activities & kActivityStealth) || forced) && LogSlot()) {
    Log("stealth: 0x%08X out of stealth mode%s%s%s", IdOf(creature), fighting ? " - its attack target set aside" : "",
        (activities & kActivityStealth) ? "" : " - its stealth activity bit was off (a Cunning Action hide before 0.66.4)",
        forced ? " - SetStealthMode(0): the engine left it on" : "");
  }
  return done;
}

// UseFeat's guard: Steady Aim refused once the Rogue moved this round (the Bonus Action not spent).
bool SteadyAimGuard(void* creature, int feat) {
  if (feat != rules::kFeatSteadyAim || !Faithful() || !creature) return true;
  if (SteadyAimReady(creature)) return true;
  if (LogSlot()) Log("actions: 0x%08X Steady Aim refused - it moved %.1f m this round", IdOf(creature),
                     MovedSinceRoundStart(creature));
  Tell(creature, "Steady Aim - only if you haven't moved this round.");
  return false;
}

void SteadyAimScript(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature) return;
  g_steady[self] = Steady{RoundsOf(self), GameMs() + kRoundMs, true};
  ApplyEffectAs(self, self, "DURATION_TYPE_TEMPORARY", "EffectCutsceneImmobilize()", 6.0f, 0.0f);
  if (LogSlot()) Log("actions: 0x%08X Steady Aim - Advantage on its next attack, no moving this round", self);
  Tell(creature, "Steady Aim - Advantage on your next attack; you can't move until your next round.");
}

}  // namespace

void ExtraActionsRoundStart(void* round) {
  if (!Faithful() || !round) return;
  void* creature = Field<void*>(round, ee::kRoundOwner);
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  g_round_start[id] = Start{Field<float>(creature, ee::kObjectPositionX),
                            Field<float>(creature, ee::kObjectPositionX + 4), RoundsOf(id)};
  int extra = 0;
  const char* why = nullptr;
  if (!g_surge_pending.empty()) {
    const auto surge = g_surge_pending.find(id);
    if (surge != g_surge_pending.end()) {
      extra += surge->second;
      why = "Action Surge";
      g_surge_pending.erase(surge);
    }
  }
  if (!g_first_round.empty()) {
    const auto first = g_first_round.find(id);
    if (first != g_first_round.end()) {
      if (GameMs() - first->second <= kFirstRoundWindowMs) {
        extra += AttackActionAttacks(StatsOf(creature));
        why = why ? "Action Surge and Thief's Reflexes" : "Thief's Reflexes";
      }
      g_first_round.erase(first);
    }
  }
  if (extra <= 0) return;
  SetField<int32_t>(round, ee::kRoundExtraAttacks, Field<int32_t>(round, ee::kRoundExtraAttacks) + extra);
  if (LogSlot()) Log("actions: 0x%08X %s - %d more attack(s) this round", id, why, extra);
  if (why && std::string(why).find("Thief") != std::string::npos) {
    Tell(creature, "Thief's Reflexes - your Attack action twice in the fight's first round.");
  }
}

void ThiefsReflexesOnInitiative(void* creature) {
  if (!Faithful() || g_config.actions != rules::RuleVariant::kFaithful || !creature) return;
  if (!rules::HasThiefsReflexes(ClassLevel(StatsOf(creature), rules::CharacterClass::kRogue))) return;
  if (!ThiefsReflexesOn(creature)) return;
  g_first_round[IdOf(creature)] = GameMs();
}

MasteryRollSources SteadyAimRollSources(void* attacker) {
  MasteryRollSources sources;
  if (g_steady.empty() || !attacker) return sources;
  const auto it = g_steady.find(IdOf(attacker));
  if (it == g_steady.end()) return sources;
  if (!it->second.advantage || GameMs() >= it->second.until) {
    g_steady.erase(it);
    return sources;
  }
  it->second.advantage = false;  // the next attack only
  sources.advantage = 1;
  sources.note = "Steady Aim";
  return sources;
}

bool SteadyAimReady(void* creature) {
  if (!creature) return false;
  const uint32_t id = IdOf(creature);
  const auto start = g_round_start.find(id);
  if (start != g_round_start.end() && start->second.rounds == RoundsOf(id)) {
    return rules::SteadyAimAllowed(MovedSinceRoundStart(creature));
  }
  return !Moving(creature);  // no round of its own running: not moving now
}

bool HideReady(void* creature) { return creature && Field<uint8_t>(creature, kCreatureStealthMode) != 1; }

bool HideHoldsAttacks(void* creature) {
  if (g_hide_hold.empty() || !creature) return false;
  const auto held = g_hide_hold.find(IdOf(creature));
  if (held == g_hide_hold.end()) return false;
  if (GameMs() <= held->second) return true;
  g_hide_hold.erase(held);
  return false;
}

bool ThiefsReflexesOn(void* creature) {
  if (!creature) return false;
  EnsureRecords(creature);
  return engine::GetLocalInt(creature, kVarThiefOff) == 0;
}

void SetThiefsReflexes(void* creature, bool on) {
  if (!creature) return;
  EnsureRecords(creature);
  SetRecord(creature, kVarThiefOff, on ? 0 : 1);
  Log("actions: 0x%08X Thief's Reflexes %s", IdOf(creature), on ? "on" : "off");
}

void InstallExtraActions() {
  if (!Faithful()) return;
  RegisterRecords({kVarThiefOff});
  const bool uses = AddFeatUsesProvider(kVarSurgeSpent, &SurgeUsesTotal);
  AddShortRestListener(&SurgeShortRest);
  const bool scripts = AddScriptHandler("n5_actionsurge", &ActionSurgeScript) &&
                       AddScriptHandler("n5_ca_dash", &CunningDashScript) &&
                       AddScriptHandler("n5_ca_diseng", &CunningDisengageScript) &&
                       AddScriptHandler("n5_ca_hide", &CunningHideScript) &&
                       AddScriptHandler("n5_ca_hidestop", &HideStopScript) &&
                       AddScriptHandler("n5_steadyaim", &SteadyAimScript);
  const bool guard = AddFeatUseGuard(&SteadyAimGuard);
  HookExport("CNWSCreature::ToggleMode (out of stealth mode with an attack target)", kToggleMode,
             reinterpret_cast<void*>(&ToggleMode), reinterpret_cast<void**>(&g_toggle_mode));
  Log("actions: extra actions - Action Surge%s, Thief's Reflexes, Cunning Action, Steady Aim%s%s",
      uses ? "" : " (NWN's uses: no pool)", scripts ? "" : " - NOT available (no script hook)",
      guard ? "" : "; Steady Aim NOT refused after a move (no UseFeat guard)");
}
