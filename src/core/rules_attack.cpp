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
// - The Fighter's Archery (+2 to ranged attack rolls) and Defense (+1 AC in
//   armour) with class_features faithful (rules_fighter.cpp).
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
#include "attack_roll.h"
#include "rules/nwn_conversion.h"

int RacialSlingBonusToRemove(void* stats, bool ranged);  // rules_racial.cpp

bool SpellAttackAgainstFullAc(void* target_stats, void* attacker);  // rules_spell_attack.cpp (SK10)

int FightingStyleAttackBonus(void* stats);  // rules_fighter.cpp (Fighter F1 Archery)
int DefenseArmorClassBonus(void* stats);    // rules_fighter.cpp (Fighter F1 Defense)

namespace {

using AttackModifierFn = int (*)(void* stats, void* target);
using ArmorClassFn = int16_t (*)(void* stats, void* attacker, int touch);

AttackModifierFn g_get_attack_modifier_versus = nullptr;
ArmorClassFn g_get_armor_class_versus = nullptr;

// Each creature's conversion is logged once, and again when its numbers
// change (another weapon, a buff, a level), so every combatant shows up.
LogOnce g_attack_logged(2000);
LogOnce g_armor_logged(2000);

bool CurrentAttackIsRanged(void* stats) {
  const uint8_t* data = CurrentAttackData(CreatureOf(stats));
  return data && Field<int32_t>(data, ee::kAttackRanged) != 0;
}

int GetAttackModifierVersus(void* stats, void* target) {
  const int style = FightingStyleAttackBonus(stats);  // Archery
  const int engine_modifier =
      g_get_attack_modifier_versus(stats, target) - RacialSlingBonusToRemove(stats, CurrentAttackIsRanged(stats));
  if (!g_config.attack_proficiency) return engine_modifier + style;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return engine_modifier + style;
  int base_attack_bonus = 0;
  int proficiency = 0;
  const int converted = ConvertEngineAttackBonus(stats, engine_modifier, facts, &base_attack_bonus, &proficiency);
  if (g_config.log_attacks && g_attack_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_modifier, converted))) {
    Log("attack: object 0x%08X %s level %d CR %.2f: 3e %+d (BAB %d) -> 5e %+d (PB %d)", ObjectIdOf(stats),
        KindOf(facts), facts.level, facts.challenge_rating, engine_modifier, base_attack_bonus, converted,
        proficiency);
  }
  return converted + style;
}

int16_t GetArmorClassVersus(void* stats, void* attacker, int touch) {
  if (touch && SpellAttackAgainstFullAc(stats, attacker)) touch = 0;  // SK10: 2024 has no touch AC
  const int16_t engine_armor_class = g_get_armor_class_versus(stats, attacker, touch);
  // Defense: +1 in armour (not to a 3e touch AC, which leaves armour out).
  const int16_t gear_armor_class =
      static_cast<int16_t>(engine_armor_class + (touch ? 0 : DefenseArmorClassBonus(stats)));
  if (!g_config.creature_ac_by_cr && g_config.debug_creature_ac <= 0) return gear_armor_class;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (facts.character) return gear_armor_class;  // players and henchmen keep their gear AC
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

// The engine's own attack modifier, without nwn5e's conversion (the hook's
// trampoline, or the export when it isn't hooked) — rules_advantage.cpp asks
// it which situational terms apply to an attack.
int EngineAttackModifier(void* stats, void* target) {
  static const auto unhooked = reinterpret_cast<AttackModifierFn>(EngineExport(ee::kGetAttackModifierVersus));
  const AttackModifierFn fn = g_get_attack_modifier_versus ? g_get_attack_modifier_versus : unhooked;
  return fn ? fn(stats, target) : 0;
}

void InstallAttackRules() {
  const bool fighter = g_config.class_features == rules::RuleVariant::kFaithful;  // Archery, Defense
  if (g_config.attack_proficiency || g_config.racial_combat_bonuses == rules::RuleVariant::kFaithful || fighter) {
    HookExport("CNWSCreatureStats::GetAttackModifierVersus", ee::kGetAttackModifierVersus,
               reinterpret_cast<void*>(&GetAttackModifierVersus),
               reinterpret_cast<void**>(&g_get_attack_modifier_versus));
  }
  if (g_config.creature_ac_by_cr || g_config.debug_creature_ac > 0 ||
      g_config.spell_attacks == rules::RuleVariant::kFaithful || fighter) {
    HookExport("CNWSCreatureStats::GetArmorClassVersus", ee::kGetArmorClassVersus,
               reinterpret_cast<void*>(&GetArmorClassVersus), reinterpret_cast<void**>(&g_get_armor_class_versus));
  }
}
