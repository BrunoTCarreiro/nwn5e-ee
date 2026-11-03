#pragma once

#include <stdint.h>

#include "mastery.h"

// Phase 6 (#53): the extra actions (extra_actions.cpp) - Action Surge, Thief's Reflexes, Cunning Action, Steady Aim.

// rules_two_weapon.cpp's round start (after InitializeNumberOfAttacks): a pending Action Surge or Thief's Reflexes
// adds the Attack action's attacks to the round; Steady Aim's starting place.
void ExtraActionsRoundStart(void* round);
// rules_initiative.cpp: a fight's start - Thief's Reflexes' first round.
void ThiefsReflexesOnInitiative(void* creature);
// rules_advantage.cpp: Steady Aim's Advantage on the next attack (taken by it).
MasteryRollSources SteadyAimRollSources(void* attacker);
// bonus_actions.cpp (the default Bonus Action): ready now.
bool SteadyAimReady(void* creature);
bool HideReady(void* creature);
// combat_options.cpp: Thief's Reflexes' switch (on by default) and the Rogue level that has it.
bool ThiefsReflexesOn(void* creature);
void SetThiefsReflexes(void* creature, bool on);
