#pragma once

#include <stdint.h>

#include "mastery.h"

// Phase 6 (#53): the extra actions (extra_actions.cpp) - Action Surge, Thief's Reflexes, Cunning Action, Steady Aim.

// rules_two_weapon.cpp's round start (after InitializeNumberOfAttacks): a pending Action Surge or Thief's Reflexes
// adds the Attack action's attacks to the round; Steady Aim's starting place.
void ExtraActionsRoundStart(void* round);
// #129 A: a Cunning Action Hide's attack stop on its way - the round's swings held until it lands (round_swings.cpp).
// Any rule's StopOwnAttacks too (Sanctuary on yourself).
bool HideHoldsAttacks(void* creature);
// #129 A's attack stop for any rule that stops a player's attacks (the Hide; Sanctuary on yourself - #39 build 2a,
// owner, 2026-11-11: "go with A for sanctuary"): the default attack off now (until the next attack order),
// the attack order cancelled a moment later as the action queue's click cancels it (its round's swings with it),
// held until then. False when it wasn't attacking (the default attack is off either way), or the extra actions
// aren't installed (class_features faithful).
struct AttackStop {
  const char* name;      // "Cunning Action: Hide" - the log's
  const char* why;       // the default attack's "off (...)"
  const char* log_tail;  // after "its attack stopped (n of m attack order(s))"
  const char* tell;      // to the player once it stopped
};
bool StopOwnAttacks(void* creature, const AttackStop& stop);
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
