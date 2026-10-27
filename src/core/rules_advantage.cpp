// Phase 1b (PLAN.md B1-B6, decided 2026-10-18): 2024 advantage /
// disadvantage replace NWN's situational attack modifiers (port of 1.69's
// rules_advantage.cpp; docs/EE-PORT.md E1).
//
// In EE every situational term of GetAttackModifierVersus is a ruleset value
// that the engine reads only when its condition holds (blind or stunned or
// prone target, invisible attacker, unseen or moving target, flanking, a
// ranged attack in the target's reach). So:
// - the ruleset hook answers each of them 0 — no flat 3e bonus — and, while
//   nwn5e asks the engine's own GetAttackModifierVersus before the attack
//   (the probe), records which ones the engine applied: the engine's checks
//   decide the situation, as on 1.69;
// - ResolveAttackRoll rolls its d20 first, so the probe runs in the attack
//   roll's before-listener, adds the 2024 close-combat check (B5: a hostile
//   creature within 5 ft of a ranged attacker that sees it and is not dead or
//   dying; Point Blank Shot cancels it, B3), and the first d20 the attack roll
//   rolls is rolled twice when the mode says so (kept die in the log).
// Conditions (CN), weapon mastery (Vex, Sap), the Fighter's Studied Attacks,
// the Heavy property (size) and Halfling Luck add to the mode.

#include <stdint.h>

#include <string>

#include "attack_roll.h"
#include "condition_effects.h"
#include "mastery.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "engine.h"
#include "log.h"
#include "rules/nwn_situational.h"
#include "ruleset.h"
#include "weapon_size.h"

int EngineAttackModifier(void* stats, void* target);  // rules_attack.cpp
MasteryRollSources TakeStudiedAttacks(void* attacker, uint32_t target_id);  // rules_fighter.cpp (Fighter F6)
// rules_barbarian_strikes.cpp (Barbarian B6, B10): Reckless Attack's Advantage; Brutal Strike giving it up.
MasteryRollSources RecklessRollSources(void* attacker, void* target, const uint8_t* data);
bool BrutalStrikeOnRoll(void* attacker, void* target, const uint8_t* data, rules::RollMode* mode, std::string* note);
// rules_grapple.cpp (G2): a grappled creature's attack on anyone but its grappler.
MasteryRollSources GrappleRollSources(void* attacker, void* target);

