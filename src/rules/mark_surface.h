// #39 (0.96.2; owner, 2026-11-09: "go with 1"): Magic Mark on the surface of a door's or a placeable's model, where
// the caster's eyes meet it, after MO-1 ("the door swings open but the magic mark stays in place") and MO-2 ("on a
// table it gets placed somehow inside the model ... I would assume it would be projected on the surface of the
// model?"). The client finds the surface with the game's own hit test and fixes the mark to the part it hit
// (core/magic_mark_surface.cpp); this is the geometry around it, and the record kept on the object.
//
// 0.96.3 (owner, 2026-11-09: "A" - the mark where you click - "but we should try to find a way to deal with walls
// that have floors behind them so that the wall is marked and not the floor"): the line is the camera's ray through
// the point under the cursor at the click (the game's own pick); the eyes' line is the fallback. A click on a wall
// (the cursor's point isn't the floor the game sends) marks the wall, and the cast's point moves in front of it.
//
// Turns are quaternions in the game's order (w, x, y, z), turning a part's own frame into its parent's (the world's):
// v' = q v q*. The mark's model is a square on its x-z plane, read from its +y side, its top +z.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace rules {

struct Vec3f {
  float x = 0, y = 0, z = 0;
};
struct Quatf {
  float w = 1, x = 0, y = 0, z = 0;
};

Vec3f Add(const Vec3f& a, const Vec3f& b);
Vec3f Sub(const Vec3f& a, const Vec3f& b);
Vec3f Scale(const Vec3f& v, float s);
float Dot(const Vec3f& a, const Vec3f& b);
Vec3f Cross(const Vec3f& a, const Vec3f& b);
float Length(const Vec3f& v);
Vec3f Normalized(const Vec3f& v);  // zero stays zero
Vec3f Rotate(const Quatf& q, const Vec3f& v);
Quatf Conjugate(const Quatf& q);
// The turn that takes the frame's x, y and z axes to these (unit, at right angles, x = y cross z).
Quatf TurnFromAxes(const Vec3f& x, const Vec3f& y, const Vec3f& z);

constexpr float kMarkSurfaceGap = 0.02f;    // metres off the surface
constexpr float kMarkNormalOffset = 0.02f;  // the two extra rays: this far beside the first
constexpr float kMarkSameSurface = 0.25f;   // an extra ray's hit farther along the line than this: another surface
constexpr float kMarkFlatSurface = 0.8f;    // a normal this close to straight up or down: a floor, a top, a ceiling

// The eyes' height above the feet of a creature of NWN size `size` (1 tiny .. 5 huge; otherwise Medium's).
float MarkEyeHeight(int creature_size);

// The point the eyes aim at, in the model's own frame (its box from `box_min` to `box_max`): the box's middle across;
// up and down, just under its top when the eyes are above it (a table's top seen from above), otherwise at the eyes'
// height, kept within the box's middle 80%.
Vec3f MarkAimPoint(const Vec3f& eye, const Vec3f& box_min, const Vec3f& box_max);
// The aim points tried in turn while the lines miss (a candelabra's gaps, an open door's leaf): the first, then
// beside it across the box (35% of its width each way, on x then y), then higher and lower (25% of its height).
std::vector<Vec3f> MarkAimPoints(const Vec3f& eye, const Vec3f& box_min, const Vec3f& box_max);
// The line's end: past the aim point by the box's diagonal, so a gap at the aim point still finds what's behind it.
Vec3f MarkRayEnd(const Vec3f& eye, const Vec3f& aim, const Vec3f& box_min, const Vec3f& box_max);
// The two extra rays' offsets from the line along `direction`: at right angles to it and to each other,
// kMarkNormalOffset long.
void MarkRayOffsets(const Vec3f& direction, Vec3f* u, Vec3f* v);

// The surface's normal at the hit `p0`, facing the eyes: the plane through it and the extra rays' hits `p1`, `p2`;
// the line's reverse when one missed, lies on another surface (kMarkSameSurface), or the three are in a line.
Vec3f MarkSurfaceNormal(const Vec3f& direction, const Vec3f& p0, const std::optional<Vec3f>& p1,
                        const std::optional<Vec3f>& p2);

// The mark's turn on the surface (world): its face (+y) out along `normal`, its top (+z) up the surface - the world's
// up; on a floor, a table's top or a ceiling (kMarkFlatSurface), away from the caster along `direction`.
Quatf MarkTurnOn(const Vec3f& normal, const Vec3f& direction);

// Where the mark sits in the frame of the part it's on: the hit kMarkSurfaceGap out along the normal, turned
// MarkTurnOn - both brought into the part's frame (the part at `part_position`, turned `part_turn`, in the world).
struct MarkFix {
  std::string part;
  Vec3f position;
  Quatf turn;
};
MarkFix MarkFixInPart(const std::string& part, const Vec3f& hit, const Vec3f& normal, const Vec3f& direction,
                      const Vec3f& part_position, const Quatf& part_turn);

// The camera's ray through the point under the cursor at the click (0.96.3): from the camera to that point.
struct MarkRay {
  Vec3f from;
  Vec3f to;
};
constexpr float kMarkClickReach = 4.0f;  // the ray tested this far past the cursor's point (it was on something nearer)
Vec3f MarkClickEnd(const MarkRay& ray);

// The record kept on the object (a local string, saved with the game): the eyes at the cast, the click's ray (0.96.3)
// and, once placed, the part and the mark's place in it - "e <x> <y> <z>", then "|c <fx> <fy> <fz> <tx> <ty> <tz>"
// with a click, then "|f <part> <px> <py> <pz> <qw> <qx> <qy> <qz>" once placed. A part's name has no spaces or
// bars (else the record keeps no fix).
struct MarkRecord {
  Vec3f eye;
  std::optional<MarkRay> click;
  std::optional<MarkFix> fix;
};
std::string MarkRecordText(const MarkRecord& record);
bool ParseMarkRecord(const std::string& text, MarkRecord* record);

// A click on the ground: the cursor's point is the floor the game sends (within kMarkFloorNear across and up or
// down). Otherwise the cursor was on a wall (with floor behind it, which the game sends), a pillar, a ledge.
constexpr float kMarkFloorNear = 0.75f;
bool MarkClickOnFloor(const Vec3f& cursor, const Vec3f& sent);
// The cast's point for a click on a wall: kMarkWallStandOff in front of the cursor's point, toward the camera, at
// the caster's feet - so the caster doesn't walk around to the floor behind the wall.
constexpr float kMarkWallStandOff = 0.6f;
Vec3f MarkCastPointBefore(const Vec3f& cursor, const Vec3f& camera, float feet_z);
// A mark on a wall stands upright when its surface isn't flat (kMarkFlatSurface), facing out along the normal: NWN
// degrees (0 east, 90 north), the way the upright model's face looks when its object faces so.
bool MarkStandsUpright(const Vec3f& normal);
float MarkFacingOf(const Vec3f& normal);
// The cursor's point on the caster's own body (within kMarkBodyReach across of its feet, up to kMarkBodyHeight
// above them): a click at the floor just past it, not a wall.
constexpr float kMarkBodyReach = 0.6f;
constexpr float kMarkBodyHeight = 2.2f;
bool MarkCursorOnCaster(const Vec3f& cursor, const Vec3f& feet);
// 0.96.5 (MO-1c; owner, 2026-11-09: "can be fixed if the player manages to click on the door itself and not on
// the transition surface"): a door model's transition surface, the part "sam" (148 of the game's 237 door models),
// is passed through - the mark goes on the leaf behind it.
bool MarkPassesThrough(const std::string& part);

}  // namespace rules
