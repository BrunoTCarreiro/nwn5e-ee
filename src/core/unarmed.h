#pragma once

#include <stdint.h>

#include <string>

// The 2024 Unarmed Strike options as special attacks (rules_shove.cpp; faithful,
// docs/PLAN.md SK6 and "Grapple and Throw"). Shove is NWN's Knockdown (feat 23,
// the engine's special attack). nwn5e's own options are feats past the game's
// rows (ids >= 1116): used on a creature they are queued as UseFeat queues
// Knockdown - AddAttackActions, then the id as a special attack on the combat
// round - so the character closes in and it is one of its attacks; the engine
// skips its uses check for such ids and resolves them as an ordinary attack,
// which rules_shove turns into no attack roll, no damage and the option's
// resolver (RE: UseFeat 0x3a91d0, ResolveMeleeSpecialAttack +0x6d).

// Called once when the option's attack resolves; `target` is the attack's
// target (any object - the resolver checks it is a creature).
using UnarmedOptionResolver = void (*)(void* attacker, void* target);

// From an Install (faithful): `feat` used on a creature becomes this option.
// False (and logged) when the Shove's hooks are off.
bool AddUnarmedOption(uint16_t feat, const char* name, UnarmedOptionResolver resolve);

// The target's save against an Unarmed Strike option: the better of its
// Strength and Dexterity saves (its choice) against 8 + the attacker's Strength
// modifier + PB, through the engine's save roll (so the combat log shows it).
struct UnarmedSave {
  bool rolled = false;  // false: no stats to roll with (nothing happens)
  bool failed = false;
  bool immune = false;
  int dc = 0;
  std::string text;     // "Strength save +2 vs DC 13 (8 + Str +3 + PB +2)"
};
UnarmedSave RollUnarmedSave(void* attacker, void* target);

// "nwn5e: <option>: <save text> <outcome>" to the players among the two, and the log.
void ReportUnarmed(void* attacker, void* target, const char* option, const UnarmedSave& save,
                   const std::string& outcome);

// A player's message (a refusal: too big, ...) to the attacker if it is a player.
void TellUnarmed(void* attacker, const std::string& line);
