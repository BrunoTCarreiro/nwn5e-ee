#include "check.h"
#include "rules/hazard_avoid.h"

using namespace rules;

// #131 (owner, 2026-11-04: "most living enemies would avoid hazards, while undead and enemies that are charmed or
// somehow dominated would not"; 2 B; 3 B). Breaks caught: a grenade hazard or a spell area not known, Holy Water's
// patch taken for anyone's, a harmless area (Darkness) taken for a hazard, a case-blind name missed, the step-out
// short of the radius; undead, constructs, the charmed or a player's character avoiding, NWN mode avoiding; a ranged
// attack held, an attacker already inside held, one it wouldn't hurt held.
TEST(HazardAvoidance) {
  CHECK_EQ(HazardOfScript("n5_fpacid_in") == HazardKind::kAnyone, true);
  CHECK_EQ(HazardOfScript("N5_FPHOLY_RD") == HazardKind::kHolyWater, true);
  CHECK_EQ(HazardOfScript("n5_bacid_out") == HazardKind::kAnyone, true);
  CHECK_EQ(HazardOfScript("x0_s3_calEN") == HazardKind::kAnyone, true);
  CHECK_EQ(HazardOfScript("x0_s3_chokeHB") == HazardKind::kAnyone, true);
  CHECK_EQ(HazardOfScript("NW_S0_AcidFogC") == HazardKind::kSpell, true);
  CHECK_EQ(HazardOfScript("NW_S0_WebA") == HazardKind::kSpell, true);
  CHECK_EQ(HazardOfScript("nw_s0_darknessa") == HazardKind::kNone, true);
  CHECK_EQ(HazardOfScript("") == HazardKind::kNone, true);
  CHECK_EQ(HazardStepOutMetres("n5_fpfire_in") > 2.3f && HazardStepOutMetres("n5_fpfire_in") < 2.4f, true);
  CHECK_EQ(HazardStepOutMetres("NW_S0_StinkCldA") > 8.1f && HazardStepOutMetres("NW_S0_StinkCldA") < 8.3f, true);
  CHECK_EQ(HazardStepOutMetres("nw_s0_darknessa") == 0.0f, true);
  const int humanoid = 6;  // racialtypes.2da: an orc
  CHECK_EQ(AvoidsHazards(true, false, humanoid, false), true);
  CHECK_EQ(AvoidsHazards(true, false, kRacialTypeUndead, false), false);
  CHECK_EQ(AvoidsHazards(true, false, kRacialTypeConstruct, false), false);
  CHECK_EQ(AvoidsHazards(true, false, humanoid, true), false);  // charmed, dominated, confused
  CHECK_EQ(AvoidsHazards(true, true, humanoid, false), false);  // a player's character
  CHECK_EQ(AvoidsHazards(false, false, humanoid, false), false);
  CHECK_EQ(HazardHoldsAttack(true, true, true, false, false), true);
  CHECK_EQ(HazardHoldsAttack(true, true, true, false, true), false);   // ranged
  CHECK_EQ(HazardHoldsAttack(true, true, true, true, false), false);   // already in it
  CHECK_EQ(HazardHoldsAttack(true, false, true, false, false), false); // it wouldn't hurt the attacker
  CHECK_EQ(HazardHoldsAttack(false, true, true, false, false), false); // an undead attacker ...
  CHECK_EQ(HazardHoldsAttack(true, true, false, false, false), false); // the target not in one
}
