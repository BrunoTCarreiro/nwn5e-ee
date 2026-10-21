// Decision D1 (PLAN.md): two-weapon fighting, and E1: two-handed Strength
// (port of 1.69's rules_two_weapon.cpp; docs/EE-PORT.md E1).
//
// D1 faithful: no attack penalty on either hand; one off-hand attack per
// round, only with two Light weapons, a double weapon, or Improved
// Two-Weapon Fighting as Dual Wielder; the off-hand attack adds no Strength to
// damage unless it is negative, or with Two-Weapon Fighting (the 2024
// fighting style); Ambidexterity does nothing. NWN mode keeps NWN's rules.
// In EE the whole attack penalty is five ruleset values (the -6 / -10 and the
// Light, Ambidexterity and Two-Weapon Fighting offsets), answered 0; the
// off-hand Strength is OFFHAND_WEAPON_STRENGTH_MODIFIER (the engine applies
// a negative modifier in full and multiplies a positive one by it): 0, or 1
// with the Two-Weapon Fighting feat — the creature asking is known from the
// GetDamageBonus hook. The off-hand attack count is computed inside
// InitializeNumberOfAttacks (CalculateOffHandAttacks is inlined), so it is
// corrected after it.
//
// E1 faithful: a two-handed weapon adds Strength once, not NWN's 1.5 x:
// TWO_HANDED_WEAPON_STRENGTH_MODIFIER 1.5 -> 1.0 (attack damage and the
// sheet's damage line alike).

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "mastery.h"
#include "rules/weapons.h"
#include "ruleset.h"

bool HasTwoWeaponFightingStyle(void* stats);  // rules_fighter.cpp (Fighter F1, the 2024 style feat)
int FinesseDamageBonus(void* stats, int off_hand);  // rules_finesse.cpp

namespace {

using InitializeNumberOfAttacksFn = void (*)(void* round);
using GetDamageBonusFn = int (*)(void* stats, void* target, int off_hand);

InitializeNumberOfAttacksFn g_initialize_number_of_attacks = nullptr;
GetDamageBonusFn g_get_damage_bonus = nullptr;
thread_local void* t_damage_stats = nullptr;  // the creature whose damage bonus is being computed
thread_local float t_two_handed_multiplier = 0.0f;  // the two-handed Strength multiplier that bonus used (0: none)

LogOnce g_logged_attacks(512);
LogOnce g_logged_style(512);

bool TwoWeaponFaithful() { return g_config.two_weapon_fighting == rules::RuleVariant::kFaithful; }

void InitializeNumberOfAttacks(void* round) {
  g_initialize_number_of_attacks(round);
  const int nwn_attacks = Field<int32_t>(round, ee::kRoundOffHandAttacks);
  if (nwn_attacks <= 0) return;
  void* creature = Field<void*>(round, ee::kRoundOwner);
  void* stats = StatsOf(creature);
  if (!stats) return;
  const int main_row = engine::BaseItemRowInSlot(creature, ee::kSlotRightHand);
  const int off_row = engine::BaseItemRowInSlot(creature, ee::kSlotLeftHand);
  const bool double_weapon = engine::IsDoubleWeapon(main_row);
  const bool dual_wielder = engine::HasFeat(stats, ee::kFeatImprovedTwoWeaponFighting);
  const int attacks = rules::OffHandAttacks(nwn_attacks, double_weapon, dual_wielder,
                                            rules::IsLightNwnBaseItem(main_row), rules::IsLightNwnBaseItem(off_row));
  SetField<int32_t>(round, ee::kRoundOffHandAttacks, attacks);
  const uint32_t id = ObjectIdOf(stats);
  if (g_config.log_attacks &&
      g_logged_attacks.First(LogOnce::Key(id, (nwn_attacks << 8) | attacks, ((main_row & 0xFF) << 8) | (off_row & 0xFF)))) {
    Log("two-weapon: object 0x%08X off-hand attacks 3e %d -> 5e %d (base items %d / %d%s%s)", id, nwn_attacks, attacks,
        main_row, off_row, double_weapon ? ", double weapon" : "", dual_wielder ? ", Dual Wielder" : "");
  }
}

int GetDamageBonus(void* stats, void* target, int off_hand) {
  void* outer = t_damage_stats;
  t_damage_stats = stats;
  t_two_handed_multiplier = 0.0f;
  int bonus = g_get_damage_bonus(stats, target, off_hand);
  t_damage_stats = outer;
  bonus += FinesseDamageBonus(stats, off_hand);  // 2024 Finesse: the better of Str and Dex
  if (!off_hand) bonus = MasteryCleaveDamage(stats, bonus, t_two_handed_multiplier);  // D4 Cleave's extra attack
  return bonus;
}

// The engine asks this only for a two-handed attack; E1 faithful answers 1.0.
float TwoHandedStrength(float engine_value) {
  const float value = g_config.two_handed_strength == rules::RuleVariant::kFaithful ? 1.0f : engine_value;
  t_two_handed_multiplier = value;
  return value;
}

float OffHandStrength(float) {
  void* stats = t_damage_stats;
  const bool style =
      stats && (engine::HasFeat(stats, ee::kFeatTwoWeaponFighting) || HasTwoWeaponFightingStyle(stats));
  if (style && g_config.log_attacks && g_logged_style.First(LogOnce::Key(ObjectIdOf(stats), 1, 1))) {
    Log("two-weapon: object 0x%08X off-hand damage keeps Strength (Two-Weapon Fighting style)", ObjectIdOf(stats));
  }
  return style ? 1.0f : 0.0f;
}

}  // namespace

void InstallTwoWeaponRules() {
  if (TwoWeaponFaithful()) {
    for (const char* label : {"ONHAND_NORMAL_OFFHAND_ATTACK_PENALTY", "OFFHAND_NORMAL_OFFHAND_ATTACK_PENALTY",
                              "LIGHT_OFFHAND_WEAPON_BONUS", "AMBIDEXTERITY_BONUS", "TWO_WEAPON_FIGHTING_BONUS"}) {
      AnswerRulesetInt(label, "two_weapon_fighting: no two-weapon attack penalties", [](int) { return 0; });
    }
    AnswerRulesetFloat("OFFHAND_WEAPON_STRENGTH_MODIFIER",
                       "two_weapon_fighting: off-hand damage without Strength unless negative or with the style",
                       &OffHandStrength);
    HookExport("CNWSCombatRound::InitializeNumberOfAttacks (off-hand attacks)", ee::kInitializeNumberOfAttacks,
               reinterpret_cast<void*>(&InitializeNumberOfAttacks),
               reinterpret_cast<void**>(&g_initialize_number_of_attacks));
  }
  const bool mastery = g_config.weapon_mastery != rules::RuleVariant::kOff;  // D4 Cleave's damage
  const bool finesse = g_config.finesse == rules::RuleVariant::kFaithful;
  if (TwoWeaponFaithful() || mastery || finesse) {
    HookExport("CNWSCreatureStats::GetDamageBonus (who deals damage; Cleave; Finesse)", ee::kGetDamageBonus,
               reinterpret_cast<void*>(&GetDamageBonus), reinterpret_cast<void**>(&g_get_damage_bonus));
  }
  if (g_config.two_handed_strength == rules::RuleVariant::kFaithful || mastery) {
    AnswerRulesetFloat("TWO_HANDED_WEAPON_STRENGTH_MODIFIER", "two_handed_strength: Strength once",
                       &TwoHandedStrength);
  }
}
