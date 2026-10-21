#pragma once

// Weapon size and the Heavy property (rules_weapon_size.cpp, `size`).

#include <string>

// The 2024 Heavy property, faithful: 1 (a Disadvantage source) when the
// attacker's current weapon is Heavy and its Strength (melee) or Dexterity
// (ranged) is below 13, else 0; `note` gets the reason for the log.
int HeavyWeaponDisadvantage(void* attacker, bool ranged, std::string* note);

// Faithful: the size a creature wields weapons at — a Small creature as
// Medium (2024 has no size rule for weapons: a halfling holds a longsword in
// one hand, a greatsword in two, as a human does); else its own size.
int WeaponWielderSize(void* creature);

// While alive, the engine's own weapon-size arithmetic (CanEquipWeapon, the
// round's off-hand attacks) sees a Small creature as Medium (faithful).
class WeaponSizeAsMedium {
 public:
  explicit WeaponSizeAsMedium(void* creature);
  ~WeaponSizeAsMedium();
  WeaponSizeAsMedium(const WeaponSizeAsMedium&) = delete;
  WeaponSizeAsMedium& operator=(const WeaponSizeAsMedium&) = delete;

 private:
  void* creature_ = nullptr;
};
