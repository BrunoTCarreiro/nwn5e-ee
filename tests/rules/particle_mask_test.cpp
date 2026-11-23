#include <math.h>

#include <vector>

#include "check.h"
#include "rules/cover_objects.h"
#include "rules/particle_mask.h"

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
// A statue 1.2 m wide (model x), 0.6 m deep, 2.8 m tall, at (12, 20) facing 30 degrees north of east - turned, so a
// mixed-up axis shows.
ObjectBox Statue() {
  return BoxOf(Bounds(-0.6f, 0.6f, -0.3f, 0.3f, 0.0f, 2.8f), 12.0f, 20.0f, 0.0f, cosf(0.5236f), sinf(0.5236f), 1.0f);
}
// A box `w` across (its +X), `d` deep, `h` tall at (x, y), facing north.
ObjectBox Crate(float x, float y, float w, float d, float h) {
  return BoxOf(Bounds(-w / 2, w / 2, -d / 2, d / 2, 0.0f, h), x, y, 0.0f, 0.0f, 1.0f, 1.0f);
}
float Num(const std::vector<MaskVec>& u, int slot, int c) {
  const MaskVec& v = u[static_cast<size_t>(slot)];
  return c == 0 ? v.x : c == 1 ? v.y : c == 2 ? v.z : v.w;
}
// A mask at (10, 20, 1.2) reaching `reach`, with a straight wall `metres` east of it (the directions that meet it within
// the reach: metres / cos), none elsewhere.
ParticleMask EastWall(float metres, float reach) {
  ParticleMask m;
  m.origin[0] = 10.0f;
  m.origin[1] = 20.0f;
  m.origin[2] = 1.2f;
  m.reach = reach;
  for (int k = 0; k < kMaskRingDirections; ++k) {
    const float c = cosf(MaskRingAngle(k));
    if (c > 0.01f && metres / c < reach) m.walls[k] = metres / c;
  }
  return m;
}
}  // namespace

// #184: off is the default and exact. Breaks caught: a reach of 0 packed as a mask on (or anything but 16 zeros), a zero
// list (every uniform's default) or a short one taken as a mask, a fragment dropped with the mask off.
TEST(ParticleMaskOffByDefault) {
  ParticleMask off = EastWall(3.0f, 0.0f);
  off.boxes.push_back(Statue());
  const std::vector<MaskVec> none = PackParticleMask(off);
  CHECK_EQ(none.size(), 16u);
  int set = 0;
  for (int s = 0; s < 16; ++s) {
    for (int c = 0; c < 4; ++c) set += Num(none, s, c) != 0.0f;
  }
  CHECK_EQ(set, 0);
  const float behind[3] = {14.0f, 20.0f, 1.0f};
  CHECK_EQ(ParticleMasked(none, behind), false);
  CHECK_EQ(ParticleMasked(std::vector<MaskVec>(16), behind), false);
  CHECK_EQ(ParticleMasked({}, behind), false);
  ParticleMask on = EastWall(3.0f, 12.0f);
  std::vector<MaskVec> short_list = PackParticleMask(on);
  CHECK_EQ(ParticleMasked(short_list, behind), true);
  short_list.pop_back();
  CHECK_EQ(ParticleMasked(short_list, behind), false);
}

