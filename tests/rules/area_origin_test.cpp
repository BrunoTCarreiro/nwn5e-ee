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

// #184's house rule (areas_fill_around): what is a lasting area. Breaks caught: one of the seven area-making spells left
// out (its saves as it appears still shaded by placeables); an instant spell taken for one - a cone (Burning Hands,
// Color Spray, a Dragon's Breath exhale, Cone of Cold), a burst (Shatter, Fireball, Ice Knife), Thunderwave's cube, Sleep's
// sphere, Faerie Fire's square (a square, but cast once); the shape taken for the test (Fog Cloud and Shatter both
// spheres); an area-of-effect object's own area not lasting whatever its spell.
TEST(AreaLastingOrInstant) {
  for (int spell : {kEntangle, kGrease, kFogCloud, kNwnWeb, kNwnCloudkill, kNwnStinkingCloud, kNwnIncendiaryCloud}) {
    CHECK_EQ(LastingAreaSpell(spell) && AreaIsLasting(false, spell), true);
  }
  for (int spell : {kBurningHands, kColorSpray, kDragonsBreathExhaleFirst, kDragonsBreathExhaleLast, 25, kShatter, 58,
                    kIceKnife, kThunderwave, kSleep, kFaerieFire, kFaerieFireBlue, -1}) {
    CHECK_EQ(LastingAreaSpell(spell) || AreaIsLasting(false, spell), false);
  }
  CHECK_EQ(AreaIsLasting(true, kShatter) && AreaIsLasting(true, -1) && AreaIsLasting(true, kFogCloud), true);
}

// What blocks a lasting area's lines. Breaks caught: the house rule taking placeables out of an instant area's lines
// (bursts stay as 2024), or leaving them in a lasting one's; the rule off (0, the 2024 rule as built) not counting them.
TEST(AreaFillsAroundPlaceables) {
  CHECK_EQ(AreaCountsPlaceables(true, true), false);   // a cloud or a zone, the house rule on: walls only
  CHECK_EQ(AreaCountsPlaceables(false, true), true);   // a cone, a line, a burst: placeables block
  CHECK_EQ(AreaCountsPlaceables(true, false), true);   // the house rule off: SRD 5.2.1 as built
  CHECK_EQ(AreaCountsPlaceables(false, false), true);
}

// #166 / #184 decision A: which NWN spells' gathers leave a creature out. Breaks caught: one of the five area shapes
// left out (a cone's, a line's), the case minded; a single-target spell ("****") or a spell with no shape taken; one of
// nwn5e's own (converted: it leaves out in its own code) taken too.
TEST(NwnAreaSpellsGatherByShape) {
  for (const char* shape : {"cone", "sphere", "hsphere", "cube", "rectangle", "Sphere", "CONE"}) {
    CHECK_EQ(NwnAreaSpell(shape, false), true);
  }
  CHECK_EQ(NwnAreaSpell("****", false) || NwnAreaSpell("", false) || NwnAreaSpell("spellcone", false), false);
  CHECK_EQ(NwnAreaSpell("sphere", true) || NwnAreaSpell("cone", true), false);
}

// The skip and the save. Breaks caught: a creature with Half or Three-Quarters Cover skipped (they save at +2 / +5); a
// placeable or door skipped (only creatures); the caster skipped from its own spell's gather; Total Cover kept in; the
// +5 for Total Cover kept where the gather left it out, or lost elsewhere; Half's or Three-Quarters' bonus lost.
TEST(NwnGatherSkipsTotalCoverOnly) {
  CHECK_EQ(GatherSkips(true, false, Cover::kTotal), true);
  CHECK_EQ(GatherSkips(true, false, Cover::kThreeQuarters) || GatherSkips(true, false, Cover::kHalf), false);
  CHECK_EQ(GatherSkips(true, false, Cover::kNone), false);
  CHECK_EQ(GatherSkips(false, false, Cover::kTotal), false);  // not a creature
  CHECK_EQ(GatherSkips(true, true, Cover::kTotal), false);    // the caster
  CHECK_EQ(AreaSaveCoverBonus(Cover::kTotal, true), 0);
  CHECK_EQ(AreaSaveCoverBonus(Cover::kTotal, false), 5);
  CHECK_EQ(AreaSaveCoverBonus(Cover::kThreeQuarters, true), 5);
  CHECK_EQ(AreaSaveCoverBonus(Cover::kHalf, true), 2);
  CHECK_EQ(AreaSaveCoverBonus(Cover::kNone, false), 0);
}
