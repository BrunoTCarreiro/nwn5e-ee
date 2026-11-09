#include "check.h"
#include "rules/mark_surface.h"

#include <math.h>

#include <optional>
#include <string>
#include <vector>

using namespace rules;

namespace {

bool CloseTo(float a, float b) { return fabsf(a - b) < 0.002f; }
bool CloseTo(const Vec3f& a, const Vec3f& b) { return CloseTo(a.x, b.x) && CloseTo(a.y, b.y) && CloseTo(a.z, b.z); }
// A turn that takes the frame's axes to these (either sign of the quaternion).
bool Turns(const Quatf& q, const Vec3f& x, const Vec3f& y, const Vec3f& z) {
  return CloseTo(Rotate(q, Vec3f{1, 0, 0}), x) && CloseTo(Rotate(q, Vec3f{0, 1, 0}), y) &&
         CloseTo(Rotate(q, Vec3f{0, 0, 1}), z);
}
constexpr float kHalfRoot2 = 0.70710678f;
// A turn of `degrees` about `axis`.
Quatf AxisTurn(const Vec3f& axis, float degrees) {
  const float half = degrees * 3.14159265f / 360.0f;
  const Vec3f unit = Normalized(axis);
  return {cosf(half), unit.x * sinf(half), unit.y * sinf(half), unit.z * sinf(half)};
}

}  // namespace

// #39 (0.96.2; owner, 2026-11-09: "go with 1"): Magic Mark on a model's surface - the turns. Breaks caught: a turn
// read the other way round, any of the matrix's four cases wrong (turns about slanted axes, each case), the mark's face or top on the wrong axis, a table's
// mark not reading from the caster's side, a slanted surface's top not up its slope.
TEST(SurfaceMarkTurns) {
  const Quatf quarter{kHalfRoot2, 0, 0, kHalfRoot2};  // a quarter turn about z
  CHECK_EQ(CloseTo(Rotate(quarter, Vec3f{1, 0, 0}), Vec3f{0, 1, 0}), true);
  CHECK_EQ(CloseTo(Rotate(Conjugate(quarter), Vec3f{1, 0, 0}), Vec3f{0, -1, 0}), true);
  // The matrix's four cases: none, and half turns about x, y and z.
  CHECK_EQ(Turns(TurnFromAxes({1, 0, 0}, {0, 1, 0}, {0, 0, 1}), {1, 0, 0}, {0, 1, 0}, {0, 0, 1}), true);
  CHECK_EQ(Turns(TurnFromAxes({0, 1, 0}, {-1, 0, 0}, {0, 0, 1}), {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}), true);
  CHECK_EQ(Turns(TurnFromAxes({1, 0, 0}, {0, -1, 0}, {0, 0, -1}), {1, 0, 0}, {0, -1, 0}, {0, 0, -1}), true);
  CHECK_EQ(Turns(TurnFromAxes({-1, 0, 0}, {0, 1, 0}, {0, 0, -1}), {-1, 0, 0}, {0, 1, 0}, {0, 0, -1}), true);
  CHECK_EQ(Turns(TurnFromAxes({-1, 0, 0}, {0, -1, 0}, {0, 0, 1}), {-1, 0, 0}, {0, -1, 0}, {0, 0, 1}), true);
  CHECK_EQ(Turns(TurnFromAxes({0, 0, 1}, {1, 0, 0}, {0, 1, 0}), {0, 0, 1}, {1, 0, 0}, {0, 1, 0}), true);
  // Slanted axes: a small turn (the trace's case), and near half turns mostly about x, y or z (theirs).
  for (const Quatf& q : {AxisTurn({1, 2, 3}, 60), AxisTurn({0.9f, 0.3f, 0.3f}, 170), AxisTurn({0.3f, 0.9f, 0.3f}, 170),
                        AxisTurn({0.3f, 0.3f, 0.9f}, 170), AxisTurn({-2, 1, 0.5f}, 250)}) {
    const Vec3f x = Rotate(q, Vec3f{1, 0, 0}), y = Rotate(q, Vec3f{0, 1, 0}), z = Rotate(q, Vec3f{0, 0, 1});
    CHECK_EQ(Turns(TurnFromAxes(x, y, z), x, y, z), true);
  }
  // A door's face toward the caster (south of it): the mark faces south, its top up, its +x west (read from the south,
  // its texture runs to the reader's right).
  CHECK_EQ(Turns(MarkTurnOn({0, -1, 0}, {0, 1, 0}), {-1, 0, 0}, {0, -1, 0}, {0, 0, 1}), true);
  // A table's top seen from the south-west: the face up, the top away from the caster.
  CHECK_EQ(Turns(MarkTurnOn({0, 0, 1}, {0.6f, 0.8f, -0.5f}), {-0.8f, 0.6f, 0}, {0, 0, 1}, {0.6f, 0.8f, 0}), true);
  // A slope facing south and up (not flat enough for a top): its top up the slope.
  CHECK_EQ(Turns(MarkTurnOn({0, -kHalfRoot2, kHalfRoot2}, {0, 1, 0}), {-1, 0, 0}, {0, -kHalfRoot2, kHalfRoot2},
                 {0, kHalfRoot2, kHalfRoot2}),
           true);
}

