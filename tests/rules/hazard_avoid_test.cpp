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

// #39 build 2c (owner, 2026-11-12: "Hazard avoidance, sentient creatures with ranged attacks should still try to
// avoid these, but not as much melee only sentient creatures."): the 2c areas.
TEST(TerrainAreaAvoidance) {
  CHECK_EQ(HazardOfScript("n5_s_entanglea") == HazardKind::kDifficultTerrain, true);
  for (const char* name : {"n5_s_greasea", "n5_s_greaseb", "N5_S_GreaseC"}) {
    CHECK_EQ(HazardOfScript(name) == HazardKind::kGrease, true);
  }
  for (const char* name : {"n5_s_entangleb", "n5_s_entanglec"}) {
    CHECK_EQ(HazardOfScript(name) == HazardKind::kDifficultTerrain, true);
  }
  CHECK_EQ(HazardOfScript("n5_s_fog1a") == HazardKind::kFog, true);
  CHECK_EQ(HazardOfScript("n5_s_fog9b") == HazardKind::kFog, true);
  CHECK_EQ(HazardOfScript("nw_s0_entanglea") == HazardKind::kSpell, true);  // NWN's own stays a harmful area
  CHECK_EQ(HazardStepOutMetres("n5_s_entanglea") > 5.7f && HazardStepOutMetres("n5_s_entanglea") < 5.9f, true);
  CHECK_EQ(HazardStepOutMetres("n5_s_fog2b") > 13.6f && HazardStepOutMetres("n5_s_fog2b") < 13.8f, true);
  const int orc = 14;
  for (int type : {0, 6, 7, 11, 12, 14, 18, 19, 20}) CHECK_EQ(IsSentientRacialType(type), true);
  for (int type : {8, 9, 25, 29}) CHECK_EQ(IsSentientRacialType(type), false);
  // #160 (owner, 2026-11-12: "go with A on all"). Q1 A: careful - Intelligence and Wisdom modifiers together 0+.
  CHECK_EQ(IsCarefulMind(0, 0), true);
  CHECK_EQ(IsCarefulMind(-2, 2), true);
  CHECK_EQ(IsCarefulMind(-2, 0), false);  // an orc (Int 7, Wis 11): plows through
  CHECK_EQ(IsCarefulMind(1, -2), false);
  using K = HazardKind;
  using S = TerrainStep;
  // Entangle's Difficult Terrain: a careful archer walks out once; a careful melee one too, unless its target is in.
  CHECK_EQ(TerrainAreaStep(K::kDifficultTerrain, true, orc, true, true, false, false) == S::kWalkOutOnce, true);
  CHECK_EQ(TerrainAreaStep(K::kDifficultTerrain, true, orc, true, true, true, false) == S::kWalkOutOnce, true);
  CHECK_EQ(TerrainAreaStep(K::kDifficultTerrain, true, orc, true, false, false, false) == S::kWalkOutOnce, true);
  CHECK_EQ(TerrainAreaStep(K::kDifficultTerrain, true, orc, true, false, true, false) == S::kStay, true);
  CHECK_EQ(TerrainAreaStep(K::kDifficultTerrain, true, orc, false, true, false, false) == S::kStay, true);  // dumb
  // Grease (Q3 A): the same creatures keep out of it; the dumb plow through.
  CHECK_EQ(TerrainAreaStep(K::kGrease, true, orc, true, true, true, false) == S::kKeepOut, true);
  CHECK_EQ(TerrainAreaStep(K::kGrease, true, orc, true, false, false, false) == S::kKeepOut, true);
  CHECK_EQ(TerrainAreaStep(K::kGrease, true, orc, true, false, true, false) == S::kStay, true);
  CHECK_EQ(TerrainAreaStep(K::kGrease, true, orc, false, true, false, false) == S::kStay, true);
  // Never a held creature, an animal, a non-avoider (undead, charmed, a player's); the fog and NWN's areas: no step.
  CHECK_EQ(TerrainAreaStep(K::kGrease, true, orc, true, true, false, true) == S::kStay, true);
  CHECK_EQ(TerrainAreaStep(K::kGrease, true, 8, true, true, false, false) == S::kStay, true);
  CHECK_EQ(TerrainAreaStep(K::kGrease, false, orc, true, true, false, false) == S::kStay, true);
  CHECK_EQ(TerrainAreaStep(K::kFog, true, orc, true, true, false, false) == S::kStay, true);
  CHECK_EQ(TerrainAreaStep(K::kSpell, true, orc, true, true, false, false) == S::kStay, true);
  // Q2 A: who searches the fog - one the game moves, in control, whose enemy vanished into it.
  CHECK_EQ(SearchesFog(true, true, true, true, false), true);
  CHECK_EQ(SearchesFog(false, true, true, true, false), false);  // a player's character
  CHECK_EQ(SearchesFog(true, false, true, true, false), false);  // charmed, dominated
  CHECK_EQ(SearchesFog(true, true, false, true, false), false);  // not its enemy
  CHECK_EQ(SearchesFog(true, true, true, false, false), false);  // not in a fog
  CHECK_EQ(SearchesFog(true, true, true, true, true), false);    // it still has a target
  CHECK_EQ(kFogSearchRounds, 3);
}
