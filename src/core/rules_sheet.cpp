// Character sheet: the attack bonuses the 2024 rules actually use (port of
// 1.69's rules_sheet.cpp; docs/EE-PORT.md E1). The sheet's main-hand,
// off-hand and ranged attack bonuses come from GetMeleeAttackBonus /
// GetRangedAttackBonus (called by UpdateCombatInformation; EE's script touch
// attacks call them too, as 1.69's did), not from the attack roll, so they
// get the same conversion as attack rolls (3e BAB swapped for the
// proficiency bonus). The attacks-per-round count is fixed by
// rules_extra_attack.cpp; the "base attack bonus" line stays 3e (EP9).

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"

int RacialSlingBonusToRemove(void* stats, bool ranged);  // rules_racial.cpp

namespace {

using MeleeAttackBonusFn = int (*)(void* stats, int offhand, int a, int b);
using RangedAttackBonusFn = int (*)(void* stats, int a, int b);

MeleeAttackBonusFn g_get_melee_attack_bonus = nullptr;
RangedAttackBonusFn g_get_ranged_attack_bonus = nullptr;

LogOnce g_sheet_logged(500);

int ConvertSheetAttackBonus(void* stats, int engine_value, const char* which) {
  const CreatureFacts facts = ReadCreatureFacts(stats);
  const int converted = ConvertEngineAttackBonus(stats, engine_value, facts, nullptr, nullptr);
  if (g_config.log_attacks && facts.character &&
      g_sheet_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_value, converted))) {
    Log("sheet: object 0x%08X %s %s attack bonus 3e %+d -> 5e %+d", ObjectIdOf(stats), KindOf(facts), which,
        engine_value, converted);
  }
  return converted;
}

int GetMeleeAttackBonus(void* stats, int offhand, int a, int b) {
  return ConvertSheetAttackBonus(stats, g_get_melee_attack_bonus(stats, offhand, a, b),
                                 offhand ? "off-hand" : "melee");
}

int GetRangedAttackBonus(void* stats, int a, int b) {
  // The sheet asks (0, 0), with a ranged weapon in hand: the path that adds
  // the halfling sling bonus.
  const int engine_value = g_get_ranged_attack_bonus(stats, a, b) - RacialSlingBonusToRemove(stats, a == 0 && b == 0);
  return g_config.attack_proficiency ? ConvertSheetAttackBonus(stats, engine_value, "ranged") : engine_value;
}

}  // namespace

void InstallSheetRules() {
  if (g_config.racial_combat_bonuses == rules::RuleVariant::kFaithful && !g_config.attack_proficiency) {
    HookExport("CNWSCreatureStats::GetRangedAttackBonus (sheet: sling)", ee::kGetRangedAttackBonus,
               reinterpret_cast<void*>(&GetRangedAttackBonus), reinterpret_cast<void**>(&g_get_ranged_attack_bonus));
    return;
  }
  if (!g_config.attack_proficiency) return;
  HookExport("CNWSCreatureStats::GetMeleeAttackBonus (sheet)", ee::kGetMeleeAttackBonus,
             reinterpret_cast<void*>(&GetMeleeAttackBonus), reinterpret_cast<void**>(&g_get_melee_attack_bonus));
  HookExport("CNWSCreatureStats::GetRangedAttackBonus (sheet)", ee::kGetRangedAttackBonus,
             reinterpret_cast<void*>(&GetRangedAttackBonus), reinterpret_cast<void**>(&g_get_ranged_attack_bonus));
}
