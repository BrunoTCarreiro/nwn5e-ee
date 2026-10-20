#pragma once

// Conditions read from a creature's engine effects (rules_conditions.cpp), for
// the attack-roll (and later save-roll) hooks. The decisions are
// rules/conditions.h.

#include <stdint.h>

#include <string>

#include "rules/conditions.h"

// The creature's conditions (no frightened / concealed: those depend on the
// other creature; ConditionAttackRollSources adds them).
rules::Conditions ReadConditions(void* creature);

// A creature that can't act (2024 Incapacitated): dazed, stunned, paralysed,
// asleep or petrified — any it holds, not only NWN's highest state (B5).
bool IncapacitatedByCondition(void* creature);

// Advantage / disadvantage sources conditions add to one attack roll, and a
// note for the log ("" when none).
struct ConditionRollSources {
  int advantage = 0;
  int disadvantage = 0;
  std::string note;
};
ConditionRollSources ConditionAttackRollSources(void* attacker, void* target, bool ranged);

// CN1–CN3 on a saving throw on `ability`: stunned (both modes), paralysed and
// asleep (faithful) creatures fail Strength and Dexterity saves; a restrained
// one (faithful) has disadvantage on Dexterity saves.
struct ConditionSave {
  bool auto_fail = false;
  int disadvantage = 0;
  std::string note;
};
ConditionSave ConditionsForSave(void* creature, rules::Ability ability);

// SK11 (faithful): why a creature has disadvantage on ability checks
// (poisoned; frightened with its fear's source in sight) — "" when none.
std::string CheckDisadvantage(void* creature);
