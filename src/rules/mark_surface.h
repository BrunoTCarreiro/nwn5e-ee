// #39 (0.96.2; owner, 2026-11-09: "go with 1"): Magic Mark on the surface of a door's or a placeable's model, where
// the caster's eyes meet it, after MO-1 ("the door swings open but the magic mark stays in place") and MO-2 ("on a
// table it gets placed somehow inside the model ... I would assume it would be projected on the surface of the
// model?"). The client finds the surface with the game's own hit test and fixes the mark to the part it hit
// (core/magic_mark_surface.cpp); this is the geometry around it, and the record kept on the object.
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

// The record kept on the object (a local string, saved with the game): the eyes at the cast and, once placed, the
// part and the mark's place in it - "e <x> <y> <z>" or "e <x> <y> <z>|f <part> <px> <py> <pz> <qw> <qx> <qy> <qz>".
// A part's name has no spaces or bars (else the record keeps the eyes only).
struct MarkRecord {
  Vec3f eye;
  std::optional<MarkFix> fix;
};
std::string MarkRecordText(const MarkRecord& record);
bool ParseMarkRecord(const std::string& text, MarkRecord* record);

}  // namespace rules