// Where the eyes aim and the lines go. Breaks caught: the top's or the eyes' height not taken, the 80% band off, an
// aim point beside the box's middle, the line not passing the box, the extra rays not at right angles (looking
// straight down too).
TEST(SurfaceMarkAim) {
  const Vec3f door_min{-1.05f, -0.15f, 0}, door_max{1.05f, 0.15f, 2.97f};
  CHECK_EQ(CloseTo(MarkAimPoint({0, -3, 1.6f}, door_min, door_max), Vec3f{0, 0, 1.6f}), true);
  CHECK_EQ(CloseTo(MarkAimPoint({0, -3, 0.1f}, door_min, door_max), Vec3f{0, 0, 0.297f}), true);
  CHECK_EQ(CloseTo(MarkAimPoint({0, -3, 2.9f}, door_min, door_max), Vec3f{0, 0, 2.673f}), true);
  const Vec3f table_min{-1, -0.5f, 0}, table_max{1, 0.5f, 0.8f};
  CHECK_EQ(CloseTo(MarkAimPoint({0, -3, 1.6f}, table_min, table_max), Vec3f{0, 0, 0.76f}), true);
  const std::vector<Vec3f> aims = MarkAimPoints({0, -3, 1.6f}, table_min, table_max);
  CHECK_EQ(static_cast<int>(aims.size()), 7);
  CHECK_EQ(CloseTo(aims[1], Vec3f{0.7f, 0, 0.76f}), true);
  CHECK_EQ(CloseTo(aims[2], Vec3f{-0.7f, 0, 0.76f}), true);
  CHECK_EQ(CloseTo(aims[4], Vec3f{0, -0.35f, 0.76f}), true);
  CHECK_EQ(CloseTo(aims[5], Vec3f{0, 0, 0.8f}), true);  // no higher than the top
  CHECK_EQ(CloseTo(aims[6], Vec3f{0, 0, 0.56f}), true);
  // The line runs on past the aim by the box's diagonal (2.1 x 0.3 x 2.97: 3.65 m).
  CHECK_EQ(CloseTo(MarkRayEnd({0, -3, 1.6f}, {0, 0, 1.6f}, door_min, door_max), Vec3f{0, 3.6498f, 1.6f}), true);
  for (const Vec3f& direction : {Vec3f{0, 1, 0}, Vec3f{0.3f, 0.8f, -0.5f}, Vec3f{0, 0, -1}}) {
    Vec3f u, v;
    MarkRayOffsets(direction, &u, &v);
    CHECK_EQ(CloseTo(Length(u), kMarkNormalOffset), true);
    CHECK_EQ(CloseTo(Length(v), kMarkNormalOffset), true);
    CHECK_EQ(CloseTo(Dot(u, Normalized(direction)), 0), true);
    CHECK_EQ(CloseTo(Dot(v, Normalized(direction)), 0), true);
    CHECK_EQ(CloseTo(Dot(u, v), 0), true);
  }
  CHECK_EQ(CloseTo(MarkEyeHeight(3), 1.6f), true);
  CHECK_EQ(CloseTo(MarkEyeHeight(2), 1.0f), true);
  CHECK_EQ(CloseTo(MarkEyeHeight(5), 3.6f), true);
  CHECK_EQ(CloseTo(MarkEyeHeight(0), 1.6f), true);
}

