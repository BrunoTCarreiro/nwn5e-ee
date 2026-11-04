#pragma once

#include <string>

// CNWSCreatureStats::ResetFeatRemainingUses — the engine restores feat uses
// after a rest (AIActionRest, ForceRest). Rules that keep uses of their own
// (the weapon mastery swap, the Fighter's Indomitable) listen here.
using RestListener = void (*)(void* stats);

// Hooks it on first use; false (and logged) if it can't.
bool AddRestListener(RestListener listener);

// CNWSCreature::Rest(bAssociate, bEnemyLOS) - a rest begins (every rest but ForceRest): the listeners hear a
// creature that is now resting (activity bit kActivityResting; a refused rest isn't heard). The Long Rest's
// all-or-nothing (rules_rest.cpp) and the Prepare spells window (#36) listen here.
using RestStartListener = void (*)(void* creature);
// Hooks it on first use; false (and logged) if it can't.
bool AddRestStartListener(RestStartListener listener);

// X6: a Short Rest (rules_rest.cpp - the HUD button, !shortrest). Features
// that come back on one listen here; each returns words for the player's line
// (" Rage: a use back.", or "" for nothing).
using ShortRestListener = std::string (*)(void* creature);
void AddShortRestListener(ShortRestListener listener);
// rules_rest.cpp, at each creature's Short Rest: the listeners' words joined.
std::string AfterShortRest(void* creature);
