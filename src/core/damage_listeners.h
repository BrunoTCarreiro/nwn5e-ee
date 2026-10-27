#pragma once

#include <stdint.h>

// CNWSEffectListHandler::OnApplyDamage - a damage effect landing on an object
// (0.47.1: shared by the weapon mastery's Vex / Slow / Push and BZ3
// Retaliation). After the engine applied it, each listener hears who made the
// effect, the creature it landed on, and the hit points (and temporary ones)
// it took - 0 when resisted or absorbed. Only creatures; the engine's thread.
using DamageListener = void (*)(uint32_t creator, void* target, int damage);

// Hooks OnApplyDamage on first use; false (and logged) if it can't.
bool AddDamageListener(DamageListener listener);