// The surface's normal from three hits. Breaks caught: the normal facing away from the eyes, a missed extra ray or
// one on another surface trusted, three hits in a line trusted.
TEST(SurfaceMarkNormals) {
  const Vec3f d = Normalized({0, 3, -0.84f});
  const Vec3f p0{0, 2.94f, 0.8f};
  const Vec3f across{0.02f, 2.94f, 0.8f}, along{0, 2.99f, 0.8f};
  CHECK_EQ(CloseTo(MarkSurfaceNormal(d, p0, across, along), Vec3f{0, 0, 1}), true);
  CHECK_EQ(CloseTo(MarkSurfaceNormal(d, p0, along, across), Vec3f{0, 0, 1}), true);  // either order
  const Vec3f back = Scale(d, -1);
  CHECK_EQ(CloseTo(MarkSurfaceNormal(d, p0, std::nullopt, along), back), true);
  CHECK_EQ(CloseTo(MarkSurfaceNormal(d, p0, across, std::nullopt), back), true);
  CHECK_EQ(CloseTo(MarkSurfaceNormal(d, p0, across, Add(p0, Scale(d, 0.5f))), back), true);  // another surface
  CHECK_EQ(CloseTo(MarkSurfaceNormal(d, p0, across, Vec3f{0.04f, 2.94f, 0.8f}), back), true);  // in a line
  // A wall facing west, seen from the west.
  const Vec3f east{1, 0, 0};
  CHECK_EQ(CloseTo(MarkSurfaceNormal(east, {5, 0, 1}, Vec3f{5, 0.02f, 1}, Vec3f{5, 0, 1.02f}), Vec3f{-1, 0, 0}), true);
}

// The mark's place in the part it's on. Breaks caught: the gap dropped or inward, the part's turn taken the other way
// or not at all, its position not taken off, the mark's turn not brought into the part's frame.
TEST(SurfaceMarkFix) {
  // A door's leaf at (10, 0, 0), turned a quarter about z (its x along the world's y); the caster south of it.
  const Vec3f part_at{10, 0, 0};
  const Quatf part_turn{kHalfRoot2, 0, 0, kHalfRoot2};
  const MarkFix fix = MarkFixInPart("door01_leaf", {10, -1, 1.5f}, {0, -1, 0}, {0, 1, 0}, part_at, part_turn);
  CHECK_EQ(fix.part == "door01_leaf", true);
  CHECK_EQ(CloseTo(fix.position, Vec3f{-1.02f, 0, 1.5f}), true);
  CHECK_EQ(Turns(fix.turn, {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}), true);
  // Back in the world: where the mark was meant to be, facing the caster.
  CHECK_EQ(CloseTo(Add(part_at, Rotate(part_turn, fix.position)), Vec3f{10, -1.02f, 1.5f}), true);
  CHECK_EQ(CloseTo(Rotate(part_turn, Rotate(fix.turn, Vec3f{0, 1, 0})), Vec3f{0, -1, 0}), true);
  // A part not turned: the world's numbers less its position.
  const MarkFix still = MarkFixInPart("top", {0, 2.94f, 0.8f}, {0, 0, 1}, {0, 1, -0.3f}, {0, 0, 0.5f}, Quatf{});
  CHECK_EQ(CloseTo(still.position, Vec3f{0, 2.94f, 0.32f}), true);
  CHECK_EQ(Turns(still.turn, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}), true);
}

