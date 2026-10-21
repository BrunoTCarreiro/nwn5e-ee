// Finesse (`finesse`). 2024 (SRD 5.2): with a Finesse weapon (dagger, dart,
// rapier, scimitar, short sword, whip; NWN's kama and kukri like them) you use
// the better of Strength and Dexterity for the attack and the damage — no feat
// needed, any size. NWN: Dexterity to attack only with the Weapon Finesse feat
// (which CM8 no longer offers in faithful), for weapons its size table allows.
//
// Faithful: the engine's GetWeaponFinesse (asked for an attack's ability and
// the sheet's attack line) answers by the 2024 property; unarmed attacks keep
// NWN's answer until the Monk's brief (Martial Arts). The damage bonus gets
// the better ability (rules::FinesseDamageAdjustment, through the
// GetDamageBonus hook in rules_two_weapon.cpp; the off hand keeps D1's rule).
// NWN mode: NWN's Weapon Finesse.
//
// Not changed: the sheet's damage line (GetMeleeDamageBonus) still shows
// Strength.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/weapons.h"

bool HasTwoWeaponFightingStyle(void* stats);  // rules_fighter.cpp

namespace {

using GetWeaponFinesseFn = int (*)(void* stats, void* item);
GetWeaponFinesseFn g_get_weapon_finesse = nullptr;
LogOnce g_logged(256);

bool Faithful() { return g_config.finesse == rules::RuleVariant::kFaithful; }

int GetWeaponFinesse(void* stats, void* item) {
  if (!item) return g_get_weapon_finesse(stats, item);  // unarmed: NWN's (the Monk's brief)
  return rules::IsFinesseNwnBaseItem(engine::BaseItemRow(item)) ? 1 : 0;
}

}  // namespace

// What GetDamageBonus adds for 2024 Finesse: the better of Str and Dex.
int FinesseDamageBonus(void* stats, int off_hand) {
  if (!Faithful() || !stats) return 0;
  void* creature = CreatureOf(stats);
  if (!creature) return 0;
  const int row = engine::BaseItemRowInSlot(creature, off_hand ? ee::kSlotLeftHand : ee::kSlotRightHand);
  if (row < 0 || !rules::IsFinesseNwnBaseItem(row)) return 0;
  const bool two_weapon_faithful = g_config.two_weapon_fighting == rules::RuleVariant::kFaithful;
  if (off_hand && !two_weapon_faithful) return 0;  // NWN's off-hand Strength: left as it is
  const bool keeps = engine::HasFeat(stats, ee::kFeatTwoWeaponFighting) || HasTwoWeaponFightingStyle(stats);
  const int strength = engine::AbilityModifier(stats, 0), dexterity = engine::AbilityModifier(stats, 1);
  const int adjustment = rules::FinesseDamageAdjustment(strength, dexterity, off_hand != 0, keeps);
  if (adjustment && g_config.log_attacks &&
      g_logged.First(LogOnce::Key(ObjectIdOf(stats), row, (off_hand ? 1000 : 0) + adjustment))) {
    Log("finesse: object 0x%08X %s damage %+d (Dexterity %+d instead of Strength %+d, base item %d)",
        ObjectIdOf(stats), off_hand ? "off-hand" : "main-hand", adjustment, dexterity, strength, row);
  }
  return adjustment;
}

void InstallFinesseRules() {
  if (!Faithful()) return;
  HookExport("CNWSCreatureStats::GetWeaponFinesse (finesse: the 2024 property, no feat)", ee::kGetWeaponFinesse,
             reinterpret_cast<void*>(&GetWeaponFinesse), reinterpret_cast<void**>(&g_get_weapon_finesse));
}
