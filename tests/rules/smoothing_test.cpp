#include "check.h"
#include "rules/smoothing.h"

#include <cmath>

using namespace rules;

namespace {
bool At(SmoothPoint p, float x, float y) { return std::fabs(p.x - x) < 1e-4f && std::fabs(p.y - y) < 1e-4f; }
}  // namespace

// Breaks caught (0.58.7): the slide off its line, early or late along it, past
// its end, not at its end for a slide of no length of time, before its start
// on a clock read early.
TEST(SmoothSlideAlongTheServersLine) {
  const SmoothPoint from{0.0f, 0.0f}, to{4.0f, -2.0f};
  CHECK_EQ(At(SlidePoint(from, to, 0.0, 150.0), 0.0f, 0.0f), true);
  CHECK_EQ(At(SlidePoint(from, to, 75.0, 150.0), 2.0f, -1.0f), true);
  CHECK_EQ(At(SlidePoint(from, to, 30.0, 150.0), 0.8f, -0.4f), true);
  CHECK_EQ(At(SlidePoint(from, to, 150.0, 150.0), 4.0f, -2.0f), true);
  CHECK_EQ(At(SlidePoint(from, to, 900.0, 150.0), 4.0f, -2.0f), true);
  CHECK_EQ(At(SlidePoint(from, to, 0.0, 0.0), 4.0f, -2.0f), true);
  CHECK_EQ(At(SlidePoint(from, to, -10.0, 150.0), 0.0f, 0.0f), true);
}

// Breaks caught (0.58.7): letting go while it still slides, before the
// client's target caught up, never, or at the wrong distance.
TEST(SmoothHandsBackOnceTheClientCaughtUp) {
  const SmoothPoint to{4.0f, -2.0f};
  CHECK_EQ(SmoothHandBack(to, to, 100.0, 150.0), false);                // still sliding
  CHECK_EQ(SmoothHandBack(to, SmoothPoint{3.0f, -1.5f}, 200.0, 150.0), false);  // its target mid-slide
  CHECK_EQ(SmoothHandBack(to, SmoothPoint{4.04f, -2.0f}, 200.0, 150.0), true);  // caught up
  CHECK_EQ(SmoothHandBack(to, SmoothPoint{4.06f, -2.0f}, 200.0, 150.0), false); // not quite
  CHECK_EQ(SmoothHandBack(to, SmoothPoint{3.0f, -1.5f}, 1149.0, 150.0), false);
  CHECK_EQ(SmoothHandBack(to, SmoothPoint{3.0f, -1.5f}, 1150.0, 150.0), true);  // a second past: let go anyway
}