// The uniforms' layout, as the shader reads them. Breaks caught: the origin or the reach not in scriptableVec1; a
// direction's nearness in the wrong slot or component, or metres packed instead of 1 / (metres + the margin); a wall
// past the reach packed; a box's numbers out of their back-to-back places (its centre, half sizes, the angle of its +X);
// a fifth box packed (the list longer than 16, or over another's numbers).
TEST(ParticleMaskPacking) {
  ParticleMask m;
  m.origin[0] = 10.0f;
  m.origin[1] = 20.0f;
  m.origin[2] = 1.2f;
  m.reach = 9.0f;
  m.walls[3] = 4.0f;
  m.walls[9] = 2.0f;
  m.walls[31] = 8.75f;
  m.walls[20] = 9.5f;  // past the reach: none
  const ObjectBox statue = Statue();
  const ObjectBox crate = Crate(14.0f, 22.0f, 1.0f, 1.0f, 1.0f);
  m.boxes = {statue, crate, crate, statue, crate};
  const std::vector<MaskVec> u = PackParticleMask(m);
  CHECK_EQ(u.size(), 16u);
  CHECK_EQ(Near(u[0].x, 10.0f) && Near(u[0].y, 20.0f) && Near(u[0].z, 1.2f) && Near(u[0].w, 9.0f), true);
  CHECK_EQ(Near(Num(u, 1, 3), 1.0f / 4.25f), true);   // direction 3: scriptableVec2.w
  CHECK_EQ(Near(Num(u, 3, 1), 1.0f / 2.25f), true);   // direction 9: scriptableVec4.y
  CHECK_EQ(Near(Num(u, 8, 3), 1.0f / 9.0f), true);    // direction 31: scriptableVec9.w
  CHECK_EQ(Num(u, 6, 0) == 0.0f, true);               // direction 20, past the reach
  CHECK_EQ(Num(u, 1, 0) == 0.0f && Num(u, 3, 0) == 0.0f, true);
  // Box 0 at numbers 0-6: scriptableVec10.xyzw, scriptableVec11.xyz.
  CHECK_EQ(Near(Num(u, 9, 0), statue.centre[0]) && Near(Num(u, 9, 1), statue.centre[1]) && Near(Num(u, 9, 2), 1.4f), true);
  CHECK_EQ(Near(Num(u, 9, 3), 0.6f) && Near(Num(u, 10, 0), 0.3f) && Near(Num(u, 10, 1), 1.4f), true);
  CHECK_EQ(Near(Num(u, 10, 2), atan2f(statue.ay, statue.ax)), true);
  // Box 1 at 7-13: scriptableVec11.w (centre x) ... scriptableVec13.y (its angle).
  CHECK_EQ(Near(Num(u, 10, 3), 14.0f) && Near(Num(u, 11, 0), 22.0f) && Near(Num(u, 11, 1), 0.5f), true);
  CHECK_EQ(Near(Num(u, 11, 2), 0.5f) && Near(Num(u, 11, 3), 0.5f) && Near(Num(u, 12, 0), 0.5f), true);
  CHECK_EQ(Near(Num(u, 12, 1), atan2f(crate.ay, crate.ax)), true);
  // Box 3 at 21-27: its angle the last number, scriptableVec16.w.
  CHECK_EQ(Near(Num(u, 14, 1), statue.centre[0]) && Near(Num(u, 15, 3), atan2f(statue.ay, statue.ax)), true);
  CHECK_EQ(Near(Num(u, 15, 0), 0.6f) && Near(Num(u, 15, 1), 0.3f), true);
}

// The walls: a fragment past its direction's wall is dropped, one short of it drawn; between two directions the
// nearness in proportion. Breaks caught: the wall test off, or reading the direction the other way round (clockwise),
// or a quarter turn out; the margin lost (a fragment just at the wall's face dropped); the nearest direction taken
// instead of the two in proportion, or the metres taken in proportion (a doorway's edge reaching straight past the
// wall); the last direction not wrapping to the first; the reach ignored (a fragment past it dropped).
TEST(ParticleMaskWalls) {
  const std::vector<MaskVec> east = PackParticleMask(EastWall(5.0f, 12.0f));
  const float face[3] = {15.1f, 20.0f, 1.0f}, past[3] = {15.6f, 20.0f, 1.0f}, past_aside[3] = {15.6f, 23.0f, 2.0f},
              west[3] = {4.0f, 20.0f, 1.0f}, north[3] = {10.0f, 28.0f, 1.0f}, beyond[3] = {22.5f, 20.0f, 1.0f},
              below_axis[3] = {15.6f, 17.5f, 0.5f};
  CHECK_EQ(ParticleMasked(east, face), false);
  CHECK_EQ(ParticleMasked(east, past), true);
  CHECK_EQ(ParticleMasked(east, past_aside), true);
  CHECK_EQ(ParticleMasked(east, below_axis), true);
  CHECK_EQ(ParticleMasked(east, west), false);
  CHECK_EQ(ParticleMasked(east, north), false);
  CHECK_EQ(ParticleMasked(east, beyond), false);
  // A doorway's edge: direction 2 blocked at 4 m, direction 3 open. Half-way between (25.3 degrees) the nearness is half
  // 1 / 4.25: drawn to 8.5 m.
  ParticleMask door;
  door.origin[0] = 10.0f;
  door.origin[1] = 20.0f;
  door.origin[2] = 1.2f;
  door.reach = 20.0f;
  door.walls[2] = 4.0f;
  const std::vector<MaskVec> u = PackParticleMask(door);
  const float a = (MaskRingAngle(2) + MaskRingAngle(3)) / 2.0f;
  const float short_of[3] = {10.0f + 8.3f * cosf(a), 20.0f + 8.3f * sinf(a), 1.0f};
  const float past_it[3] = {10.0f + 8.7f * cosf(a), 20.0f + 8.7f * sinf(a), 1.0f};
  CHECK_EQ(ParticleMasked(u, short_of), false);
  CHECK_EQ(ParticleMasked(u, past_it), true);
  // The ring wraps: direction 31 blocked at 3 m, direction 0 open - a quarter of the way from direction 31 to 0 the
  // nearness is three-quarters of 1 / 3.25 (drawn to 4.33 m). A crate far north fills the slot after the ring's last.
  ParticleMask wrap = door;
  wrap.walls[2] = 0.0f;
  wrap.walls[31] = 3.0f;
  wrap.boxes = {Crate(10.0f, 30.0f, 1.0f, 1.0f, 1.0f)};
  const std::vector<MaskVec> w = PackParticleMask(wrap);
  const float b = -0.75f * MaskRingAngle(1);
  const float in_reach[3] = {10.0f + 4.2f * cosf(b), 20.0f + 4.2f * sinf(b), 1.0f};
  const float out_reach[3] = {10.0f + 4.45f * cosf(b), 20.0f + 4.45f * sinf(b), 1.0f};
  CHECK_EQ(ParticleMasked(w, in_reach), false);
  CHECK_EQ(ParticleMasked(w, out_reach), true);
}

