#pragma once

#include <stdint.h>

// Phase 6 part 1 (#52): the 2024 Bonus Action (bonus_actions.cpp), `actions` faithful.

// rules_two_weapon.cpp, after InitializeNumberOfAttacks set the round's off-hand attacks and before the Monk's
// strike: the round's automatic bonus attacks - the off-hand attack dropped when the Bonus Action is taken or kept
// free (unless Nick makes it part of the Attack action).
void BonusActionRoundStart(void* round);
// rules_monk.cpp: whether the Martial Arts strike / Flurry of Blows is this round's Bonus Action.
bool MartialArtsBonusAllowed(void* creature);
// The Combat configuration window (combat_options.cpp): the default Bonus Action - the automatic bonus attack (on)
// or none (off).
bool ActionEconomyOn();
bool DefaultBonusAttackOn(void* creature);
void SetDefaultBonusAttack(void* creature, bool on);
