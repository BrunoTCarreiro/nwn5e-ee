#pragma once

#include <stdint.h>

// #39 Part 1, build 3b: Shillelagh and True Strike on the game's own weapon attacks (rules_weapon_cantrips.cpp).
// Faithful `spellcasting` only; outside them each answers 0 / false.

// The attack, damage and round hooks below are wanted (`spellcasting` faithful): their files install them.
bool WeaponCantripsOn();

// GetAttackModifierVersus: how much the current attack's ability term changes - Shillelagh's weapon in melee (the
// better of the spellcasting ability and Strength), True Strike's attack (the spellcasting ability's in place of
// Strength or Dexterity).
int WeaponCantripAttackBonus(void* stats);

// GetDamageBonus (after 2024 Finesse's swap): the same for the damage roll. `strength_multiplier`: NWN's two-handed
// Strength multiplier the engine's bonus used (0: none).
int WeaponCantripDamageBonus(void* stats, int off_hand, float strength_multiplier);

// GetDamageRoll: Shillelagh's die in place of the weapon's - `count` / `sides` in: the dice the hit would roll (the
// weapon's, or Martial Arts'), out: the better of them and Shillelagh's. False: not a Shillelagh hit.
bool WeaponCantripDice(void* stats, int off_hand, int* count, int* sides);

// InitializeNumberOfAttacks, before the round's Bonus Action and extra attacks: true when the round is a True
// Strike's - cut to its one attack (no off-hand attack; the caller adds no Martial Arts strike).
bool TrueStrikeRoundStart(void* round);

// #39 Part 2, level 2, build 1b (owner, 2026-11-18, Flame Blade "A an equipped blade, your attacks with it the spell's
// (one a round, the Magic action)"): Flame Blade's blade, an item in `caster`'s main hand (rules_spells2024.cpp makes
// it) - while it's held, the creature's round is that one attack (the Magic action; an Action Surge's still come), a
// melee spell attack: its attack bonus the Proficiency Bonus + `modifier` (the casting class's), its damage `dice`d6
// + `modifier`, Fire (a Critical Hit: the dice twice). `blade` 0: no blade now (let go). Clear: the spell over.
void SetFlameBlade(uint32_t caster, uint32_t blade, int dice, int modifier);
void ClearFlameBlade(uint32_t caster);
void ClearFlameBlades();  // a game's start
// GetAttackModifierVersus (rules_attack.cpp): the blade's attack - true and its whole bonus (PB + the modifier: no
// weapon proficiency, Strength or Dexterity in it).
bool FlameBladeAttackBonus(void* stats, int* bonus);

// From InstallCantrips (faithful).
void InstallWeaponCantrips();
