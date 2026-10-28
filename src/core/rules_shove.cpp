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
//
// G1 (docs/PLAN.md "Grapple and Throw", owner 2026-10-27): the Shove's other
// choice, push 5 ft, is nwn5e's feat 1313 "Shove: push" (Knockdown stays the
// knock-Prone choice, named "Shove"). nwn5e's Unarmed Strike options (unarmed.h)
// are queued from UseFeat as the engine queues Knockdown and resolved here:
// no attack roll, no damage, the option's resolver. Every player character
// has both Shoves while the Shove is faithful (rules_features.cpp).

#include "unarmed.h"

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
#include "data_serving.h"
#include "ee_layout.h"
#include "engine.h"
#include "feat_use.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "resolve_damage.h"
#include "rules/class_features.h"
#include "rules/grapple.h"
#include "saves.h"
#include "skill_rank.h"

void ShoveSwing(uint32_t server_id);  // anim_layers.cpp: the player's next swing is nwstab (0.45.5)
int MartialArtsLevel(void* creature);  // rules_monk.cpp: Monk M1, Dexterous Attacks for the DC

namespace {

using AddSpecialAttackFn = void (*)(void* round, uint16_t id);
using TargetFn = void (*)(void* creature, void* target);
using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);
using AddAttackActionsFn = int (*)(void* creature, uint32_t target, int passive, int clear_first, int front);
using CreatureCheckFn = int (*)(void* creature);
struct Vector3 {
  float x, y, z;
};
using UseFeatFn = int (*)(void* creature, uint16_t feat, uint16_t subfeat, uint32_t target, uint32_t area,
                          Vector3* location);

AddSpecialAttackFn g_add_special_attack = nullptr;
CodeRange g_special_code;          // ResolveMeleeSpecialAttack
uintptr_t g_roll_caller = 0;       // its ResolveAttackRoll call
uintptr_t g_damage_caller = 0;     // its ResolveDamage call
uintptr_t g_post_damage_caller = 0;  // its ResolvePostMeleeDamage call
bool g_on = false;
int g_log_lines = 0;

struct Option {
  uint16_t feat;
  const char* name;
  UnarmedOptionResolver resolve;
  UnarmedOptionStart start;  // or null
};
std::vector<Option> g_options;  // filled at install, then only read

