// Conditions (PLAN.md "Conditions — decided by the owner", 2026-10-19; port
// of 1.69's rules_conditions.cpp; docs/EE-PORT.md E2). Each item has a
// faithful (2024) and an NWN-mode version, switched by the `conditions` key;
// the decisions are rules/conditions.h. Conditions are read from the
// creature's applied effects (any it holds — NWN keeps only the highest state
// number in one byte).
//
// - CN8 Deafened (faithful): EE adds 20% spell failure to any caster's verbal
//   spell while the caster can't hear (AIActionCastSpell, the AI-state byte's
//   hearing bit; 1.69 hit Bard / Sorcerer / Wizard only). Faithful sets the
//   bit for the cast of a deafened creature: no failure. NWN mode = EE's
//   (EP1).
// - CN1 Restrained (faithful): EE's OnApplyEntangled still adds its own -2
//   attack and -4 Dexterity sub-effects; while it runs, the attack / ability
//   decrease handlers refuse them (it still can't move). Being restrained
//   gives advantage to attacks against it and disadvantage on its attacks
//   (and on Dexterity saves — E3).
// - CN2/CN3: stunned, paralysed and sleeping targets give advantage (both
//   modes); a hit on a paralysed or sleeping creature from within 5 ft is a
//   critical hit (faithful; its critical immunity and crits_against_players
//   respected). Their automatic Strength / Dexterity save failures come with
//   the saves (E3).
// - CN4 Poisoned (faithful): disadvantage on its attack rolls while an NWN
//   poison effect is on it.
// - CN5 Frightened (faithful): NWN's fear state never takes hold — it makes
//   the creature flee, takes a player's control away and stops NWN's monster
//   AI from fighting. Instead the creature gets the fear icon for the
//   effect's duration, credited to the source (through the script bridge,
//   scheduled right after the effect is handled), and attacks with
//   disadvantage while it can see that source. Immunities stay NWN's.
// - Coup de grace (faithful): gone (2024 has none) — COUP_DE_GRACE_LEVEL_LIMIT
//   answers -1, so no level qualifies.
// - CN6 Concealment (faithful, with advantage_disadvantage): NWN's
//   concealment / miss-chance d100s (ResolveDefensiveEffects) always pass; a
//   target with a concealment effect (Blur, Displacement, ...) or Epic
//   Self-Concealment gives disadvantage instead (invisible and unseen
//   targets already do, B1).

#include <stdint.h>
#include <stdio.h>

#include "attack_roll.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "ruleset.h"

