#pragma once

#include <stdint.h>

// Guards on the orders a creature takes (#106; the dying rules' and the Rage's
// hooks moved here, so several rules can refuse orders).
//
// CNWSObject::AddAction / AddActionToFront (action, group, then up to 12
// (type, pointer to value) parameter pairs): every action reaches a queue
// through these - the scripts' Action*, the AI's, a player's orders. An attack
// order is action 0xC with one parameter: type kActionParamObject, the target's
// id (CNWSCreature::AddAttackActions, read 89.8193.37). A guard returns true to
// refuse the action; `type1` / `param1` are its first parameter.
constexpr uint32_t kActionParamObject = 3;
using ActionGuard = bool (*)(void* object, uint32_t action, uint32_t type1, const void* param1);
// Hooks AddAction on first use; false (and logged) if it can't.
bool AddActionGuard(ActionGuard guard);

// CNWSCreature::AddCastSpellActions: a creature's order to cast a spell at
// `target` (an object id, or invalid for a location). A guard returns true to
// refuse it (a guard that refuses says why to the caster).
using SpellCastGuard = bool (*)(void* creature, uint32_t spell, uint32_t target);
// Hooks AddCastSpellActions on first use; false (and logged) if it can't.
bool AddSpellCastGuard(SpellCastGuard guard);
