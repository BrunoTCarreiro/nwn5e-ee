#include <math.h>

#include <string>

#include "check.h"
#include "rules/cover_objects.h"

using namespace rules;

namespace {
bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
ModelBounds Bounds(float x0, float x1, float y0, float y1, float z0, float z1) {
  ModelBounds b;
  b.lo[0] = x0;
  b.hi[0] = x1;
  b.lo[1] = y0;
  b.hi[1] = y1;
  b.lo[2] = z0;
  b.hi[2] = z1;
  return b;
}
}  // namespace

// #166 part 2, the bounds table. Breaks caught: a comment or a short line read, a value in the wrong place, a box
// inside out accepted, the walkmesh's numbers taken for the bounds, the height floor off.
TEST(CoverBoundsTableLines) {
  int row = -1;
  ModelBounds b;
  CHECK_EQ(ParseBoundsLine("1 plc_a02 -1.004 0.946 -0.273 0.265 -0.002 2.485 walk -1.006 0.994 -0.25 0.27", &row, &b),
           true);
  CHECK_EQ(row, 1);
  CHECK_EQ(Near(b.lo[0], -1.004f) && Near(b.hi[0], 0.946f) && Near(b.lo[1], -0.273f) && Near(b.hi[1], 0.265f) &&
               Near(b.lo[2], -0.002f) && Near(b.hi[2], 2.485f),
           true);
  CHECK_EQ(ParseBoundsLine("97 plc_m05 -1.043 1 -0.503 0.5 0 0.9", &row, &b), true);
  CHECK_EQ(row == 97 && Near(b.hi[2], 0.9f), true);
  CHECK_EQ(ParseBoundsLine("# miss 28 plc_d07 nothing drawn", &row, &b), false);
  CHECK_EQ(ParseBoundsLine("   ", &row, &b), false);
  CHECK_EQ(ParseBoundsLine("5 plc_x 0 1 0 1 0", &row, &b), false);      // short
  CHECK_EQ(ParseBoundsLine("5 plc_x 1 0 0 1 0 1", &row, &b), false);    // inside out
  CHECK_EQ(ParseBoundsLine("-2 plc_x 0 1 0 1 0 1", &row, &b), false);   // no such row
  CHECK_EQ(BoundsCount(Bounds(0, 1, 0, 1, 0, 0.3f)), true);
  CHECK_EQ(BoundsCount(Bounds(0, 1, 0, 1, 0, 0.29f)), false);
  CHECK_EQ(BoundsCount(Bounds(0, 1, 0, 1, 1.0f, 1.2f)), false);  // a sliver up high
}

