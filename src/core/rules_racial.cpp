// Decision R1 (PLAN.md): in faithful mode NWN's racial combat bonuses against
// specific enemies are removed — Battle Training (+1 to hit vs orcs,
// goblinoids, reptilians), Good Aim (+1 with thrown weapons), the halfling
// sling bonus (+1) and Defensive Training (+4 AC vs giants). 2024 species
// have no such bonuses. NWN mode (or off) keeps them. (Port of 1.69's
// rules_racial.cpp; docs/EE-PORT.md E1.)
//
// In EE three of them are ruleset values, answered 0: OFFENSIVE_TRAINING_
// MODIFIER, GOOD_AIM_MODIFIER, DEFENSIVE_TRAINING_MODIFIER. The sling bonus is
// hard-coded (GetAttackModifierVersus and GetRangedAttackBonus: a ranged
// attack by race 3 with base item 61 in the right hand), so the attack and
// sheet hooks take it back out (RacialSlingBonusToRemove).

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "ruleset.h"

namespace {

bool Faithful() { return g_config.racial_combat_bonuses == rules::RuleVariant::kFaithful; }

}  // namespace

// What the engine's hard-coded halfling sling bonus added to a ranged attack
// (or the sheet's ranged line) of the creature owning `stats`.
int RacialSlingBonusToRemove(void* stats, bool ranged) {
  if (!Faithful() || !ranged || !stats || engine::RaceOf(stats) != ee::kRaceHalfling) return 0;
  return engine::BaseItemRowInSlot(CreatureOf(stats), ee::kSlotRightHand) == ee::kBaseItemSling ? 1 : 0;
}

void InstallRacialRules() {
  if (!Faithful()) return;
  AnswerRulesetInt("OFFENSIVE_TRAINING_MODIFIER", "racial_combat_bonuses: no Battle Training", [](int) { return 0; });
  AnswerRulesetInt("GOOD_AIM_MODIFIER", "racial_combat_bonuses: no Good Aim", [](int) { return 0; });
  AnswerRulesetInt("DEFENSIVE_TRAINING_MODIFIER", "racial_combat_bonuses: no Defensive Training",
                   [](int) { return 0; });
}