// The record kept on the object. Breaks caught: the eyes or the fix lost on the way through, a record that isn't one
// read, a fix that doesn't read refusing the eyes, a part's name with a space written.
TEST(SurfaceMarkRecord) {
  MarkRecord record;
  record.eye = {12.5f, -3.25f, 1.6f};
  CHECK_EQ(MarkRecordText(record) == "e 12.500 -3.250 1.600", true);
  MarkRecord read;
  CHECK_EQ(ParseMarkRecord(MarkRecordText(record), &read), true);
  CHECK_EQ(CloseTo(read.eye, record.eye), true);
  CHECK_EQ(read.fix.has_value(), false);
  MarkFix fix;
  fix.part = "plc_table_top";
  fix.position = {0.25f, -0.5f, 0.82f};
  fix.turn = {0.5f, 0.5f, -0.5f, 0.5f};
  record.fix = fix;
  CHECK_EQ(ParseMarkRecord(MarkRecordText(record), &read), true);
  CHECK_EQ(read.fix.has_value(), true);
  CHECK_EQ(read.fix->part == "plc_table_top", true);
  CHECK_EQ(CloseTo(read.fix->position, fix.position), true);
  CHECK_EQ(CloseTo(read.fix->turn.w, 0.5f) && CloseTo(read.fix->turn.x, 0.5f) && CloseTo(read.fix->turn.y, -0.5f) &&
               CloseTo(read.fix->turn.z, 0.5f),
           true);
  CHECK_EQ(ParseMarkRecord("x 1 2 3", &read), false);
  CHECK_EQ(ParseMarkRecord("e 1 2", &read), false);
  CHECK_EQ(ParseMarkRecord("e 1 2 3|nonsense", &read), true);
  CHECK_EQ(read.fix.has_value(), false);
  CHECK_EQ(CloseTo(read.eye, Vec3f{1, 2, 3}), true);
  record.fix->part = "a leaf";
  CHECK_EQ(MarkRecordText(record) == "e 12.500 -3.250 1.600", true);
  // 0.96.3: the click's ray, before the fix.
  MarkRecord clicked;
  clicked.eye = {1, 2, 3};
  clicked.click = MarkRay{{10, 20, 8.5f}, {12, 24, 1.25f}};
  CHECK_EQ(MarkRecordText(clicked) == "e 1.000 2.000 3.000|c 10.000 20.000 8.500 12.000 24.000 1.250", true);
  CHECK_EQ(ParseMarkRecord(MarkRecordText(clicked), &read), true);
  CHECK_EQ(read.click.has_value(), true);
  CHECK_EQ(CloseTo(read.click->from, Vec3f{10, 20, 8.5f}) && CloseTo(read.click->to, Vec3f{12, 24, 1.25f}), true);
  CHECK_EQ(read.fix.has_value(), false);
  clicked.fix = fix;
  CHECK_EQ(ParseMarkRecord(MarkRecordText(clicked), &read), true);
  CHECK_EQ(read.click.has_value() && read.fix.has_value(), true);
  CHECK_EQ(read.fix->part == "plc_table_top", true);
  CHECK_EQ(CloseTo(read.fix->position, fix.position), true);
  CHECK_EQ(CloseTo(read.click->to, Vec3f{12, 24, 1.25f}), true);
  CHECK_EQ(ParseMarkRecord("e 1 2 3|f leaf 1 2 3 1 0 0 0", &read), true);  // 0.96.2's: no click
  CHECK_EQ(read.click.has_value(), false);
  CHECK_EQ(read.fix.has_value() && read.fix->part == "leaf", true);
}

