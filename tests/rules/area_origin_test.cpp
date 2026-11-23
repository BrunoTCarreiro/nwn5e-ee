#include <math.h>

#include "check.h"
#include "rules/area_origin.h"

using namespace rules;

namespace {
bool Near(float a, float b) { return fabsf(a - b) < 0.01f; }
}  // namespace

// #166 decision 2 (owner, 2026-11-23): each shape's point of origin. Breaks caught: a cone or a cube from the target
// point, a sphere from the caster, a ground area or a falling cylinder from its centre instead of its top, NWN's line
// (rectangle) or cone from the target, its sphere or cube from the caster, a from-above spell missed or a Fireball taken
// for one, the heights (eyes, half the cube's side, the sphere's centre, the top) wrong.
TEST(AreaOriginPerShape) {
  CHECK_EQ(OriginOf2024(SpellShape2024::kCone) == AreaOrigin::kCaster, true);
  CHECK_EQ(OriginOf2024(SpellShape2024::kCubeFromCaster) == AreaOrigin::kCasterFace, true);
  CHECK_EQ(OriginOf2024(SpellShape2024::kSphere) == AreaOrigin::kCentre, true);
  CHECK_EQ(OriginOf2024(SpellShape2024::kSquare) == AreaOrigin::kTop, true);
  CHECK_EQ(OriginOf2024(SpellShape2024::kOne) == AreaOrigin::kCaster, true);
  CHECK_EQ(OriginOfNwn("cone", false) == AreaOrigin::kCaster, true);
  CHECK_EQ(OriginOfNwn("rectangle", false) == AreaOrigin::kCaster, true);
  CHECK_EQ(OriginOfNwn("Sphere", false) == AreaOrigin::kCentre, true);
  CHECK_EQ(OriginOfNwn("hsphere", false) == AreaOrigin::kCentre, true);
  CHECK_EQ(OriginOfNwn("cube", false) == AreaOrigin::kCentre, true);
  CHECK_EQ(OriginOfNwn("****", false) == AreaOrigin::kCaster, true);
  CHECK_EQ(OriginOfNwn("sphere", true) == AreaOrigin::kTop, true);
  CHECK_EQ(ComesFromAbove("Flame_Strike") && ComesFromAbove("ice_storm") && ComesFromAbove("Call_Lightning"), true);
  CHECK_EQ(ComesFromAbove("Storm_of_Vengeance") && ComesFromAbove("Moonbeam"), true);
  CHECK_EQ(ComesFromAbove("Fireball") || ComesFromAbove("Cone_of_Cold") || ComesFromAbove(""), false);
  CHECK_EQ(Near(OriginHeight(AreaOrigin::kCaster, 1.6f, 15), 1.6f), true);
  CHECK_EQ(Near(OriginHeight(AreaOrigin::kCreature, 2.5f, 10), 2.5f), true);
  CHECK_EQ(Near(OriginHeight(AreaOrigin::kCasterFace, 1.6f, 15), 15 * 0.3048f / 2.0f), true);
  CHECK_EQ(Near(OriginHeight(AreaOrigin::kCentre, 1.6f, 20), 1.0f), true);
  CHECK_EQ(Near(OriginHeight(AreaOrigin::kTop, 1.6f, 20), 20 * 0.3048f), true);
  CHECK_EQ(Near(OriginHeight(AreaOrigin::kTop, 1.6f, 0), 40 * 0.3048f), true);
  CHECK_EQ(OriginAtCaster(AreaOrigin::kCaster) && OriginAtCaster(AreaOrigin::kCasterFace), true);
  CHECK_EQ(OriginAtCaster(AreaOrigin::kCreature), true);
  CHECK_EQ(OriginAtCaster(AreaOrigin::kCentre) || OriginAtCaster(AreaOrigin::kTop), false);
}

// In or out, and the save. Breaks caught: Total Cover keeping a creature in nwn5e's own area, NWN's own leaving it
// out, Half or Three-Quarters leaving it out, the bonus given to a save that isn't Dexterity's or kept from it.
TEST(AreaCoverInOrOut) {
  CHECK_EQ(AreaCoverEffectFor(Cover::kTotal, true, true) == AreaCoverEffect::kLeftOut, true);
  CHECK_EQ(AreaCoverEffectFor(Cover::kTotal, true, false) == AreaCoverEffect::kLeftOut, true);
  CHECK_EQ(AreaCoverEffectFor(Cover::kTotal, false, true) == AreaCoverEffect::kSaveBonus, true);
  CHECK_EQ(AreaCoverEffectFor(Cover::kTotal, false, false) == AreaCoverEffect::kNone, true);
  CHECK_EQ(AreaCoverEffectFor(Cover::kThreeQuarters, true, true) == AreaCoverEffect::kSaveBonus, true);
  CHECK_EQ(AreaCoverEffectFor(Cover::kHalf, true, false) == AreaCoverEffect::kNone, true);
  CHECK_EQ(AreaCoverEffectFor(Cover::kNone, true, true) == AreaCoverEffect::kNone, true);
}
