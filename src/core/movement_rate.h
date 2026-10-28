#pragma once

// CNWSCreature::GetMovementRateFactor - the factor a creature's walking and
// running speeds are multiplied by (0.49.0: shared by Fast Movement and the
// grapple's drag, as damage_listeners.cpp shares OnApplyDamage).

// The factor the engine computed, adjusted (adjusters run in the order added).
using MovementRateAdjuster = float (*)(void* creature, float factor);

// Hooks GetMovementRateFactor on first use; false (and logged) if it can't.
bool HookMovementRate();
bool AddMovementRateAdjuster(MovementRateAdjuster adjuster);
// A speed bonus (0.52.0, the Monk's Unarmored Movement): bonuses run before
// the adjusters, so a halving (the grapple's drag) halves them too.
bool AddMovementRateBonus(MovementRateAdjuster bonus);

// The creature whose movement rate the engine is computing right now (for a
// ruleset value read inside it, as Fast Movement's), or null.
void* MovingCreature();