namespace {

constexpr float kFiveFeet = 1.524f;  // metres, edge to edge
constexpr int kLogLimit = 400;
int g_log_lines = 0;

struct PendingAttack {
  bool active = false;
  bool d20_pending = false;
  rules::NwnSituationalTerms terms;
  rules::RollMode mode = rules::RollMode::kNormal;
  int threats = 0;
  bool point_blank_shot = false;
  std::string conditions;  // CN note for the log
  uint32_t attacker_id = 0;
  uint32_t target_id = 0;
};
thread_local PendingAttack t_pending;
thread_local rules::NwnSituationalTerms* t_probe = nullptr;  // set while the probe asks the engine

CodeRange g_attack_roll_code;

bool LogSlotLeft() { return g_config.log_attacks && g_log_lines < kLogLimit && ++g_log_lines; }

const char* ModeName(rules::RollMode mode) {
  switch (mode) {
    case rules::RollMode::kAdvantage:
      return "advantage";
    case rules::RollMode::kDisadvantage:
      return "disadvantage";
    default:
      return "normal";
  }
}

std::string TermNames(const rules::NwnSituationalTerms& t) {
  std::string names;
  const auto add = [&](bool on, const char* name) {
    if (!on) return;
    if (!names.empty()) names += ", ";
    names += name;
  };
  add(t.target_blind, "target blind");
  add(t.attacker_invisible, "attacker invisible");
  add(t.cant_see_target, "target unseen");
  add(t.target_moving, "target moving");
  add(t.target_prone, "target prone");
  add(t.target_stunned, "target stunned");
  add(t.target_flanked, "target flanked");
  add(t.ranged_in_melee, "3e ranged in reach");
  return names.empty() ? "none" : names;
}

// B5: hostile creatures within 5 ft of the attacker that see it and can act.
struct ThreatCount {
  void* attacker;
  uint32_t attacker_id;
  int threats;
};

void CountIfThreat(void* context, uint32_t id, void* creature) {
  auto* count = static_cast<ThreatCount*>(context);
  if (!engine::IsEnemy(count->attacker, id) || engine::IsDeadOrDying(creature) || IncapacitatedByCondition(creature)) {
    return;
  }
  if (engine::EdgeDistance(count->attacker, creature) > kFiveFeet) return;
  if (!engine::Sees(creature, count->attacker_id)) return;
  ++count->threats;
}

int CountCloseCombatThreats(void* attacker) {
  ThreatCount count{attacker, Field<uint32_t>(attacker, ee::kObjectId), 0};
  engine::ForEachOtherCreatureInArea(attacker, &CountIfThreat, &count);
  return count.threats;
}

void BeforeAttackRoll(AttackRoll& roll) {
  t_pending = PendingAttack();
  void* stats = StatsOf(roll.attacker);
  void* target = CreatureById(Field<uint32_t>(roll.target, ee::kObjectId));
  if (!stats || !target || !roll.data) return;  // doors and placeables: no situational terms
  rules::NwnSituationalTerms terms;
  terms.ranged = Field<int32_t>(roll.data, ee::kAttackRanged) != 0;
  t_probe = &terms;
  EngineAttackModifier(stats, target);
  t_probe = nullptr;
  t_pending.terms = terms;
  t_pending.point_blank_shot = terms.ranged && engine::HasFeat(stats, ee::kFeatPointBlankShot);
  t_pending.threats = terms.ranged ? CountCloseCombatThreats(roll.attacker) : 0;
  const ConditionRollSources conditions = ConditionAttackRollSources(roll.attacker, target, terms.ranged);
  const MasteryRollSources mastery = TakeMasteryRollSources(roll.attacker, Field<uint32_t>(target, ee::kObjectId));
  const MasteryRollSources studied = TakeStudiedAttacks(roll.attacker, Field<uint32_t>(target, ee::kObjectId));
  const MasteryRollSources reckless = RecklessRollSources(roll.attacker, target, roll.data);
  const MasteryRollSources grappled = GrappleRollSources(roll.attacker, target);
  t_pending.conditions = conditions.note.empty() ? "" : "conditions: " + conditions.note;
  if (!mastery.note.empty()) {
    t_pending.conditions += (t_pending.conditions.empty() ? "mastery: " : "; mastery: ") + mastery.note;
  }
  if (!studied.note.empty()) {
    t_pending.conditions += (t_pending.conditions.empty() ? "fighter: " : "; fighter: ") + studied.note;
  }
  if (!reckless.note.empty()) {
    t_pending.conditions += (t_pending.conditions.empty() ? "barbarian: " : "; barbarian: ") + reckless.note;
  }
  if (!grappled.note.empty()) t_pending.conditions += (t_pending.conditions.empty() ? "" : "; ") + grappled.note;
  std::string heavy_note;
  const int heavy = HeavyWeaponDisadvantage(roll.attacker, terms.ranged, &heavy_note);
  if (heavy) t_pending.conditions += (t_pending.conditions.empty() ? "" : "; ") + heavy_note;
  t_pending.mode = rules::SituationalRollMode(
      terms, rules::RangedCloseCombat(terms.ranged, t_pending.point_blank_shot, t_pending.threats),
      conditions.advantage + mastery.advantage + studied.advantage + reckless.advantage,
      conditions.disadvantage + mastery.disadvantage + heavy + grappled.disadvantage);
  {
    std::string brutal;
    if (BrutalStrikeOnRoll(roll.attacker, target, roll.data, &t_pending.mode, &brutal)) {
      t_pending.conditions += (t_pending.conditions.empty() ? "" : "; ") + brutal;
    }
  }
  t_pending.attacker_id = Field<uint32_t>(roll.attacker, ee::kObjectId);
  t_pending.target_id = Field<uint32_t>(target, ee::kObjectId);
  t_pending.active = true;
  t_pending.d20_pending = true;
}

// The first d20 ResolveAttackRoll rolls is the attack roll (+0x6f).
bool RollAttackD20(uintptr_t caller, int count, int sides, int* result) {
  if (!t_pending.active || !t_pending.d20_pending || count != 1 || sides != 20 ||
      !g_attack_roll_code.Contains(caller)) {
    return false;
  }
  t_pending.d20_pending = false;
  if (t_pending.mode == rules::RollMode::kNormal) return false;
  const int first = RollEngineDice(1, 20);
  const int second = RollEngineDice(1, 20);
  const bool keep_higher = t_pending.mode == rules::RollMode::kAdvantage;
  const int kept = keep_higher ? (first > second ? first : second) : (first < second ? first : second);
  if (LogSlotLeft()) {
    Log("advantage: 0x%08X vs 0x%08X (%s): %s; 3e situational terms removed (%s); close-combat enemies %d%s%s%s; "
        "d20 %d / %d -> %d",
        t_pending.attacker_id, t_pending.target_id, t_pending.terms.ranged ? "ranged" : "melee",
        ModeName(t_pending.mode), TermNames(t_pending.terms).c_str(), t_pending.threats,
        t_pending.point_blank_shot ? " (Point Blank Shot)" : "", t_pending.conditions.empty() ? "" : "; ",
        t_pending.conditions.c_str(), first, second, kept);
  }
  *result = kept;
  return true;
}

void AfterAttackRoll(AttackRoll&) {
  if (t_pending.active && t_pending.mode == rules::RollMode::kNormal &&
      (rules::NwnSituationalTotal(t_pending.terms) != 0 || t_pending.threats > 0 || !t_pending.conditions.empty()) &&
      LogSlotLeft()) {
    Log("advantage: 0x%08X vs 0x%08X (%s): 3e situational terms removed (%s); close-combat enemies %d%s%s%s; normal "
        "roll",
        t_pending.attacker_id, t_pending.target_id, t_pending.terms.ranged ? "ranged" : "melee",
        TermNames(t_pending.terms).c_str(), t_pending.threats, t_pending.point_blank_shot ? " (Point Blank Shot)" : "",
        t_pending.conditions.empty() ? "" : "; ", t_pending.conditions.c_str());
  }
  t_pending = PendingAttack();
}

// Each situational ruleset value: recorded during the probe, 0 always.
template <bool rules::NwnSituationalTerms::*kTerm>
int Situational(int) {
  if (t_probe) t_probe->*kTerm = true;
  return 0;
}

int ProneTarget(int) {
  if (t_probe) t_probe->target_prone = true;
  return 0;
}

}  // namespace