// The shader's box test is the cover rule's line: within the reach, a fragment is dropped exactly when the line from the
// origin to it meets a box (SegmentHitsBox). Breaks caught: the box's axes swapped or its angle read as the +Y's, its
// half sizes swapped, the origin's height ignored, the line taken past the fragment (one in front of the statue
// dropped), a box after the first ignored, a box with no half size across taken as one.
TEST(ParticleMaskAgreesWithTheCoverLine) {
  ParticleMask m;
  m.origin[0] = 10.0f;
  m.origin[1] = 20.0f;
  m.origin[2] = 1.2f;
  m.reach = 30.0f;
  const ObjectBox statue = Statue();
  const ObjectBox pillar = BoxOf(Bounds(-0.4f, 0.4f, -0.4f, 0.4f, 0.0f, 1.6f), 9.0f, 23.0f, 0.0f, 0.0f, 1.0f, 1.0f);
  const ObjectBox crate = Crate(7.0f, 17.0f, 1.4f, 0.5f, 1.0f);
  m.boxes = {statue};
  const std::vector<MaskVec> one = PackParticleMask(m);
  m.boxes = {statue, pillar, crate};
  const std::vector<MaskVec> three = PackParticleMask(m);
  const float behind[3] = {14.5f, 21.0f, 1.0f}, beside[3] = {12.0f, 18.0f, 1.0f}, before[3] = {11.0f, 20.3f, 1.0f},
              inside[3] = {12.0f, 20.0f, 2.0f}, over[3] = {13.5f, 20.9f, 7.0f}, past_pillar[3] = {8.4f, 24.8f, 1.0f},
              past_crate[3] = {5.0f, 15.0f, 0.2f};
  CHECK_EQ(ParticleMasked(one, behind), true);
  CHECK_EQ(ParticleMasked(one, beside), false);
  CHECK_EQ(ParticleMasked(one, before), false);
  CHECK_EQ(ParticleMasked(one, inside), true);
  CHECK_EQ(ParticleMasked(one, over), false);
  CHECK_EQ(ParticleMasked(one, past_pillar), false);
  CHECK_EQ(ParticleMasked(three, past_pillar), true);
  CHECK_EQ(ParticleMasked(three, past_crate), true);
  // A grid round all three, off the boxes' faces (no ties): the same answer as the cover line everywhere.
  int masked = 0, clear = 0, disagree = 0;
  for (float x = 4.013f; x < 18.0f; x += 0.29f) {
    for (float y = 13.007f; y < 27.0f; y += 0.31f) {
      for (float z = 0.051f; z < 5.0f; z += 0.47f) {
        const float p[3] = {x, y, z};
        const bool line = SegmentHitsBox(m.origin, p, statue) || SegmentHitsBox(m.origin, p, pillar) ||
                          SegmentHitsBox(m.origin, p, crate);
        const bool mask = ParticleMasked(three, p);
        disagree += line != mask;
        masked += mask;
        clear += !mask;
      }
    }
  }
  CHECK_EQ(disagree, 0);
  CHECK_EQ(masked > 300 && clear > 2000, true);
  // Past the reach nothing is dropped, behind a box or not.
  m.reach = 3.0f;
  m.boxes = {statue};
  CHECK_EQ(ParticleMasked(PackParticleMask(m), behind), false);
  // A box with no half size across is none, whatever its other numbers: box 0 crafted on the line to `behind`, 0 across,
  // 1 m deep and tall.
  m.reach = 30.0f;
  m.boxes.clear();
  std::vector<MaskVec> flat = PackParticleMask(m);
  flat[9].x = 12.25f;
  flat[9].y = 20.5f;
  flat[9].z = 1.1f;
  flat[10].x = 1.0f;
  flat[10].y = 1.0f;
  CHECK_EQ(ParticleMasked(flat, behind), false);
}

