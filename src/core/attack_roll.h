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

// A rule that resolves an attack itself (SK6: a Shove makes no attack roll)
// returns true from a skipper, after writing the attack data's result: the
// engine's roll is not called and no listener runs. `caller` = the engine call
// site's return address.
using AttackRollSkipper = bool (*)(AttackRoll& roll, uintptr_t caller);
bool AddAttackRollSkipper(AttackRollSkipper skipper);

// The attack being resolved on this thread, or nullptr.
AttackRoll* CurrentAttackRoll();

// The CNWSCombatAttackData of the attack `creature`'s round is on (the one
// ResolveAttackRoll resolves next or now), or nullptr.
uint8_t* CurrentAttackData(void* creature);

// ResolveAttackRoll's code, for telling its calls apart (empty if not listed).
CodeRange ResolveAttackRollCode();
