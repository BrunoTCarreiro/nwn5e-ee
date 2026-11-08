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

// The whole order, for guards that need its class, domain level or metamagic (#36: no more casts queued than the
// slots left). class_slot: the caster's class slot 0-7; 0xFF none (a script's cheat cast), 0xFE a spell-like
// ability. spontaneous: NWN's spontaneous conversion (a Cleric's cures); fake: a cast that only plays.
struct SpellOrder {
  uint32_t spell = 0;
  int class_slot = 0xFF;
  int domain_level = 0;
  int metamagic = 0;
  bool spontaneous = false;
  uint32_t target = 0;
  bool fake = false;
  float x = 0.0f, y = 0.0f, z = 0.0f;  // #39 build 4b: the point it's cast at (Dancing Lights' Move)
};
using SpellOrderGuard = bool (*)(void* creature, const SpellOrder& order);
// The same hook as AddSpellCastGuard (asked after those guards); false (and logged) if it can't.
bool AddSpellOrderGuard(SpellOrderGuard guard);
