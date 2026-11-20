#pragma once

namespace rules {

// SRD 5.2.1, Breaking Objects: "Objects can be harmed by attacks and by some spells ... Armor Class. The Object Armor
// Class table suggests ACs for various substances." An object's AC by its substance, as NWN names it in
// placeableobjsnds.2da's ArmorType (placeables.2da's SoundAppType points there): leather (the cloth rows) 11, wood
// 15, stone 17, plate (the metal rows) 19; anything else or none, wood's 15. (The table's 13 for crystal, glass, ice
// and 21 / 23 for mithral and adamantine have no ArmorType of their own.)
int ObjectArmorClass(const char* armor_type);

// #171 (owner, 2026-11-20: "scorching rays are still attacks you should still be able to attack placeables"): an
// attack roll against an object - `d20` the die, `bonus` the attacker's (a spell attack's: PB + its casting
// modifier). 0 a miss, 1 a hit, 2 a Critical Hit: a natural 20 always hits and is a Critical Hit, a natural 1 always
// misses, else a hit when the total meets the AC.
int AttackAgainstObject(int d20, int bonus, int armor_class);

}  // namespace rules
