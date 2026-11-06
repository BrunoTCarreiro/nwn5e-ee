#include "check.h"
#include "rules/cantrips.h"

#include <algorithm>

using namespace rules;

// #39 Part 1: Cantrip Upgrade - a die more at character levels 5, 11 and 17; doubled on a Critical Hit.
TEST(CantripUpgrade) {
  CHECK_EQ(CantripDice(1), 1);
  CHECK_EQ(CantripDice(4), 1);
  CHECK_EQ(CantripDice(5), 2);
  CHECK_EQ(CantripDice(10), 2);
  CHECK_EQ(CantripDice(11), 3);
  CHECK_EQ(CantripDice(16), 3);
  CHECK_EQ(CantripDice(17), 4);
  CHECK_EQ(CantripDice(20), 4);
  CHECK_EQ(CantripDice(30), 4);  // an epic level: still four
  CHECK_EQ(CantripDiceRolled(5, false), 2);
  CHECK_EQ(CantripDiceRolled(5, true), 4);
  CHECK_EQ(CantripDiceRolled(1, true), 2);
}

// SRD 5.2.1's dice, damage types and rolls.
TEST(CantripsAreSrd52) {
  const Cantrip* fire_bolt = CantripOf(1050);
  CHECK_EQ(fire_bolt != nullptr, true);
  CHECK_EQ(fire_bolt->die, 10);
  CHECK_EQ(fire_bolt->damage_type, kDamageTypeFire);
  CHECK_EQ(fire_bolt->roll == CantripRoll::kRangedSpellAttack, true);
  CHECK_EQ(CantripOf(144)->die, 8);                                       // Ray of Frost
  CHECK_EQ(CantripOf(144)->damage_type, kDamageTypeCold);
  CHECK_EQ(CantripOf(424)->roll == CantripRoll::kDexteritySave, true);    // Acid Splash: a Dexterity save
  CHECK_EQ(CantripOf(424)->die, 6);
  CHECK_EQ(CantripOf(1051)->roll == CantripRoll::kMeleeSpellAttack, true);  // Chill Touch
  CHECK_EQ(CantripOf(1051)->damage_type, kDamageTypeNecrotic);
  CHECK_EQ(CantripOf(1052)->damage_type, kDamageTypeLightning);           // Shocking Grasp
  CHECK_EQ(CantripOf(1053)->die, 12);                                     // Poison Spray
  CHECK_EQ(CantripOf(1053)->damage_type, kDamageTypePoison);
  CHECK_EQ(CantripOf(1054)->damage_type, kDamageTypeRadiant);             // Sacred Flame
  CHECK_EQ(CantripOf(1055)->die, 8);                                      // Starry Wisp
  CHECK_EQ(CantripOf(1056)->roll == CantripRoll::kWisdomSave, true);      // Vicious Mockery
  CHECK_EQ(CantripOf(1056)->damage_type, kDamageTypePsychic);
  CHECK_EQ(CantripOf(100)->die, 0);                                       // Light: no damage
  CHECK_EQ(CantripOf(107) == nullptr, true);                              // Magic Missile
  CHECK_EQ(CantripOf(37) == nullptr, true);                               // Daze: retired
  // 2024's types NWN lacks are custom types of their own (owner: "poison and acid are different things and force
  // and psychic aren't magical"): none is NWN's Acid or Magical.
  CHECK_EQ(kDamageTypePoison != kDamageTypeAcid && kDamageTypePoison == 8192u, true);
  CHECK_EQ(kDamageTypeForce == 16384u && kDamageTypePsychic == 32768u, true);
  const std::vector<Cantrip>& all = Cantrips();
  CHECK_EQ(all.size(), 10u);
  CHECK_EQ(std::is_sorted(all.begin(), all.end(), [](const Cantrip& a, const Cantrip& b) { return a.spell < b.spell; }),
           true);
}

// 4A: NWN's own cantrips retired in faithful.
TEST(CantripsRetired) {
  CHECK_EQ(IsRetiredCantrip(37), true);    // Daze
  CHECK_EQ(IsRetiredCantrip(416), true);   // Flare
  CHECK_EQ(IsRetiredCantrip(439), true);   // Electric Jolt
  CHECK_EQ(IsRetiredCantrip(189), true);   // Virtue
  CHECK_EQ(IsRetiredCantrip(33), true);    // Cure Minor Wounds
  CHECK_EQ(IsRetiredCantrip(431), true);   // Inflict Minor Wounds
  CHECK_EQ(IsRetiredCantrip(144), false);  // Ray of Frost stays
  CHECK_EQ(IsRetiredCantrip(100), false);  // Light stays
  CHECK_EQ(RetiredCantrips().size(), 6u);
}