// SK10 (rules_spell_attack.cpp): the d20 of a spell attack with the roll mode
// an attack would have — NWN's situational terms as advantage /
// disadvantage (asked of the engine as for weapon attacks; the melee / ranged
// kind is the spell's, not the caster's current attack's), the 2024
// close-combat check for ranged, conditions (concealment included) — and
// Halfling Luck. `note` gets the reasons for the log.
int RollSpellAttackD20(void* caster, void* target, bool ranged, std::string* note) {
  rules::RollMode mode = rules::RollMode::kNormal;
  void* stats = caster ? StatsOf(caster) : nullptr;
  if (stats && target && g_config.advantage_disadvantage && Field<void*>(caster, ee::kCreatureCombatRound)) {
    rules::NwnSituationalTerms terms;
    terms.ranged = ranged;
    t_probe = &terms;
    EngineAttackModifier(stats, target);
    t_probe = nullptr;
    terms.ranged = ranged;
    if (ranged) {
      terms.target_flanked = false;
    } else {
      terms.ranged_in_melee = false;
    }
    const bool point_blank_shot = ranged && engine::HasFeat(stats, ee::kFeatPointBlankShot);
    const int threats = ranged ? CountCloseCombatThreats(caster) : 0;
    const ConditionRollSources conditions = ConditionAttackRollSources(caster, target, ranged);
    mode = rules::SituationalRollMode(terms, rules::RangedCloseCombat(ranged, point_blank_shot, threats),
                                      conditions.advantage, conditions.disadvantage);
    if (note) {
      *note = ModeName(mode);
      if (rules::NwnSituationalTotal(terms) != 0) *note += std::string(" (") + TermNames(terms) + ")";
      if (threats > 0) *note += "; close-combat enemies " + std::to_string(threats);
      if (!conditions.note.empty()) *note += "; conditions: " + conditions.note;
    }
  }
  int kept = RollEngineDice(1, 20);
  if (mode != rules::RollMode::kNormal) {
    const int second = RollEngineDice(1, 20);
    if (mode == rules::RollMode::kAdvantage ? second > kept : second < kept) kept = second;
  }
  if (kept == 1 && engine::RaceOf(stats) == ee::kRaceHalfling) {
    kept = RollEngineDice(1, 20);  // Halfling Luck
    if (note) *note += "; Halfling Luck rerolled a 1";
  }
  return kept;
}

void InstallAdvantageRules() {
  if (!g_config.advantage_disadvantage) return;
  g_attack_roll_code = ResolveAttackRollCode();
  if (!g_attack_roll_code.Valid()) {
    Log("SKIP advantage_disadvantage: ResolveAttackRoll's code range is unknown");
    return;
  }
  const char* why = "advantage_disadvantage: a 2024 roll mode instead";
  using Terms = rules::NwnSituationalTerms;
  AnswerRulesetInt("BLIND_TARGET_BONUS", why, &Situational<&Terms::target_blind>);
  AnswerRulesetInt("INVISIBILITY_ATTACK_BONUS", why, &Situational<&Terms::attacker_invisible>);
  AnswerRulesetInt("NOSIGHT_TARGET_PENALTY", why, &Situational<&Terms::cant_see_target>);
  AnswerRulesetInt("MOVING_TARGET_PENALTY", why, &Situational<&Terms::target_moving>);
  AnswerRulesetInt("PRONE_MELEE_TARGET_BONUS", why, &ProneTarget);
  AnswerRulesetInt("PRONE_RANGED_TARGET_BONUS", why, &ProneTarget);
  AnswerRulesetInt("STUNNED_TARGET_BONUS", why, &Situational<&Terms::target_stunned>);
  AnswerRulesetInt("FLANK_ATTACK_BONUS", why, &Situational<&Terms::target_flanked>);
  AnswerRulesetInt("RANGED_ATTACK_IN_MELEE_RANGE", why, &Situational<&Terms::ranged_in_melee>);
  if (AddAttackRollListeners(&BeforeAttackRoll, &AfterAttackRoll) && AddDiceFilter(&RollAttackD20)) {
    Log("advantage_disadvantage: situational terms read from the engine, the attack d20 rolled per 2024");
  }
}
