#include "check.h"
#include "rules/grenades.h"

using namespace rules;

// #124 (owner, 2026-11-04: "good recs, go with them"). Breaks caught: a grenade spell mapped wrong, the joke counted;
// the DC's 8 / Dexterity / PB; Caltrops on the thrower's DC; a flask's or a bomb's dice; the bombs not half on a save,
// a flask half; the throw going at once outside a round, in NWN mode or for a non-grenade.
TEST(GrenadeSpells) {
  CHECK_EQ(GrenadeForSpell(464) == Grenade::kAlchemistsFire, true);
  CHECK_EQ(GrenadeForSpell(465) == Grenade::kTanglefoot, true);
  CHECK_EQ(GrenadeForSpell(466) == Grenade::kHolyWater, true);
  CHECK_EQ(GrenadeForSpell(467) == Grenade::kChokingPowder, true);
  CHECK_EQ(GrenadeForSpell(468) == Grenade::kThunderstone, true);
  CHECK_EQ(GrenadeForSpell(469) == Grenade::kAcid, true);
  CHECK_EQ(GrenadeForSpell(471) == Grenade::kCaltrops, true);
  CHECK_EQ(GrenadeForSpell(744) == Grenade::kFireBomb, true);
  CHECK_EQ(GrenadeForSpell(745) == Grenade::kAcidBomb, true);
  CHECK_EQ(IsGrenadeSpell(470), false);  // Chickenegg, the joke
  CHECK_EQ(IsGrenadeSpell(463), false);
  CHECK_EQ(IsGrenadeSpell(746), false);
}

TEST(GrenadeRules) {
  CHECK_EQ(GrenadeSaveDc(3, 2), 13);
  CHECK_EQ(GrenadeSaveDc(-1, 4), 11);
  CHECK_EQ(GrenadeSaveOnThrowerDc(Grenade::kTanglefoot), true);
  CHECK_EQ(GrenadeSaveOnThrowerDc(Grenade::kFireBomb), true);
  CHECK_EQ(GrenadeSaveOnThrowerDc(Grenade::kCaltrops), false);
  CHECK_EQ(GrenadeSaveOnThrowerDc(Grenade::kNone), false);
  CHECK_EQ(GrenadeHitDice(Grenade::kAcid).count == 2 && GrenadeHitDice(Grenade::kAcid).sides == 6, true);
  CHECK_EQ(GrenadeHitDice(Grenade::kAlchemistsFire).count == 1 && GrenadeHitDice(Grenade::kAlchemistsFire).sides == 4,
           true);
  CHECK_EQ(GrenadeHitDice(Grenade::kHolyWater).count == 2 && GrenadeHitDice(Grenade::kHolyWater).sides == 8, true);
  CHECK_EQ(GrenadeHitDice(Grenade::kFireBomb).count == 10 && GrenadeHitDice(Grenade::kAcidBomb).sides == 6, true);
  CHECK_EQ(GrenadeHitDice(Grenade::kTanglefoot).count, 0);
  CHECK_EQ(GrenadeHalfOnSave(Grenade::kFireBomb) && GrenadeHalfOnSave(Grenade::kAcidBomb), true);
  CHECK_EQ(GrenadeHalfOnSave(Grenade::kAcid), false);
  CHECK_EQ(ThrowGoesNow(true, true, true), true);
  CHECK_EQ(ThrowGoesNow(false, true, true), false);
  CHECK_EQ(ThrowGoesNow(true, false, true), false);
  CHECK_EQ(ThrowGoesNow(true, true, false), false);
  CHECK_EQ(kBurningRounds == 3 && kBurningDieSides == 4, true);
  CHECK_EQ(kCaltropsSaveDc == 15 && kCaltropsDamage == 1 && kCaltropsFieldDamage == 25, true);
}

// The descriptions in 2024 terms. Breaks caught: one swapped for another's, a grenade's left NWN's, another item's
// swapped (2993 Coldstone, 2996).
TEST(GrenadeDescriptions) {
  CHECK_EQ(GrenadeDescription2024(2989), 16000555u);
  CHECK_EQ(GrenadeDescription2024(2990), 16000556u);
  CHECK_EQ(GrenadeDescription2024(2991), 16000557u);
  CHECK_EQ(GrenadeDescription2024(2992), 16000558u);
  CHECK_EQ(GrenadeDescription2024(2994), 16000559u);
  CHECK_EQ(GrenadeDescription2024(2995), 16000560u);
  CHECK_EQ(GrenadeDescription2024(2997), 16000561u);
  CHECK_EQ(GrenadeDescription2024(84353), 16000562u);
  CHECK_EQ(GrenadeDescription2024(84354), 16000563u);
  CHECK_EQ(GrenadeDescription2024(2993), 0u);
  CHECK_EQ(GrenadeDescription2024(2996), 0u);
}
