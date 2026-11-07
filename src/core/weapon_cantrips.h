#pragma once

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

// From InstallCantrips (faithful).
void InstallWeaponCantrips();
