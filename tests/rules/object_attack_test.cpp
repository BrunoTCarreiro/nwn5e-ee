#include "check.h"
#include "rules/object_attack.h"

using namespace rules;

// SRD 5.2.1, Breaking Objects: "Armor Class. The Object Armor Class table suggests ACs for various substances" -
// 11 Cloth, paper, rope; 15 Wood; 17 Stone; 19 Iron, steel. NWN's placeableobjsnds.2da ArmorType names them.
TEST(ObjectArmorClassBySubstance) {
  CHECK_EQ(ObjectArmorClass("leather"), 11);  // cloth_object_small / large
  CHECK_EQ(ObjectArmorClass("wood"), 15);
  CHECK_EQ(ObjectArmorClass("stone"), 17);
  CHECK_EQ(ObjectArmorClass("plate"), 19);    // the metal rows
  CHECK_EQ(ObjectArmorClass("STONE"), 17);     // the 2da's case doesn't matter
  CHECK_EQ(ObjectArmorClass("stonework"), 15);  // a longer name isn't stone
  CHECK_EQ(ObjectArmorClass(""), 15);         // none given: wood, the commonest
  CHECK_EQ(ObjectArmorClass(nullptr), 15);
  CHECK_EQ(ObjectArmorClass("chain"), 15);
}

// #171 (owner, 2026-11-20: "scorching rays are still attacks you should still be able to attack placeables"): a ray's
// attack roll against an object - 0 a miss, 1 a hit, 2 a Critical Hit; a natural 20 always hits and is a Critical Hit,
// a natural 1 always misses.
TEST(AttackAgainstObject) {
  CHECK_EQ(AttackAgainstObject(10, 5, 15), 1);   // 15 meets the AC
  CHECK_EQ(AttackAgainstObject(9, 5, 15), 0);    // 14 short of it
  CHECK_EQ(AttackAgainstObject(20, 0, 23), 2);   // a natural 20
  CHECK_EQ(AttackAgainstObject(1, 30, 11), 0);   // a natural 1
  CHECK_EQ(AttackAgainstObject(19, 5, 11), 1);   // 19 isn't a Critical Hit
}