bool LogSlot() { return g_config.log_attacks && g_log_lines < 300 && ++g_log_lines; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
bool IsKnockdown(int special) { return special == ee::kFeatKnockdown || special == ee::kFeatImprovedKnockdown; }

const Option* OptionFor(int special) {
  for (const Option& option : g_options) {
    if (option.feat == special) return &option;
  }
  return nullptr;
}

int SpecialOf(void* creature) {
  const uint8_t* data = CurrentAttackData(creature);
  return data ? Field<uint16_t>(data, ee::kAttackSpecial) : 0;
}

bool KnockdownAttack(void* creature) { return IsKnockdown(SpecialOf(creature)); }

// A Shove or one of nwn5e's options: no attack roll, no damage.
bool UnarmedAttack(void* creature) {
  const int special = SpecialOf(creature);
  return IsKnockdown(special) || OptionFor(special);
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

// No attack roll: an automatic hit (d20 and modifier 0). A Shove goes on to
// the engine's size check and Discipline check - where its save decides; one
// of nwn5e's options is resolved here.
bool ShoveInsteadOfAttackRoll(AttackRoll& roll, uintptr_t caller) {
  if (caller != g_roll_caller || !roll.data) return false;
  const int special = Field<uint16_t>(roll.data, ee::kAttackSpecial);
  const Option* option = OptionFor(special);
  if (!option && !IsKnockdown(special)) return false;
  if (special == ee::kFeatImprovedKnockdown) {
    SetField<uint16_t>(roll.data, ee::kAttackSpecial, ee::kFeatKnockdown);  // 2024: at most one size larger
  }
  roll.data[ee::kAttackD20] = 0;
  SetField<int32_t>(roll.data, ee::kAttackModifier, 0);
  roll.data[ee::kAttackResult] = ee::kAttackResultAutomaticHit;
  if (roll.attacker && IsPlayer(roll.attacker)) ShoveSwing(IdOf(roll.attacker));  // swung as nwstab
  if (option && roll.attacker && roll.target) option->resolve(roll.attacker, roll.target);
  return true;
}

// No damage for a Shove or one of nwn5e's options (resolve_damage.cpp's skip).
bool ShoveSkipsResolveDamage(void* creature, void* /*target*/, uintptr_t caller) {
  return caller == g_damage_caller && UnarmedAttack(creature);
}

// The Discipline value the engine compares: the Shove's save decides.
int ShoveSave(const SkillQuery& query, int value) {
  if (query.skill != ee::kSkillDiscipline || !g_special_code.Contains(query.caller) || !query.vs) return value;
  void* attacker = CreatureById(IdOf(query.vs));
  void* target = CreatureOf(query.stats);
  if (!attacker || !target || !KnockdownAttack(attacker)) return value;
  const UnarmedSave save = RollUnarmedSave(attacker, target);
  if (!save.rolled) return value;
  ReportUnarmed(attacker, target, "Shove", save,
                save.failed ? "failed - knocked down" : save.immune ? "immune" : "made - stays up");
  return save.failed ? -127 : 127;
}

// --- Shove: push (G1) ---------------------------------------------------------

void ResolveShovePush(void* attacker, void* target) {
  void* creature = CreatureById(IdOf(target));
  if (!creature) {
    TellUnarmed(attacker, "nwn5e: Shove: push - only a creature can be shoved.");
    return;
  }
  const int shover_size = Field<int32_t>(attacker, ee::kCreatureSize);
  const int target_size = Field<int32_t>(creature, ee::kCreatureSize);
  if (!rules::ShoveSizeAllowed(shover_size, target_size)) {
    TellUnarmed(attacker, "nwn5e: Shove: push - too big to shove (at most one size larger than you).");
    if (LogSlot()) Log("shove: 0x%08X push on 0x%08X: too big (%d vs %d)", IdOf(attacker), IdOf(creature),
                       target_size, shover_size);
    return;
  }
  const UnarmedSave save = RollUnarmedSave(attacker, creature);
  if (!save.rolled) return;
  std::string outcome = save.immune ? "immune" : "made - stays put";
  if (save.failed) {
    float pushed = 0.0f;
    const std::string why = ShovePush(attacker, creature, rules::kShovePushMetres, &pushed);
    if (!why.empty()) {
      outcome = "failed - but it can't be pushed (" + why + ")";
    } else if (pushed >= rules::kShovePushMetres - 0.01f) {
      outcome = "failed - pushed 5 ft";
    } else {  // the line was cut short (0.5 m steps): something close behind it
      char text[96];
      sprintf_s(text, "failed - pushed only %.1f m (something close behind it)", pushed);
      outcome = text;
    }
  }
  ReportUnarmed(attacker, creature, "Shove: push", save, outcome);
}

// --- Queueing an option (UseFeat) ------------------------------------------------

// As UseFeat queues Knockdown (0x3a9df0): the attack on the target, then the
// option as a special attack - not with a ranged weapon (the engine's rule).
int UseUnarmedOption(void* creature, int feat, uint32_t target) {
  const Option* option = OptionFor(feat);
  if (!option) return -1;
  void* stats = StatsOf(creature);
  if (!stats || !engine::HasFeat(stats, feat)) return 0;
  static const auto add_attack_actions = reinterpret_cast<AddAttackActionsFn>(EngineExport(ee::kAddAttackActions));
  static const auto ranged = reinterpret_cast<CreatureCheckFn>(EngineExport(ee::kGetRangeWeaponEquipped));
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  if (!add_attack_actions || !ranged || !round || !CreatureById(target)) return 0;
  if (ranged(creature)) {
    TellUnarmed(creature, std::string("nwn5e: ") + option->name + " - not with a ranged weapon in hand.");
    return 0;
  }
  if (option->start) {
    const int started = option->start(creature, target);
    if (started >= 0) return started;
  }
  if (!add_attack_actions(creature, target, 0, 0, 0)) return 0;
  g_add_special_attack(round, option->feat);
  if (LogSlot()) Log("shove: 0x%08X queued %s on 0x%08X", IdOf(creature), option->name, target);
  return 1;
}

int UseFeatHandler(void* creature, int feat, int /*subfeat*/, uint32_t target) {
  return UseUnarmedOption(creature, feat, target);
}

// A player's Shove (Knockdown) with a ranged weapon in hand: the engine would
// queue a plain ranged attack (UseFeat adds no special attack then - owner's
// test of 0.45.0: "shove with a ranged weapon targets and fires a ranged
// attack"); refused with a word instead. Other creatures keep the engine's way
// (an AI refused here might stand idle).
bool RangedShoveGuard(void* creature, int feat) {
  if (!IsKnockdown(feat) || !IsPlayer(creature)) return true;
  static const auto ranged = reinterpret_cast<CreatureCheckFn>(EngineExport(ee::kGetRangeWeaponEquipped));
  if (!ranged || !ranged(creature)) return true;
  TellUnarmed(creature, "nwn5e: Shove - not with a ranged weapon in hand (a melee weapon or empty hands).");
  if (LogSlot()) Log("shove: 0x%08X Shove refused: a ranged weapon in hand", IdOf(creature));
  return false;
}

// !shove [push]: a Shove on the creature you are attacking (the radial's
// fallback): Knockdown's own use, or Shove: push.
std::vector<std::string> ShoveCommand(void* creature, const std::string& args) {
  const bool push = args.find("push") != std::string::npos;
  const uint32_t target = Field<uint32_t>(creature, ee::kCreatureAttackTarget);
  if (!CreatureById(target)) return {"nwn5e: !shove: attack a creature first - or use Shove on the radial."};
  static const auto use_feat = reinterpret_cast<UseFeatFn>(EngineExport(ee::kUseFeat));  // through the hook
  if (!use_feat) return {"nwn5e: !shove: no UseFeat in this game build."};
  Vector3 nowhere{0.0f, 0.0f, 0.0f};
  const int feat = push ? rules::kFeatShovePush : ee::kFeatKnockdown;
  if (!use_feat(creature, static_cast<uint16_t>(feat), 0, target, ee::kObjectInvalid, &nowhere)) {
    return {std::string("nwn5e: !shove: the ") + (push ? "push" : "Shove") + " couldn't start (you need the feat, a "
            "melee weapon or empty hands, and a hostile living target)."};
  }
  return {};
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

bool AddUnarmedOption(uint16_t feat, const char* name, UnarmedOptionResolver resolve, UnarmedOptionStart start) {
  if (!g_on || !resolve) {
    Log("SKIP %s: the Shove's hooks are off", name);
    return false;
  }
  g_options.push_back(Option{feat, name, resolve, start});
  return true;
}

UnarmedSave RollUnarmedSave(void* attacker, void* target) {
  UnarmedSave result;
  void* attacker_stats = attacker ? StatsOf(attacker) : nullptr;
  void* target_stats = target ? StatsOf(target) : nullptr;
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));  // through the hook
  if (!attacker_stats || !target_stats || !save_roll) return result;
  const uint32_t attacker_id = IdOf(attacker);
  const CreatureFacts facts = ReadCreatureFacts(attacker_stats);
  const int pb = rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating);
  const int strength = engine::AbilityModifier(attacker_stats, static_cast<int>(rules::Ability::kStrength));
  const int dexterity = engine::AbilityModifier(attacker_stats, static_cast<int>(rules::Ability::kDexterity));
  // Monk M1 (a) Dexterous Attacks: the DC may use Dexterity under Martial Arts (the better one).
  const bool dexterous = dexterity > strength && g_config.class_features == rules::RuleVariant::kFaithful &&
                         MartialArtsLevel(attacker) > 0;
  const int ability_modifier = dexterous ? rules::DexterousAbilityModifier(strength, dexterity) : strength;
  const int dc = rules::ShoveDc(ability_modifier, pb);
  const int str_save = SaveBonusFor(target_stats, rules::Ability::kStrength);
  const int dex_save = SaveBonusFor(target_stats, rules::Ability::kDexterity);
  const rules::Ability ability = rules::ShoveSaveAbility(str_save, dex_save);
  RememberSaveDc2024(attacker_id, dc);
  if (ability == rules::Ability::kStrength) SetForcedSaveAbility(rules::Ability::kStrength);
  const uint8_t save = save_roll(target, ee::kSaveReflex, static_cast<uint16_t>(dc), 0, attacker_id, 1, 0, 1);
  SetForcedSaveAbility(std::nullopt);
  result.rolled = true;
  result.failed = save == 0;
  result.immune = save == 2;
  result.dc = dc;
  char text[120];
  sprintf_s(text, "%s save %+d vs DC %d (8 + %s %+d + PB %+d)",
            ability == rules::Ability::kStrength ? "Strength" : "Dexterity",
            ability == rules::Ability::kStrength ? str_save : dex_save, dc, dexterous ? "Dex" : "Str",
            ability_modifier, pb);
  result.text = text;
  return result;
}

