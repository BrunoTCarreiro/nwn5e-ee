#pragma once

#include <stdint.h>

#include "hooks.h"

// CNWSCreature::ResolveAttackRoll(target) — one attack's d20, hit, threat and
// confirmation (EE: 5 callers — melee, ranged, special attacks). Rules hook in
// before and after it; while it runs, CurrentAttackRoll() says which attack
// this thread is resolving, so hooks on the functions it calls (RollDice,
// GetCriticalHitRoll, ...) know they are inside an attack roll.
struct AttackRoll {
  void* attacker = nullptr;  // CNWSCreature
  void* target = nullptr;    // CNWSObject
  uint8_t* data = nullptr;   // the attack's CNWSCombatAttackData (nullptr if unknown)
};

using AttackRollListener = void (*)(AttackRoll& roll);

// Hooks ResolveAttackRoll on first use; either listener may be nullptr.
// False (and logged) if the hook can't be made.
bool AddAttackRollListeners(AttackRollListener before, AttackRollListener after);

// The attack being resolved on this thread, or nullptr.
AttackRoll* CurrentAttackRoll();

// ResolveAttackRoll's code, for telling its calls apart (empty if not listed).
CodeRange ResolveAttackRollCode();
