// SK6 (port of 1.69's rules_shove.cpp; docs/EE-PORT.md E3c), `knockdown`
// faithful: Knockdown is a 2024 Shove and Disarm does nothing.
//
// NWN: Knockdown / Improved Knockdown (feats 23 / 17) is an attack at -4; on a
// hit (with its damage) the target, at most one size larger than the attacker
// (two with Improved), rolls d20 + Discipline against the attack total and is
// knocked down on a loss. Disarm / Improved Disarm (9 / 16) is an attack at
// -6 / -4 that provokes; on a hit the same Discipline check decides.
//
// Faithful: a Knockdown attack makes no attack roll and deals no damage (the
// attack becomes an "automatic hit" in place of ResolveAttackRoll, and the
// special path's damage calls are skipped for it); the target, at most one
// size larger than the attacker for both feats (Improved Knockdown is handled
// as Knockdown for that check), makes the better of its Strength or Dexterity
// save against 8 + the attacker's Strength modifier + PB (the engine's save
// roll, so the combat log shows it) and is knocked down by the engine's own
// effect on a failure: the Discipline value the engine compares is answered
// -127 (down) or +127 (resisted). Disarm is never added as a special attack,
// so the attack is an ordinary one (no penalty, no opportunity attack, no
// disarm). NWN mode: NWN's.

#include <stdint.h>
#include <stdio.h>

#include <intrin.h>

#include <optional>
#include <string>
#include <vector>

#include "attack_roll.h"
#include "chat.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "saves.h"
#include "skill_rank.h"

namespace {

using AddSpecialAttackFn = void (*)(void* round, uint16_t id);
using TargetFn = void (*)(void* creature, void* target);
using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);

AddSpecialAttackFn g_add_special_attack = nullptr;
TargetFn g_resolve_damage = nullptr;
CodeRange g_special_code;          // ResolveMeleeSpecialAttack
uintptr_t g_roll_caller = 0;       // its ResolveAttackRoll call
uintptr_t g_damage_caller = 0;     // its ResolveDamage call
uintptr_t g_post_damage_caller = 0;  // its ResolvePostMeleeDamage call
bool g_on = false;
int g_log_lines = 0;

