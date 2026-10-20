#pragma once

// Weapon mastery (D3 who, D4 what), rules_mastery.cpp.

#include <stdint.h>

#include <string>

// Does `creature` have mastery with weapons of `base_item`? Characters only.
bool HasWeaponMasteryWith(void* creature, int base_item);

// D4 Vex / Sap sources for the attack about to be rolled; takes (uses up) the
// marks that apply. Asked by the advantage rule.
struct MasteryRollSources {
  int advantage = 0;
  int disadvantage = 0;
  std::string note;  // "Vex advantage", "Sap disadvantage" for the log
};
MasteryRollSources TakeMasteryRollSources(void* attacker, uint32_t target_id);

// The creature's ended combat rounds (while weapon mastery is on).
uint32_t CombatRoundsEnded(uint32_t id);

// D4 Cleave: the extra attack Cleave adds deals its damage without the
// ability modifier unless it is negative. Called from the damage bonus hook
// for main-hand attacks with the engine's bonus and the two-handed Strength
// multiplier it used (0 when the attack was not two-handed); returns `bonus`
// unchanged for every other attack.
int MasteryCleaveDamage(void* stats, int bonus, float two_handed_multiplier);

// D4 Push (rules_push.cpp; faithful only). A hit with a Push weapon the
// attacker has mastery with: the line is planned and the attacker may turn to
// a closer enemy; returns what was done for the mastery log line (empty when
// Push is off).
std::string PushOnHit(void* attacker, void* target);
// A Push hit waits for its damage to land (OnApplyDamage).
bool PushAwaiting();
// A damage effect created by `attacker_id` landed on `target`: the push, if it waited for it.
void PushOnDamage(uint32_t attacker_id, void* target);
// The creature's combat round ended (after the engine's EndCombatRound).
void PushOnRoundEnd(void* owner);
// From InstallMasteryRules (faithful); false and logged when the engine lacks a piece.
bool InstallPushRules();
