#pragma once

// CNWSCreatureStats::ResetFeatRemainingUses — the engine restores feat uses
// after a rest (AIActionRest, ForceRest). Rules that keep uses of their own
// (the weapon mastery swap, the Fighter's Indomitable) listen here.
using RestListener = void (*)(void* stats);

// Hooks it on first use; false (and logged) if it can't.
bool AddRestListener(RestListener listener);
