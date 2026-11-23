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
}  // namespace

// #184's spike: off is the default and exact. Breaks caught: no box packed as a mask on, a zero vector (every uniform's
// default) or an empty or short list taken as a mask, a fragment dropped with the mask off.
TEST(ParticleMaskOffByDefault) {
  const float origin[3] = {10.0f, 20.0f, 1.2f};
  const std::vector<MaskVec> none = PackParticleMask(origin, {});
  CHECK_EQ(none.size(), 1u);
  CHECK_EQ(none[0].w == 0.0f, true);
  const float behind[3] = {14.0f, 20.0f, 1.0f};
  CHECK_EQ(ParticleMasked(none, behind), false);
  std::vector<MaskVec> zeros(15);
  CHECK_EQ(ParticleMasked(zeros, behind), false);
  CHECK_EQ(ParticleMasked({}, behind), false);
  std::vector<MaskVec> short_list = PackParticleMask(origin, {Statue()});
  short_list.pop_back();
  CHECK_EQ(ParticleMasked(short_list, behind), false);
}

// The uniforms' layout, as the shader reads them. Breaks caught: the origin or the count not in scriptableVec1, a box's
// centre, half sizes or +X in the wrong place, more than seven boxes packed (scriptableVec16 and past), a box holding
// the origin kept (the whole effect masked) or one after it lost.
TEST(ParticleMaskPacking) {
  const float origin[3] = {10.0f, 20.0f, 1.2f};
  const ObjectBox statue = Statue();
  const std::vector<MaskVec> one = PackParticleMask(origin, {statue});
  CHECK_EQ(one.size(), 3u);
  CHECK_EQ(Near(one[0].x, 10.0f) && Near(one[0].y, 20.0f) && Near(one[0].z, 1.2f) && Near(one[0].w, 1.0f), true);
  CHECK_EQ(Near(one[1].x, statue.centre[0]) && Near(one[1].y, statue.centre[1]) && Near(one[1].z, 1.4f) &&
               Near(one[1].w, statue.ax),
           true);
  CHECK_EQ(Near(one[2].x, 0.6f) && Near(one[2].y, 0.3f) && Near(one[2].z, 1.4f) && Near(one[2].w, statue.ay), true);
  const std::vector<ObjectBox> nine(9, statue);
  const std::vector<MaskVec> full = PackParticleMask(origin, nine);
  CHECK_EQ(full.size(), static_cast<size_t>(1 + 2 * kParticleMaskMaxBoxes));
  CHECK_EQ(Near(full[0].w, 7.0f), true);
  // A box round the origin (a table the caster stands in) is left out; the statue after it is kept.
  const ObjectBox around = BoxOf(Bounds(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 2.0f), 10.0f, 20.0f, 0.0f, 0.0f, 1.0f, 1.0f);
  const std::vector<MaskVec> kept = PackParticleMask(origin, {around, statue});
  CHECK_EQ(kept.size(), 3u);
  CHECK_EQ(Near(kept[0].w, 1.0f) && Near(kept[1].x, statue.centre[0]), true);
}

// The shader's test is the cover rule's line: a fragment is dropped exactly when the line from the origin to it meets a
// box (SegmentHitsBox). Breaks caught: the box's axes swapped (its +Y packed as +X), its half sizes swapped, the
// origin's height ignored, the line taken past the fragment (one in front of the statue dropped), a second box ignored.
// (A +Y mirrored is no break: a centred box is the same box either way.)
TEST(ParticleMaskAgreesWithTheCoverLine) {
  const float origin[3] = {10.0f, 20.0f, 1.2f};
  const ObjectBox statue = Statue();
  const ObjectBox pillar = BoxOf(Bounds(-0.4f, 0.4f, -0.4f, 0.4f, 0.0f, 1.6f), 9.0f, 23.0f, 0.0f, 0.0f, 1.0f, 1.0f);
  const std::vector<MaskVec> one = PackParticleMask(origin, {statue});
  const std::vector<MaskVec> two = PackParticleMask(origin, {statue, pillar});
  const float behind[3] = {14.5f, 21.0f, 1.0f}, beside[3] = {12.0f, 18.0f, 1.0f}, before[3] = {11.0f, 20.3f, 1.0f},
              inside[3] = {12.0f, 20.0f, 2.0f}, over[3] = {13.5f, 20.9f, 7.0f}, past_pillar[3] = {8.4f, 24.8f, 1.0f};
  CHECK_EQ(ParticleMasked(one, behind), true);
  CHECK_EQ(ParticleMasked(one, beside), false);
  CHECK_EQ(ParticleMasked(one, before), false);
  CHECK_EQ(ParticleMasked(one, inside), true);
  CHECK_EQ(ParticleMasked(one, over), false);
  CHECK_EQ(ParticleMasked(one, past_pillar), false);
  CHECK_EQ(ParticleMasked(two, past_pillar), true);
  // A grid round both, off the boxes' faces (no ties): the same answer as the cover line everywhere.
  int masked = 0, clear = 0, disagree = 0;
  for (float x = 6.013f; x < 18.0f; x += 0.29f) {
    for (float y = 14.007f; y < 27.0f; y += 0.31f) {
      for (float z = 0.051f; z < 5.0f; z += 0.47f) {
        const float p[3] = {x, y, z};
        const bool line = SegmentHitsBox(origin, p, statue) || SegmentHitsBox(origin, p, pillar);
        const bool mask = ParticleMasked(two, p);
        disagree += line != mask;
        masked += mask;
        clear += !mask;
      }
    }
  }
  CHECK_EQ(disagree, 0);
  CHECK_EQ(masked > 200 && clear > 2000, true);
}