// 0.96.3 (owner, 2026-11-09: "A. but we should try to find a way to deal with walls that have floors behind them so
// that the wall is marked and not the floor"): a click on a wall. Breaks caught: the ray not run on past the cursor,
// (0.96.8) a wall, a top or the floor told wrong (steep surfaces, the height over the walkmesh, none under it), the cast's point behind the wall or not at
// the feet, a wall mark facing into the wall or along it, a flat surface given an upright mark.
TEST(SurfaceMarkClicks) {
  CHECK_EQ(CloseTo(MarkClickEnd(MarkRay{{0, 0, 10}, {0, 3, 6}}), Vec3f{0, 5.4f, 2.8f}), true);
  // 0.96.8: by the cursor's surface and the walkmesh's height under it (0 here).
  CHECK_EQ(ClassifyMarkClick({5, 0, 1.5f}, {0, -1, 0}, 0.0f) == MarkClickKind::kWall, true);  // a wall
  CHECK_EQ(ClassifyMarkClick({5, 0, 0.1f}, {-0.7f, -0.7f, 0.1f}, 0.0f) == MarkClickKind::kWall, true);  // its foot
  CHECK_EQ(ClassifyMarkClick({5, 0, 0.8f}, {0, -1, 0}, std::nullopt) == MarkClickKind::kWall, true);  // the map's edge
  CHECK_EQ(ClassifyMarkClick({5, 0, 0.45f}, {0, 0, 1}, 0.0f) == MarkClickKind::kTop, true);  // a bench's seat
  CHECK_EQ(ClassifyMarkClick({5, 0, 1.2f}, {0, 0, 1}, std::nullopt) == MarkClickKind::kTop, true);  // no walkmesh
  CHECK_EQ(ClassifyMarkClick({5, 0, 0.2f}, {0, 0.1f, 0.99f}, 0.0f) == MarkClickKind::kFloor, true);  // a step up
  CHECK_EQ(ClassifyMarkClick({5, 0, 0.02f}, {0, 0, 1}, 0.0f) == MarkClickKind::kFloor, true);
  CHECK_EQ(ClassifyMarkClick({5, 0, -0.1f}, {0, 0, 1}, 0.0f) == MarkClickKind::kFloor, true);  // a dip
  // A wall west of the camera's line: the cursor at (5, 0, 1.5), the camera at (5, -8, 9); the caster's feet at 0.
  CHECK_EQ(CloseTo(MarkCastPointBefore({5, 0, 1.5f}, {5, -8, 9}, 0.25f), Vec3f{5, -0.6f, 0.25f}), true);
  CHECK_EQ(CloseTo(MarkCastPointBefore({0, 0, 1}, {-3, -4, 6}, 0), Vec3f{-0.36f, -0.48f, 0}), true);
  // The wall's face toward the camera: facing out of it.
  CHECK_EQ(CloseTo(MarkFacingOf({0, -1, 0}), 270.0f), true);
  CHECK_EQ(CloseTo(MarkFacingOf({1, 0, 0}), 0.0f), true);
  CHECK_EQ(CloseTo(MarkFacingOf({-kHalfRoot2, kHalfRoot2, 0}), 135.0f), true);
  CHECK_EQ(MarkStandsUpright({0, -1, 0}), true);
  CHECK_EQ(MarkStandsUpright({0, -0.7f, 0.7f}), true);
  CHECK_EQ(MarkStandsUpright({0, 0.1f, 0.99f}), false);  // a ledge's top
  CHECK_EQ(MarkStandsUpright({0, 0, -1}), false);        // a ceiling
  // The cursor on the caster's own body (feet at (3, 3, 0.5)): not a wall.
  CHECK_EQ(MarkCursorOnCaster({3.3f, 2.8f, 1.7f}, {3, 3, 0.5f}), true);
  CHECK_EQ(MarkCursorOnCaster({3.7f, 3, 1.7f}, {3, 3, 0.5f}), false);  // 0.7 m off: beside it
  CHECK_EQ(MarkCursorOnCaster({3, 3, 2.9f}, {3, 3, 0.5f}), false);     // above its head
  CHECK_EQ(MarkCursorOnCaster({3, 3, 0.2f}, {3, 3, 0.5f}), false);     // below its feet
  // 0.96.5: a door's transition surface passed through, its leaf and others not.
  CHECK_EQ(MarkPassesThrough("sam"), true);
  CHECK_EQ(MarkPassesThrough("SAM"), true);
  CHECK_EQ(MarkPassesThrough("Mesh05"), false);
  CHECK_EQ(MarkPassesThrough("sample"), false);
  CHECK_EQ(MarkPassesThrough("sap"), false);
  CHECK_EQ(MarkPassesThrough(""), false);
}