// The box and the segment. Breaks caught: the model's width put along the facing instead of across it, the centre
// not turned with it, the scale or height ignored, a line beside, over or short of a box taken as through it, one
// through missed, its entry point wrong, the cull keeping a far box or dropping a near one, a point inside missed or
// one outside (beside, past its end, over it, or past a turned box's side) taken as inside, the model's right
// mirrored, a box behind the segment's start counted.
TEST(CoverSegmentAgainstABox) {
  // A bookcase 2 m wide (model x), 0.5 m deep, 2.5 m tall, facing north at (10, 20): it spans x 9-11.
  const ModelBounds shelf = Bounds(-1.0f, 1.0f, -0.25f, 0.25f, 0.0f, 2.5f);
  ObjectBox box = BoxOf(shelf, 10.0f, 20.0f, 0.0f, 0.0f, 1.0f, 1.0f);
  CHECK_EQ(Near(box.lo[0], 9.0f) && Near(box.hi[0], 11.0f) && Near(box.lo[1], 19.75f) && Near(box.hi[1], 20.25f), true);
  const float south[3] = {10.0f, 15.0f, 1.0f}, north[3] = {10.0f, 25.0f, 1.0f};
  float t = -1.0f;
  CHECK_EQ(SegmentHitsBox(south, north, box, &t), true);
  CHECK_EQ(Near(t, 0.475f), true);                     // enters at y 19.75
  const float beside_a[3] = {11.5f, 15.0f, 1.0f}, beside_b[3] = {11.5f, 25.0f, 1.0f};
  CHECK_EQ(SegmentHitsBox(beside_a, beside_b, box), false);
  const float over_a[3] = {10.0f, 15.0f, 2.6f}, over_b[3] = {10.0f, 25.0f, 2.6f};
  CHECK_EQ(SegmentHitsBox(over_a, over_b, box), false);  // over its top
  const float short_b[3] = {10.0f, 19.5f, 1.0f};
  CHECK_EQ(SegmentHitsBox(south, short_b, box), false);  // stops before it
  const float past_a[3] = {10.0f, 21.0f, 1.0f};
  CHECK_EQ(SegmentHitsBox(past_a, north, box), false);  // starts past it, going away
  // Turned to face east: its width now runs north-south, y 19-21 at x 10.
  box = BoxOf(shelf, 10.0f, 20.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  CHECK_EQ(Near(box.lo[1], 19.0f) && Near(box.hi[1], 21.0f) && Near(box.lo[0], 9.75f) && Near(box.hi[0], 10.25f), true);
  const float west[3] = {5.0f, 20.8f, 1.0f}, east[3] = {15.0f, 20.8f, 1.0f};
  CHECK_EQ(SegmentHitsBox(west, east, box), true);
  CHECK_EQ(SegmentHitsBox(south, north, box), true);     // through its 0.5 m depth
  const float west2[3] = {5.0f, 21.2f, 1.0f}, east2[3] = {15.0f, 21.2f, 1.0f};
  CHECK_EQ(SegmentHitsBox(west2, east2, box), false);
  // An off-centre model (a chair whose seat is behind its origin) turned with the facing; the scale doubles it.
  const ModelBounds chair = Bounds(-0.25f, 0.25f, 0.5f, 1.0f, 0.0f, 1.0f);
  box = BoxOf(chair, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 2.0f);  // facing east: model +Y is east
  CHECK_EQ(Near(box.centre[0], 1.5f) && Near(box.centre[1], 0.0f) && Near(box.half[0], 0.5f) && Near(box.half[2], 1.0f),
           true);
  // A model off-centre to its own right (+X): facing east, its right is south.
  const ModelBounds right = Bounds(0.5f, 1.0f, -0.25f, 0.25f, 0.0f, 1.0f);
  box = BoxOf(right, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  CHECK_EQ(Near(box.centre[0], 0.0f) && Near(box.centre[1], -0.75f), true);
  box = BoxOf(chair, 0.0f, 0.0f, 5.0f, 0.0f, 0.0f, 1.0f);  // no facing: north
  CHECK_EQ(Near(box.centre[1], 0.75f) && Near(box.lo[2], 5.0f) && Near(box.hi[2], 6.0f), true);
  // A slanted line: from the eyes (1 m) down to the feet (0.2 m) over a 0.9 m table halfway: through it.
  const ModelBounds table = Bounds(-1.0f, 1.0f, -0.5f, 0.5f, 0.0f, 0.9f);
  box = BoxOf(table, 0.0f, 2.5f, 0.0f, 0.0f, 1.0f, 1.0f);
  const float eye[3] = {0.0f, 0.0f, 1.0f}, feet[3] = {0.0f, 5.0f, 0.2f}, chest[3] = {0.0f, 5.0f, 1.3f};
  CHECK_EQ(SegmentHitsBox(eye, feet, box), true);
  CHECK_EQ(SegmentHitsBox(eye, chest, box), false);
  CHECK_EQ(SegmentNearBox(eye, feet, box), true);
  const float far_a[3] = {20.0f, 0.0f, 1.0f}, far_b[3] = {20.0f, 5.0f, 1.0f};
  CHECK_EQ(SegmentNearBox(far_a, far_b, box), false);
  const float up_a[3] = {0.0f, 0.0f, 3.0f}, up_b[3] = {0.0f, 5.0f, 3.0f};
  CHECK_EQ(SegmentNearBox(up_a, up_b, box), false);
  // A point inside the box blocks a line starting there; a line along a face's plane touches.
  const float inside[3] = {0.0f, 2.5f, 0.5f};
  CHECK_EQ(SegmentHitsBox(inside, chest, box), true);
  const float edge_a[3] = {-5.0f, 3.0f, 0.5f}, edge_b[3] = {5.0f, 3.0f, 0.5f};
  CHECK_EQ(SegmentHitsBox(edge_a, edge_b, box), true);
  CHECK_EQ(PointInBox(inside, box), true);
  const float out_x[3] = {1.1f, 2.5f, 0.5f}, out_y[3] = {0.0f, 3.1f, 0.5f}, out_z[3] = {0.0f, 2.5f, 0.95f};
  CHECK_EQ(PointInBox(out_x, box) || PointInBox(out_y, box) || PointInBox(out_z, box), false);
  box = BoxOf(table, 0.0f, 2.5f, 0.0f, 1.0f, 0.0f, 1.0f);  // turned east: 1 m wide in x, 2 m in y
  const float turned_in[3] = {0.0f, 3.4f, 0.5f}, turned_out[3] = {0.8f, 2.5f, 0.5f};
  CHECK_EQ(PointInBox(turned_in, box) && !PointInBox(turned_out, box), true);
}

// The degrees and what they do. Breaks caught: the counts' boundaries one off (six points or seven), an eye height
// wrong for a size, the slices ending anywhere but a crouched hider's shoulders, Half hiding, Three-Quarters not
// hiding, the save bonuses swapped or Total's missing, a melee attack refused from afar, a ranged one let through, a
// cone's origin at the target point, a sphere's at the caster, Sacred Flame's save getting cover.
TEST(CoverDegreesFromBlockedPoints) {
  CHECK_EQ(CoverFromBlocked(0, 6) == Cover::kNone, true);
  CHECK_EQ(CoverFromBlocked(2, 6) == Cover::kNone, true);
  CHECK_EQ(CoverFromBlocked(3, 6) == Cover::kHalf, true);
  CHECK_EQ(CoverFromBlocked(4, 6) == Cover::kHalf, true);
  CHECK_EQ(CoverFromBlocked(5, 6) == Cover::kThreeQuarters, true);
  CHECK_EQ(CoverFromBlocked(6, 6) == Cover::kTotal, true);
  // #166 (2026-11-23): seven points - Half from 4 (half, rounded up), Three-Quarters from 5 (5.25), Total 7.
  CHECK_EQ(CoverFromBlocked(3) == Cover::kNone && CoverFromBlocked(4) == Cover::kHalf, true);
  CHECK_EQ(CoverFromBlocked(5) == Cover::kThreeQuarters && CoverFromBlocked(6) == Cover::kThreeQuarters, true);
  CHECK_EQ(CoverFromBlocked(7) == Cover::kTotal && CoverFromBlocked(0, 0) == Cover::kNone, true);
  CHECK_EQ(CoverFromBlocked(6, 8) == Cover::kThreeQuarters && CoverFromBlocked(5, 8) == Cover::kHalf, true);
  // The eyes by size, and the slices' end (a crouched Medium hider's shoulders).
  CHECK_EQ(Near(EyeHeight(1), 0.3f) && Near(EyeHeight(2), 0.9f) && Near(EyeHeight(3), 1.6f), true);
  CHECK_EQ(Near(EyeHeight(4), 2.5f) && Near(EyeHeight(5), 4.0f) && Near(EyeHeight(0), 1.6f), true);
  CHECK_EQ(Near(SliceTargetHeight(3), 1.45f * 1.1f / 1.8f), true);
  CHECK_EQ(CoverHides(Cover::kNone) || CoverHides(Cover::kHalf), false);
  CHECK_EQ(CoverHides(Cover::kThreeQuarters) && CoverHides(Cover::kTotal), true);
  CHECK_EQ(CoverSaveBonus(Cover::kNone), 0);
  CHECK_EQ(CoverSaveBonus(Cover::kHalf), 2);
  CHECK_EQ(CoverSaveBonus(Cover::kThreeQuarters), 5);
  CHECK_EQ(CoverSaveBonus(Cover::kTotal), 5);
  CHECK_EQ(SpellIgnoresCover(kSpellSacredFlame), true);
  CHECK_EQ(SpellIgnoresCover(-1) || SpellIgnoresCover(58), false);  // no spell, Fireball
  CHECK_EQ(CoverRefusesAttack(Cover::kTotal, true, 30.0f), true);
  CHECK_EQ(CoverRefusesAttack(Cover::kTotal, false, 2.0f), true);
  CHECK_EQ(CoverRefusesAttack(Cover::kTotal, false, 3.0f), true);
  CHECK_EQ(CoverRefusesAttack(Cover::kTotal, false, 3.5f), false);  // it walks on
  CHECK_EQ(CoverRefusesAttack(Cover::kThreeQuarters, true, 5.0f), false);
  CHECK_EQ(SaveOriginForShape("sphere") == SaveOrigin::kTargetPoint, true);
  CHECK_EQ(SaveOriginForShape("hsphere") == SaveOrigin::kTargetPoint, true);
  CHECK_EQ(SaveOriginForShape("Cone") == SaveOrigin::kCaster, true);
  CHECK_EQ(SaveOriginForShape("rectangle") == SaveOrigin::kCaster, true);
  CHECK_EQ(SaveOriginForShape("****") == SaveOrigin::kVersus, true);
  CHECK_EQ(SaveOriginForShape("") == SaveOrigin::kVersus, true);
}
