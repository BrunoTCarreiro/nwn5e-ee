#pragma once

#include <stdint.h>

// CNWSCreature::ResolveDamage(attacker, target) - an attack's damage rolled into
// the attacker's current attack data (int16 per damage type at +0x16), before
// ResolvePost{Melee,Ranged}Damage turns it into the damage effect (0.50.0:
// shared by the Shove, which skips it, and the Rogue's Uncanny Dodge).

// True skips the engine's step (`caller` = its call site's return address).
using ResolveDamageSkip = bool (*)(void* attacker, void* target, uintptr_t caller);
// After the engine's step: the damage is in CurrentAttackData(attacker).
using ResolveDamageListener = void (*)(void* attacker, void* target);

// Hook ResolveDamage on first use; false (and logged) if it can't.
bool AddResolveDamageSkip(ResolveDamageSkip skip);
bool AddResolveDamageListener(ResolveDamageListener listener);