// The boxes a mask takes. Breaks caught: a box holding the origin taken (the whole effect masked); one past the reach
// taken, or one whose centre is past it but its near side within it left out; more than four taken; the narrowest
// first, or the angle ignored (a far statue before a near crate as wide); the angle taken from the box's centre alone
// (the near side's corners missed) or across its depth.
TEST(ParticleMaskPicksTheWidestBoxes) {
  const float origin[3] = {10.0f, 20.0f, 1.2f};
  // (A Crate's first size is its east-west one: across for the boxes north and south, along the line for those east.)
  const ObjectBox around = Crate(10.0f, 20.0f, 2.0f, 2.0f, 2.0f);
  const ObjectBox near_crate = Crate(13.0f, 20.0f, 0.5f, 1.0f, 1.0f);  // 1 m across, its near side 2.75 m off
  const ObjectBox far_statue = Crate(10.0f, 28.0f, 1.0f, 0.5f, 2.8f);  // as wide, 7.75 m off
  const ObjectBox wide_wall = Crate(10.0f, 14.0f, 4.0f, 0.5f, 2.0f);   // 4 m across, 5.75 m off
  const ObjectBox edge = Crate(21.0f, 20.0f, 4.0f, 3.0f, 1.0f);        // 3 m across; centre 11 m off, near side 9 m
  const ObjectBox gone = Crate(23.0f, 20.0f, 4.0f, 3.0f, 1.0f);        // near side 11 m: past a 10 m reach
  const ObjectBox twin = Crate(7.0f, 20.0f, 0.5f, 1.0f, 1.0f);         // the near crate's mirror
  CHECK_EQ(Near(BoxGroundDistance(origin, edge), 9.0f) && Near(BoxGroundDistance(origin, around), 0.0f), true);
  CHECK_EQ(Near(BoxAngleFrom(origin, near_crate), 2.0f * atanf(0.5f / 2.75f)), true);
  CHECK_EQ(Near(BoxAngleFrom(origin, edge), 2.0f * atanf(1.5f / 9.0f)), true);
  CHECK_EQ(Near(BoxAngleFrom(origin, around), 3.14159265f), true);
  const std::vector<ObjectBox> picked = PickMaskBoxes(origin, 10.0f, {far_statue, gone, around, near_crate, wide_wall, edge});
  CHECK_EQ(picked.size(), 4u);
  if (picked.size() == 4) {
    CHECK_EQ(Near(picked[0].centre[1], 14.0f), true);  // the wide wall: about 38 degrees
    CHECK_EQ(Near(picked[1].centre[0], 13.0f), true);  // the near crate: about 21 degrees
    CHECK_EQ(Near(picked[2].centre[0], 21.0f), true);  // the edge: about 19 degrees
    CHECK_EQ(Near(picked[3].centre[1], 28.0f), true);  // the far statue: about 7 degrees
  }
  const std::vector<ObjectBox> five = PickMaskBoxes(origin, 10.0f, {far_statue, twin, near_crate, wide_wall, edge});
  CHECK_EQ(five.size(), 4u);
  for (const ObjectBox& b : five) CHECK_EQ(Near(b.centre[1], 28.0f), false);  // the far statue left out
}