bool LogSlot() { return g_config.log_attacks && g_log_lines < 300 && ++g_log_lines; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
bool IsKnockdown(int special) { return special == ee::kFeatKnockdown || special == ee::kFeatImprovedKnockdown; }

bool KnockdownAttack(void* creature) {
  const uint8_t* data = CurrentAttackData(creature);
  return data && IsKnockdown(Field<uint16_t>(data, ee::kAttackSpecial));
}

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

// Disarm is not added: the action stays an ordinary attack.
void AddSpecialAttack(void* round, uint16_t id) {
  if (id == ee::kFeatDisarm || id == ee::kFeatImprovedDisarm) {
    if (LogSlot()) Log("disarm: an ordinary attack (faithful)");
    return;
  }
  g_add_special_attack(round, id);
}

// No attack roll: an automatic hit (d20 and modifier 0), so the engine goes on
// to its size check and Discipline check — where the Shove's save decides.
bool ShoveInsteadOfAttackRoll(AttackRoll& roll, uintptr_t caller) {
  if (caller != g_roll_caller || !roll.data) return false;
  const int special = Field<uint16_t>(roll.data, ee::kAttackSpecial);
  if (!IsKnockdown(special)) return false;
  if (special == ee::kFeatImprovedKnockdown) {
    SetField<uint16_t>(roll.data, ee::kAttackSpecial, ee::kFeatKnockdown);  // 2024: at most one size larger
  }
  roll.data[ee::kAttackD20] = 0;
  SetField<int32_t>(roll.data, ee::kAttackModifier, 0);
  roll.data[ee::kAttackResult] = ee::kAttackResultAutomaticHit;
  return true;
}

void ResolveDamage(void* creature, void* target) {
  if (reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_damage_caller && KnockdownAttack(creature)) return;
  g_resolve_damage(creature, target);
}

// The Discipline value the engine compares: the Shove's save decides.
int ShoveSave(const SkillQuery& query, int value) {
  if (query.skill != ee::kSkillDiscipline || !g_special_code.Contains(query.caller) || !query.vs) return value;
  void* attacker = CreatureById(IdOf(query.vs));
  void* target = CreatureOf(query.stats);
  if (!attacker || !target || !KnockdownAttack(attacker)) return value;
  void* attacker_stats = StatsOf(attacker);
  if (!attacker_stats) return value;
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));  // through the hook
  if (!save_roll) return value;
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t target_id = IdOf(target);
  const CreatureFacts facts = ReadCreatureFacts(attacker_stats);
  const int pb = rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating);
  const int strength = engine::AbilityModifier(attacker_stats, static_cast<int>(rules::Ability::kStrength));
  const int dc = rules::ShoveDc(strength, pb);
  const int str_save = SaveBonusFor(query.stats, rules::Ability::kStrength);
  const int dex_save = SaveBonusFor(query.stats, rules::Ability::kDexterity);
  const rules::Ability ability = rules::ShoveSaveAbility(str_save, dex_save);
  RememberSaveDc2024(attacker_id, dc);
  if (ability == rules::Ability::kStrength) SetForcedSaveAbility(rules::Ability::kStrength);
  const uint8_t save = save_roll(target, ee::kSaveReflex, static_cast<uint16_t>(dc), 0, attacker_id, 1, 0, 1);
  SetForcedSaveAbility(std::nullopt);
  const char* ability_name = ability == rules::Ability::kStrength ? "Strength" : "Dexterity";
  const char* outcome = save == 0 ? "failed - knocked down" : save == 2 ? "immune" : "made - stays up";
  const bool attacker_player = IsPlayer(attacker);
  const bool target_player = IsPlayer(target);
  if (attacker_player || target_player) {
    char line[200];
    sprintf_s(line, "nwn5e: Shove: %s save %+d vs DC %d (8 + Str %+d + PB %+d) %s", ability_name,
              ability == rules::Ability::kStrength ? str_save : dex_save, dc, strength, pb, outcome);
    if (attacker_player) TellPlayer(attacker_id, line);
    if (target_player && target_id != attacker_id) TellPlayer(target_id, line);
    if (LogSlot()) {
      Log("shove: 0x%08X on 0x%08X: no attack roll, no damage; %s save (Str %+d, Dex %+d) vs DC %d: %s", attacker_id,
          target_id, ability_name, str_save, dex_save, dc, outcome);
    }
  }
  return save == 0 ? -127 : 127;
}

uintptr_t SingleCallReturn(const char* target_export, const char* what) {
  const std::vector<uintptr_t> calls = FindCallsTo(g_special_code, EngineExport(target_export));
  if (calls.size() != 1) {
    Log("SKIP knockdown: %zu %s calls in ResolveMeleeSpecialAttack (expected 1)", calls.size(), what);
    return 0;
  }
  return calls[0] + kCallLength;
}

}  // namespace

// rules_retarget.cpp's ResolvePostMeleeDamage hook: a Shove's post-damage step is skipped.
bool ShoveSkipsDamage(void* creature, uintptr_t caller) {
  return g_on && caller == g_post_damage_caller && KnockdownAttack(creature);
}

void InstallShoveRules() {
  if (g_config.knockdown != rules::RuleVariant::kFaithful) return;
  HookExport("CNWSCombatRound::AddSpecialAttack (Disarm is an ordinary attack)", ee::kAddSpecialAttack,
             reinterpret_cast<void*>(&AddSpecialAttack), reinterpret_cast<void**>(&g_add_special_attack));
  g_special_code = FunctionAt(EngineExport(ee::kResolveMeleeSpecialAttack));
  g_roll_caller = SingleCallReturn(ee::kResolveAttackRoll, "ResolveAttackRoll");
  g_damage_caller = SingleCallReturn(ee::kResolveDamage, "ResolveDamage");
  g_post_damage_caller = SingleCallReturn(ee::kResolvePostMeleeDamage, "ResolvePostMeleeDamage");
  if (!g_roll_caller || !g_damage_caller || !g_post_damage_caller) return;
  g_on = AddAttackRollSkipper(&ShoveInsteadOfAttackRoll) &&
         HookExport("CNWSCreature::ResolveDamage (a Shove deals no damage)", ee::kResolveDamage,
                    reinterpret_cast<void*>(&ResolveDamage), reinterpret_cast<void**>(&g_resolve_damage)) &&
         AddSkillAdjuster(&ShoveSave);
  Log(g_on ? "knockdown: Knockdown is a 2024 Shove (faithful)" : "knockdown: Shove NOT on");
}
