// 2024 attack numbers on top of the engine's 3e ones (port of 1.69's
// rules_attack.cpp; docs/EE-PORT.md E1):
//
// - Attack rolls (attack_proficiency): the engine's attack modifier minus the
//   3e base attack bonus plus the proficiency bonus (by level for player
//   characters and henchmen, by challenge rating for every other creature).
//   The ability modifier, weapon enhancement, effects and feats the engine
//   adds are kept. EE's 3e iterative penalty is a ruleset value, removed by
//   rules_extra_attack.cpp.
// - Armor Class (creature_ac_by_cr): player characters and henchmen keep the
//   engine's (gear) AC; every other creature — monsters, summons, familiars,
//   animal companions — uses the expected AC for its challenge rating
//   (decision 11). [debug] creature_ac gives them all one AC instead.
//
// If the challenge rating read from memory is not plausible, the engine's
// numbers are returned unchanged and the read is logged.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/nwn_conversion.h"

namespace {

using AttackModifierFn = int (*)(void* stats, void* target);
using ArmorClassFn = int16_t (*)(void* stats, void* attacker, int touch);

AttackModifierFn g_get_attack_modifier_versus = nullptr;
ArmorClassFn g_get_armor_class_versus = nullptr;

// Each creature's conversion is logged once, and again when its numbers
// change (another weapon, a buff, a level), so every combatant shows up.
LogOnce g_attack_logged(2000);
LogOnce g_armor_logged(2000);

int GetAttackModifierVersus(void* stats, void* target) {
  const int engine_modifier = g_get_attack_modifier_versus(stats, target);
  if (!g_config.attack_proficiency) return engine_modifier;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return engine_modifier;
  int base_attack_bonus = 0;
  int proficiency = 0;
  const int converted = ConvertEngineAttackBonus(stats, engine_modifier, facts, &base_attack_bonus, &proficiency);
  if (g_config.log_attacks && g_attack_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_modifier, converted))) {
    Log("attack: object 0x%08X %s level %d CR %.2f: 3e %+d (BAB %d) -> 5e %+d (PB %d)", ObjectIdOf(stats),
        KindOf(facts), facts.level, facts.challenge_rating, engine_modifier, base_attack_bonus, converted,
        proficiency);
  }
  return converted;
}

int16_t GetArmorClassVersus(void* stats, void* attacker, int touch) {
  const int16_t engine_armor_class = g_get_armor_class_versus(stats, attacker, touch);
  if (!g_config.creature_ac_by_cr && g_config.debug_creature_ac <= 0) return engine_armor_class;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (facts.character) return engine_armor_class;  // players and henchmen keep their gear AC
  if (g_config.debug_creature_ac > 0) {
    // Test switch: attacks against creatures miss (except natural 20s), so a
    // round's attacks are all rolled instead of ending with the first kill.
    if (g_config.log_attacks &&
        g_armor_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_armor_class, g_config.debug_creature_ac))) {
      Log("AC: object 0x%08X creature: 3e %d -> debug %d", ObjectIdOf(stats), engine_armor_class,
          g_config.debug_creature_ac);
    }
    return static_cast<int16_t>(g_config.debug_creature_ac);
  }
  if (!facts.plausible) return engine_armor_class;
  const int converted = rules::ConvertArmorClass(false, engine_armor_class, facts.challenge_rating);
  if (g_config.log_attacks && g_armor_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_armor_class, converted))) {
    Log("AC: object 0x%08X creature CR %.2f%s: 3e %d -> 5e %d", ObjectIdOf(stats), facts.challenge_rating,
        touch ? " (touch)" : "", engine_armor_class, converted);
  }
  return static_cast<int16_t>(converted);
}

}  // namespace

void InstallAttackRules() {
  if (g_config.attack_proficiency) {
    HookExport("CNWSCreatureStats::GetAttackModifierVersus", ee::kGetAttackModifierVersus,
               reinterpret_cast<void*>(&GetAttackModifierVersus),
               reinterpret_cast<void**>(&g_get_attack_modifier_versus));
  }
  if (g_config.creature_ac_by_cr || g_config.debug_creature_ac > 0) {
    HookExport("CNWSCreatureStats::GetArmorClassVersus", ee::kGetArmorClassVersus,
               reinterpret_cast<void*>(&GetArmorClassVersus), reinterpret_cast<void**>(&g_get_armor_class_versus));
  }
}
