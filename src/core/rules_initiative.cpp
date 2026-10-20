// SK9 (port of 1.69's rules_initiative.cpp; docs/EE-PORT.md E3b), `initiative`
// faithful: initiative is a 2024 Dexterity check. EE's ResolveInitiative rolls
// d20 + Dex modifier + EPIC_SUPERIOR_INITIATIVE_BONUS (8) or else
// IMPROVED_INITIATIVE_BONUS (4), + BLOODED / THUG_INITIATIVE_BONUS (2 each).
// Faithful: Improved Initiative (2024's Alert) and Epic Superior Initiative
// add the proficiency bonus instead, Blooded and Thug nothing — the four
// ruleset values answered while a creature's initiative is rolled (each is
// read only there). The d20 has advantage while the creature is invisible
// (2024's Invisible condition), disadvantage when it is poisoned or frightened
// (SK11), and Halfling Luck rerolls a natural 1; hidden (SK5: hiding with a
// hide that held) counts as invisible; Alert from a background (CM5: the
// Criminal's Origin feat) is read as Improved Initiative while initiative is
// rolled. Remarkable Athlete comes with the Fighter. NWN mode: NWN's.

#include <stdint.h>

#include <string>

#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "engine.h"
#include "feats.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "ruleset.h"
#include "rules/skills.h"

bool HiddenByStealth(void* creature);         // rules_perception.cpp (SK5)
bool HasAlertFromBackground(void* creature);  // rules_abilities.cpp (CM5)

namespace {

using ResolveInitiativeFn = void (*)(void* creature);
using GetInvisibleFn = int (*)(void* creature, void* object, int flag);

ResolveInitiativeFn g_resolve_initiative = nullptr;
GetInvisibleFn g_get_invisible = nullptr;
uintptr_t g_d20_caller = 0;
CodeRange g_initiative_code;
thread_local void* t_creature = nullptr;  // whose initiative is being rolled
LogOnce g_logged(300);
int g_roll_lines = 0;

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

int ProficiencyBonus(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return -1;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return -1;
  return rules::ProficiencyBonusForCreature(facts.character, facts.level, facts.challenge_rating);
}

// Improved / Epic Superior Initiative: the proficiency bonus (Alert).
int AlertBonus(int engine_value) {
  const int pb = t_creature ? ProficiencyBonus(t_creature) : -1;
  if (pb < 0) return engine_value;
  void* stats = StatsOf(t_creature);
  if (g_config.log_attacks && ReadCreatureFacts(stats).is_player &&
      g_logged.First(LogOnce::Key(IdOf(t_creature), engine_value, pb))) {
    Log("initiative: object 0x%08X Improved Initiative %+d -> PB %+d (Alert)", IdOf(t_creature), engine_value, pb);
  }
  return pb;
}

int NoBonus(int) { return 0; }

// CM5: Alert from the Criminal background counts as Improved Initiative while
// the creature's initiative is rolled.
bool AlertFromBackground(void* stats, int feat, uintptr_t caller, int* answer) {
  if (feat != ee::kFeatImprovedInitiative || !t_creature || StatsOf(t_creature) != stats ||
      !g_initiative_code.Contains(caller) || !HasAlertFromBackground(t_creature)) {
    return false;
  }
  *answer = 1;
  return true;
}

void ResolveInitiative(void* creature) {
  void* outer = t_creature;
  t_creature = creature;
  g_resolve_initiative(creature);
  t_creature = outer;
}

bool Invisible(void* creature) { return g_get_invisible && g_get_invisible(creature, creature, 0) != 0; }

// The initiative d20.
bool InitiativeD20(uintptr_t caller, int count, int sides, int* result) {
  if (caller != g_d20_caller || !t_creature || count != 1 || sides != 20) return false;
  void* creature = t_creature;
  const bool invisible = Invisible(creature);
  const bool hidden = !invisible && HiddenByStealth(creature);
  const std::string worse = CheckDisadvantage(creature);  // SK11
  const int advantage = invisible || hidden ? 1 : 0;
  const int disadvantage = worse.empty() ? 0 : 1;
  const int first = RollEngineDice(1, 20);
  const int second = advantage || disadvantage ? RollEngineDice(1, 20) : 0;
  int kept = rules::KeptCheckD20(first, second, advantage, disadvantage);
  void* stats = StatsOf(creature);
  int luck = 0;
  if (kept == 1 && engine::RaceOf(stats) == ee::kRaceHalfling) {
    luck = RollEngineDice(1, 20);  // Halfling Luck: a natural 1 on a d20 test is rerolled once
    kept = luck;
  }
  if (g_config.log_attacks && (second || luck) && g_roll_lines < 200 && stats && ReadCreatureFacts(stats).is_player) {
    ++g_roll_lines;
    std::string why = invisible ? "advantage: invisible" : hidden ? "advantage: hidden" : "";
    if (disadvantage) why += (why.empty() ? "disadvantage: " : "; disadvantage: ") + worse;
    if (advantage && disadvantage) why += " - they cancel";
    if (luck) why += why.empty() ? "Halfling Luck rerolled a 1" : "; Halfling Luck rerolled a 1";
    Log("initiative: 0x%08X d20 %d%s%s -> %d (%s)", IdOf(creature), first, second ? " / " : "",
        second ? std::to_string(second).c_str() : "", kept, why.c_str());
  }
  *result = kept;
  return true;
}

}  // namespace

void InstallInitiativeRules() {
  if (g_config.initiative != rules::RuleVariant::kFaithful) return;
  const uintptr_t function = EngineExport(ee::kResolveInitiative);
  const std::vector<uintptr_t> d20 = FindCallsTo(FunctionAt(function), EngineExport(ee::kRollDice));
  if (!function || d20.size() != 1) {
    Log("SKIP initiative: ResolveInitiative not found or not one RollDice in it (%zu)", d20.size());
    return;
  }
  g_d20_caller = d20[0] + kCallLength;
  g_initiative_code = FunctionAt(function);
  g_get_invisible = reinterpret_cast<GetInvisibleFn>(EngineExport(ee::kGetInvisible));
  if (!HookExport("CNWSCreature::ResolveInitiative (2024 initiative)", ee::kResolveInitiative,
                  reinterpret_cast<void*>(&ResolveInitiative), reinterpret_cast<void**>(&g_resolve_initiative))) {
    return;
  }
  AnswerRulesetInt("IMPROVED_INITIATIVE_BONUS", "initiative: PB with Improved Initiative (Alert)", &AlertBonus);
  AnswerRulesetInt("EPIC_SUPERIOR_INITIATIVE_BONUS", "initiative: PB with Epic Superior Initiative (Alert)",
                   &AlertBonus);
  AnswerRulesetInt("BLOODED_INITIATIVE_BONUS", "initiative: no Blooded bonus (2024)", &NoBonus);
  AnswerRulesetInt("THUG_INITIATIVE_BONUS", "initiative: no Thug bonus (2024)", &NoBonus);
  AddDiceFilter(&InitiativeD20);
  AddFeatFilter(&AlertFromBackground);
}
