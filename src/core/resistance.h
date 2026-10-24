#pragma once

#include <stdint.h>

// X4 (docs/PLAN.md "Phase 3", shared): 2024 Resistance - damage of a type the
// creature resists is halved, rounded down, once (resistances don't stack).
// One hook on CNWSObject::DoDamageImmunity, which OnApplyDamage asks first for
// each damage type: after the engine's own % immunity, a resisted type's
// damage left is at most half the damage (50% immunity from an item already
// leaves about half - it doesn't stack; NWN's flat resistance and reduction
// still follow). For Rage now; Petrified, species and spells later.

// NWN damage-type bits (DAMAGE_TYPE_*).
constexpr uint32_t kDamageBludgeoning = 1;
constexpr uint32_t kDamagePiercing = 2;
constexpr uint32_t kDamageSlashing = 4;

// The damage-type bits `object` (a CNWSObject; for a creature, the creature)
// resists right now, or 0.
using ResistanceProvider = uint32_t (*)(void* object);

// Hooks DoDamageImmunity on first use; false (and logged) if it can't.
bool AddResistanceProvider(ResistanceProvider provider);
