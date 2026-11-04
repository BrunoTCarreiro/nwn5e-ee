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

// #130 (owner, 2026-11-04: "1A (later we'll check if it's too small), 2B, 3A, 4B"), the flask patches' house rule.
// Breaks caught: a patch with the house rule off or the grenades NWN's, a bomb's or Caltrops' or Tanglefoot's patch,
// a flask without one, two flasks' areas swapped, 3 rounds, the lock a full round (the next tick missed), the house
// rule's descriptions not swapped or swapped for another grenade.
TEST(FlaskPatches) {
  CHECK_EQ(FlaskMakesPatch(Grenade::kAcid, true, true), true);
  CHECK_EQ(FlaskMakesPatch(Grenade::kAlchemistsFire, true, true), true);
  CHECK_EQ(FlaskMakesPatch(Grenade::kHolyWater, true, true), true);
  CHECK_EQ(FlaskMakesPatch(Grenade::kAcid, true, false), false);  // the house rule off (the default)
  CHECK_EQ(FlaskMakesPatch(Grenade::kHolyWater, false, true), false);  // NWN's grenades
  CHECK_EQ(FlaskMakesPatch(Grenade::kFireBomb, true, true), false);
  CHECK_EQ(FlaskMakesPatch(Grenade::kCaltrops, true, true), false);
  CHECK_EQ(FlaskMakesPatch(Grenade::kTanglefoot, true, true), false);
  CHECK_EQ(FlaskPatchAreaEffect(Grenade::kAcid), 100);
  CHECK_EQ(FlaskPatchAreaEffect(Grenade::kAlchemistsFire), 101);
  CHECK_EQ(FlaskPatchAreaEffect(Grenade::kHolyWater), 102);
  CHECK_EQ(FlaskPatchAreaEffect(Grenade::kAcidBomb), -1);
  CHECK_EQ(kFlaskPatchRounds, 2);
  CHECK_EQ(kFlaskPatchLockSeconds > 0.0f && kFlaskPatchLockSeconds < 6.0f, true);
  CHECK_EQ(GrenadeDescription2024(2989, true), 16000564u);
  CHECK_EQ(GrenadeDescription2024(2990, true), 16000565u);
  CHECK_EQ(GrenadeDescription2024(2994, true), 16000566u);
  CHECK_EQ(GrenadeDescription2024(2991, true), 16000557u);  // Caltrops: no patch, its 2024 text
  CHECK_EQ(GrenadeDescription2024(84353, true), 16000562u);
  CHECK_EQ(GrenadeDescription2024(2989, false), 16000555u);
}

// #132 (owner, 2026-11-04: "go with your recommendations" - A): a throw from hiding keeps the hide. Breaks caught: NWN
// mode kept hidden, a spell or another item kept hidden, a creature not hiding counted.
TEST(ThrowFromHiding) {
  CHECK_EQ(ThrowKeepsHiding(true, true, true), true);
  CHECK_EQ(ThrowKeepsHiding(true, true, false), false);  // NWN's: the throw ends it
  CHECK_EQ(ThrowKeepsHiding(true, false, true), false);  // a spell or another item: NWN's
  CHECK_EQ(ThrowKeepsHiding(false, true, true), false);  // not hiding
}

// 0.71.0 (owner, 2026-11-04: "bombs (poison and fire) along with thunderstone should tunr off torches ..."): the blasts
// that put out lights. Breaks caught: a bomb without a blast, the Thunderstone's the bombs', a flask or Caltrops with one.
TEST(GrenadeBlasts) {
  CHECK_EQ(GrenadeBlastRadius(Grenade::kFireBomb) == 5.0f, true);
  CHECK_EQ(GrenadeBlastRadius(Grenade::kAcidBomb) == 5.0f, true);
  CHECK_EQ(GrenadeBlastRadius(Grenade::kThunderstone) > 3.3f && GrenadeBlastRadius(Grenade::kThunderstone) < 3.4f, true);
  CHECK_EQ(GrenadeBlastRadius(Grenade::kAlchemistsFire) == 0.0f, true);
  CHECK_EQ(GrenadeBlastRadius(Grenade::kAcid) == 0.0f, true);
  CHECK_EQ(GrenadeBlastRadius(Grenade::kCaltrops) == 0.0f, true);
  CHECK_EQ(GrenadeBlastRadius(Grenade::kChokingPowder) == 0.0f, true);
}

// The descriptions in 2024 terms. Breaks caught: one swapped for another's, a grenade's left NWN's, another item's
// swapped (2993 Coldstone, 2996).
TEST(GrenadeDescriptions) {
  CHECK_EQ(GrenadeDescription2024(2989, false), 16000555u);
  CHECK_EQ(GrenadeDescription2024(2990, false), 16000556u);
  CHECK_EQ(GrenadeDescription2024(2991, false), 16000557u);
  CHECK_EQ(GrenadeDescription2024(2992, false), 16000558u);
  CHECK_EQ(GrenadeDescription2024(2994, false), 16000559u);
  CHECK_EQ(GrenadeDescription2024(2995, false), 16000560u);
  CHECK_EQ(GrenadeDescription2024(2997, false), 16000561u);
  CHECK_EQ(GrenadeDescription2024(84353, false), 16000562u);
  CHECK_EQ(GrenadeDescription2024(84354, false), 16000563u);
  CHECK_EQ(GrenadeDescription2024(2993, false), 0u);
  CHECK_EQ(GrenadeDescription2024(2996, false), 0u);
}