namespace {

using HandlerFn = int (*)(void* handler, void* object, void* effect, int loading);
using GetEffectImmunityFn = int (*)(void* stats, uint8_t type, void* versus, int consider_feats);
using CastSpellFn = uint32_t (*)(void* creature, void* action);

HandlerFn g_on_apply_set_state = nullptr;
HandlerFn g_on_apply_entangled = nullptr;
HandlerFn g_on_apply_attack_decrease = nullptr;
HandlerFn g_on_apply_ability_decrease = nullptr;
CastSpellFn g_ai_action_cast_spell = nullptr;
GetEffectImmunityFn g_get_effect_immunity = nullptr;
CodeRange g_defensive_effects_code;
bool g_concealment_roll_removed = false;
thread_local const void* t_entangling = nullptr;  // the object OnApplyEntangled is handling

constexpr float kFiveFeet = 1.524f;  // metres, edge to edge
constexpr int kEpicSelfConcealmentFirst = 0x2ec;  // feats 748-752 (Epic Self-Concealment 10-50%), as on 1.69
constexpr int kEpicSelfConcealmentLast = 0x2f0;
constexpr float kPermanentFearSeconds = 60.0f;
int g_log_lines = 0;

bool LogSlot() { return g_config.log_attacks && g_log_lines < 300 && ++g_log_lines; }
bool Faithful() { return g_config.conditions == rules::RuleVariant::kFaithful; }

bool Immune(void* creature, int type, void* versus) {
  void* stats = StatsOf(creature);
  if (!g_get_effect_immunity) g_get_effect_immunity = reinterpret_cast<GetEffectImmunityFn>(EngineExport(ee::kGetEffectImmunity));
  return stats && g_get_effect_immunity && g_get_effect_immunity(stats, static_cast<uint8_t>(type), versus, 1) != 0;
}

// CN5: the source of the fear nwn5e put on `creature` (its fear icon), or 0.
uint32_t FearSource(const void* creature) {
  uint32_t source = 0;
  effects::ForEachOfType(creature, ee::kEffectTypeIcon, [&](const void* effect) {
    if (effects::Integer(effect, 0) == ee::kIconFrightened) source = effects::Creator(effect);
  });
  return source;
}

// NWN's own refusals of a fear effect (the handler sends their feedback).
bool NwnRefusesFear(void* creature, const void* effect) {
  if (engine::IsDeadOrDying(creature)) return true;
  void* versus = CreatureById(effects::Creator(effect));
  if (Immune(creature, ee::kImmunityFear, versus)) return true;
  const bool bypass = (effects::Flags(effect) & 0x18) == 0x10 && effects::Integer(effect, 1) == 1;
  return Immune(creature, ee::kImmunityMindSpells, versus) && !bypass;
}

int OnApplySetState(void* handler, void* object, void* effect, int loading) {
  if (!Faithful() || !object || !effect || loading || effects::Integer(effect, 0) != ee::kStateFrightened) {
    return g_on_apply_set_state(handler, object, effect, loading);
  }
  const uint32_t id = Field<uint32_t>(object, ee::kObjectId);
  void* creature = CreatureById(id);
  if (!creature || NwnRefusesFear(creature, effect)) return g_on_apply_set_state(handler, object, effect, loading);
  const uint32_t source = effects::Creator(effect);
  const bool temporary = (effects::Flags(effect) & 0x7) == ee::kEffectDurationTemporary;
  const float seconds = temporary ? effects::Duration(effect) : kPermanentFearSeconds;
  // Scheduled (DelayCommand 0) by the source, so the icon is credited to it
  // and applied after the engine finishes with this effect.
  char script[512];
  sprintf_s(script,
            "object o = StringToObject(\"%x\"); if (!GetIsObjectValid(o)) return;"
            "DelayCommand(0.0, ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectIcon(%d), o, %.1f));"
            "if (GetIsPC(o)) SendMessageToPC(o, \"nwn5e: frightened for %.0f s - disadvantage on your attacks while "
            "you can see its source; you keep control\");",
            id, ee::kIconFrightened, seconds, seconds);
  RunScriptChunk(script, CreatureById(source) ? source : id);
  if (LogSlot()) {
    Log("conditions: 0x%08X frightened by 0x%08X for %.0f s (2024 fear; NWN's flee state not applied)", id, source,
        seconds);
  }
  return 1;  // NWN's frightened state is not kept
}

// CN1: OnApplyEntangled's own -2 attack / -4 Dexterity sub-effects are refused.
int OnApplyEntangled(void* handler, void* object, void* effect, int loading) {
  const void* outer = t_entangling;
  t_entangling = object;
  const int result = g_on_apply_entangled(handler, object, effect, loading);
  t_entangling = outer;
  return result;
}

int OnApplyAttackDecrease(void* handler, void* object, void* effect, int loading) {
  if (t_entangling && object == t_entangling) return 1;
  return g_on_apply_attack_decrease(handler, object, effect, loading);
}

int OnApplyAbilityDecrease(void* handler, void* object, void* effect, int loading) {
  if (t_entangling && object == t_entangling) return 1;
  return g_on_apply_ability_decrease(handler, object, effect, loading);
}

// CN8: a deafened caster's verbal spells don't fail (faithful).
uint32_t AIActionCastSpell(void* creature, void* action) {
  if (!creature || !effects::HasType(creature, ee::kEffectTypeDeaf)) return g_ai_action_cast_spell(creature, action);
  uint8_t& state = *reinterpret_cast<uint8_t*>(static_cast<uint8_t*>(creature) + ee::kCreatureAiState);
  const bool set_by_us = (state & ee::kAiStateCanHear) == 0;
  if (set_by_us) state |= ee::kAiStateCanHear;
  const uint32_t result = g_ai_action_cast_spell(creature, action);
  if (set_by_us) state &= static_cast<uint8_t>(~ee::kAiStateCanHear);
  return result;
}

// CN6: every miss-chance d100 of the concealment / miss-chance roll passes.
bool NoMissChance(uintptr_t caller, int count, int sides, int* result) {
  if (count != 1 || sides != 100 || !g_defensive_effects_code.Contains(caller)) return false;
  *result = 1000;
  return true;
}

// CN6: a concealment effect on `target` that applies to this attack (its
// melee / ranged filter and race filter; alignment-limited ones are left
// out), or Epic Self-Concealment.
bool TargetConcealed(void* attacker, void* target, bool ranged) {
  const int race = engine::RaceOf(StatsOf(attacker));
  bool concealed = false;
  effects::ForEachOfType(target, ee::kEffectTypeConcealment, [&](const void* effect) {
    if (effects::Integer(effect, 0) <= 0) return;
    const int effect_race = effects::Integer(effect, 1);
    if (effect_race != ee::kRacialTypeAll && effect_race != race) return;
    if (effects::Integer(effect, 2) != 0 || effects::Integer(effect, 3) != 0) return;
    const int kind = effects::Integer(effect, 4);  // 0 all, 2 ranged, 3 melee
    if ((kind == 2 && !ranged) || (kind == 3 && ranged)) return;
    concealed = true;
  });
  for (int feat = kEpicSelfConcealmentFirst; !concealed && feat <= kEpicSelfConcealmentLast; ++feat) {
    concealed = engine::HasFeat(StatsOf(target), feat);
  }
  return concealed;
}

void AddName(std::string* names, bool on, const char* name) {
  if (!on) return;
  if (!names->empty()) *names += ", ";
  *names += name;
}

// CN2 (faithful): after the attack roll, a hit on a paralysed or sleeping
// creature from within 5 ft becomes a critical hit.
void ForceCriticalOnHelpless(AttackRoll& roll) {
  if (!roll.data || roll.data[ee::kAttackResult] != 1) return;
  void* target = CreatureById(Field<uint32_t>(roll.target, ee::kObjectId));
  void* target_stats = StatsOf(target);
  if (!target_stats) return;
  const bool within_five_feet = engine::EdgeDistance(roll.attacker, target) <= kFiveFeet;
  if (!rules::ConditionForcesCritical(ReadConditions(target), within_five_feet, g_config.conditions)) return;
  const uint32_t attacker_id = Field<uint32_t>(roll.attacker, ee::kObjectId);
  const uint32_t target_id = Field<uint32_t>(target, ee::kObjectId);
  const char* refused = nullptr;
  if (Immune(target, ee::kImmunityCriticalHit, roll.attacker)) {
    refused = "immune to critical hits";
  } else if (!g_config.crits_against_players && ReadCreatureFacts(target_stats).character) {
    refused = "no critical hits on characters (crits_against_players=0)";
  }
  if (refused) {
    if (LogSlot()) Log("conditions: 0x%08X hit helpless 0x%08X: %s, stays a hit", attacker_id, target_id, refused);
    return;
  }
  roll.data[ee::kAttackResult] = ee::kAttackResultCritical;
  if (LogSlot()) {
    Log("conditions: 0x%08X hit 0x%08X (paralysed, asleep or down) from within 5 ft: critical hit", attacker_id,
        target_id);
  }
}

}  // namespace