void ReportUnarmed(void* attacker, void* target, const char* option, const UnarmedSave& save,
                   const std::string& outcome) {
  const uint32_t attacker_id = IdOf(attacker);
  const uint32_t target_id = IdOf(target);
  const bool attacker_player = IsPlayer(attacker);
  const bool target_player = IsPlayer(target);
  const std::string line = std::string("nwn5e: ") + option + ": " + save.text + " " + outcome;
  if (attacker_player) TellPlayer(attacker_id, line);
  if (target_player && target_id != attacker_id) TellPlayer(target_id, line);
  if ((attacker_player || target_player) && LogSlot()) {
    Log("shove: 0x%08X %s on 0x%08X: no attack roll, no damage; %s %s", attacker_id, option, target_id,
        save.text.c_str(), outcome.c_str());
  }
}

void TellUnarmed(void* attacker, const std::string& line) {
  if (attacker && IsPlayer(attacker)) TellPlayer(IdOf(attacker), line);
}

// rules_retarget.cpp's ResolvePostMeleeDamage hook: a Shove's post-damage step is skipped.
bool ShoveSkipsDamage(void* creature, uintptr_t caller) {
  return g_on && caller == g_post_damage_caller && UnarmedAttack(creature);
}

void InstallShoveRules() {
  if (g_config.knockdown != rules::RuleVariant::kFaithful) return;
  HookExport("CNWSCombatRound::AddSpecialAttack (Disarm is an ordinary attack)", ee::kAddSpecialAttack,
             reinterpret_cast<void*>(&AddSpecialAttack), reinterpret_cast<void**>(&g_add_special_attack));
  g_special_code = FunctionAt(EngineExport(ee::kResolveMeleeSpecialAttack));
  g_roll_caller = SingleCallReturn(ee::kResolveAttackRoll, "ResolveAttackRoll");
  g_damage_caller = SingleCallReturn(ee::kResolveDamage, "ResolveDamage");
  g_post_damage_caller = SingleCallReturn(ee::kResolvePostMeleeDamage, "ResolvePostMeleeDamage");
  if (!g_roll_caller || !g_damage_caller || !g_post_damage_caller || !g_add_special_attack) return;
  g_on = AddAttackRollSkipper(&ShoveInsteadOfAttackRoll) &&
         AddResolveDamageSkip(&ShoveSkipsResolveDamage) &&
         AddSkillAdjuster(&ShoveSave);
  if (!g_on) {
    Log("knockdown: Shove NOT on");
    return;
  }
  ServeConditionalRows("shove");  // feat 23 named and described as the Shove (data/rows/feat.if_shove.txt)
  AddChatCommand("!shove", &ShoveCommand);
  // G1: Shove: push - nwn5e's own option, queued from UseFeat.
  if (!AddFeatUseGuard(&RangedShoveGuard)) Log("SKIP knockdown: a Shove with a ranged weapon fires it (no UseFeat hook)");
  const bool push = InstallPushRules() && AddFeatUseHandler(&UseFeatHandler) &&
                    AddUnarmedOption(static_cast<uint16_t>(rules::kFeatShovePush), "Shove: push", &ResolveShovePush);
  Log(push ? "knockdown: Knockdown is a 2024 Shove (faithful); Shove: push (feat 1313) pushes 5 ft"
           : "knockdown: Knockdown is a 2024 Shove (faithful); Shove: push is off (the push or UseFeat is missing)");
}