uint32_t FearSourceOf(const void* creature) { return FearSource(creature); }

rules::Conditions ReadConditions(void* creature) {
  rules::Conditions conditions;
  if (!creature) return conditions;
  conditions.stunned = effects::HasState(creature, ee::kStateStunned);
  conditions.paralyzed = effects::HasState(creature, ee::kStateParalyzed);
  // A player character down at 0 HP (the engine's dying state: death saves,
  // rules_dying.cpp) is Unconscious too: attacks against it have advantage
  // and a hit from within 5 ft is a critical hit - two failed death saves
  // (owner, 2026-10-24: the 5b recommendations accepted).
  const int hit_points = Field<int16_t>(creature, ee::kObjectCurrentHitPoints);
  const bool player_down =
      Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0 && hit_points < 1 && hit_points > ee::kHitPointsDead;
  conditions.asleep = effects::HasState(creature, ee::kStateAsleep) || player_down;
  conditions.restrained = effects::HasType(creature, ee::kEffectTypeEntangle);
  conditions.poisoned = effects::HasType(creature, ee::kEffectTypePoison);
  return conditions;
}

bool IncapacitatedByCondition(void* creature) {
  return effects::HasState(creature, ee::kStateDazed) || effects::HasState(creature, ee::kStateStunned) ||
         effects::HasState(creature, ee::kStateParalyzed) || effects::HasState(creature, ee::kStateAsleep) ||
         effects::HasState(creature, ee::kStatePetrified);
}

ConditionRollSources ConditionAttackRollSources(void* attacker, void* target, bool ranged) {
  ConditionRollSources out;
  if (g_config.conditions == rules::RuleVariant::kOff || !attacker || !target) return out;
  const rules::Conditions attacker_conditions = ReadConditions(attacker);
  rules::Conditions attacker_frightened = attacker_conditions;
  const uint32_t fear_source = FearSource(attacker);
  attacker_frightened.frightened_source_in_sight = fear_source != 0 && engine::Sees(attacker, fear_source);
  rules::Conditions target_conditions = ReadConditions(target);
  target_conditions.concealed = g_concealment_roll_removed && TargetConcealed(attacker, target, ranged);
  const rules::RollSources sources =
      rules::ConditionAttackSources(attacker_frightened, target_conditions, g_config.conditions);
  out.advantage = sources.advantage;
  out.disadvantage = sources.disadvantage;
  const bool faithful = Faithful();
  AddName(&out.note, target_conditions.stunned, "target stunned");
  AddName(&out.note, target_conditions.paralyzed, "target paralysed");
  AddName(&out.note, target_conditions.asleep, "target asleep");
  AddName(&out.note, faithful && target_conditions.restrained, "target restrained");
  AddName(&out.note, faithful && target_conditions.concealed, "target concealed");
  AddName(&out.note, faithful && attacker_conditions.restrained, "attacker restrained");
  AddName(&out.note, faithful && attacker_conditions.poisoned, "attacker poisoned");
  AddName(&out.note, faithful && attacker_frightened.frightened_source_in_sight, "attacker frightened, source in sight");
  return out;
}

ConditionSave ConditionsForSave(void* creature, rules::Ability ability) {
  ConditionSave out;
  if (g_config.conditions == rules::RuleVariant::kOff || !creature) return out;
  const rules::Conditions conditions = ReadConditions(creature);
  out.auto_fail = rules::ConditionSaveOutcome(conditions, ability, g_config.conditions) == rules::SaveOutcome::kAutoFail;
  out.disadvantage = rules::ConditionSaveDisadvantage(conditions, ability, g_config.conditions);
  if (out.auto_fail) {
    out.note = conditions.stunned ? "stunned" : (conditions.paralyzed ? "paralysed" : "asleep");
  } else if (out.disadvantage > 0) {
    out.note = "restrained";
  }
  return out;
}

std::string CheckDisadvantage(void* creature) {
  if (!creature || !Faithful()) return std::string();
  if (g_config.debug_players_poisoned) {
    void* stats = StatsOf(creature);
    if (stats && ReadCreatureFacts(stats).is_player) return "poisoned (test switch)";
  }
  if (ReadConditions(creature).poisoned) return "poisoned";
  const uint32_t source = FearSource(creature);
  if (source != 0 && engine::Sees(creature, source)) return "frightened";
  return std::string();
}

void InstallConditionRules() {
  if (!Faithful()) return;
  HookExport("CNWSEffectListHandler::OnApplySetState (CN5 2024 fear)", ee::kOnApplySetState,
             reinterpret_cast<void*>(&OnApplySetState), reinterpret_cast<void**>(&g_on_apply_set_state));
  HookExport("CNWSEffectListHandler::OnApplyEntangled (CN1: no -2 attack / -4 Dex)", ee::kOnApplyEntangled,
             reinterpret_cast<void*>(&OnApplyEntangled), reinterpret_cast<void**>(&g_on_apply_entangled));
  HookExport("CNWSEffectListHandler::OnApplyAttackDecrease (CN1)", ee::kOnApplyAttackDecrease,
             reinterpret_cast<void*>(&OnApplyAttackDecrease), reinterpret_cast<void**>(&g_on_apply_attack_decrease));
  HookExport("CNWSEffectListHandler::OnApplyAbilityDecrease (CN1)", ee::kOnApplyAbilityDecrease,
             reinterpret_cast<void*>(&OnApplyAbilityDecrease), reinterpret_cast<void**>(&g_on_apply_ability_decrease));
  HookExport("CNWSCreature::AIActionCastSpell (CN8: deafness adds no spell failure)", ee::kAIActionCastSpell,
             reinterpret_cast<void*>(&AIActionCastSpell), reinterpret_cast<void**>(&g_ai_action_cast_spell));
  AnswerRulesetInt("COUP_DE_GRACE_LEVEL_LIMIT", "conditions: no coup de grace (2024)", [](int) { return -1; });
  AddAttackRollListeners(nullptr, &ForceCriticalOnHelpless);
  // CN6 replaces the miss chance with disadvantage, so it needs the
  // advantage / disadvantage rule.
  if (g_config.advantage_disadvantage) {
    g_defensive_effects_code = FunctionAt(EngineExport(ee::kResolveDefensiveEffects));
    g_concealment_roll_removed = g_defensive_effects_code.Valid() && AddDiceFilter(&NoMissChance);
    Log(g_concealment_roll_removed ? "conditions: no concealment / miss chance roll (faithful)"
                                   : "SKIP conditions CN6: ResolveDefensiveEffects' code range is unknown");
  } else {
    Log("conditions: concealment keeps NWN's miss chance (advantage_disadvantage=0)");
  }
}
