#include "check.h"
#include "rules/push_slide.h"

using namespace rules;

// Breaks caught (0.58.8): a falling push sliding before its fall shows, with
// no delay into the fall, never giving up on a fall that doesn't show (or
// giving up early), the delay added to a slide that gave up waiting, a Shove's
// push waiting for a fall it doesn't have.
TEST(PushSlidesWhileItFalls) {
  // A Shove's push: at once.
  CHECK_EQ(PushSlideStartMs(false, 1000.0, -1.0, 1000.0, 100.0), 1000.0);
  // A falling push waits for its fall ...
  CHECK_EQ(PushSlideStartMs(true, 1000.0, -1.0, 1000.0, 100.0), -1.0);
  CHECK_EQ(PushSlideStartMs(true, 1000.0, -1.0, 1599.0, 100.0), -1.0);
  // ... then slides the delay into it.
  CHECK_EQ(PushSlideStartMs(true, 1000.0, 1020.0, 1030.0, 100.0), 1120.0);
  CHECK_EQ(PushSlideStartMs(true, 1000.0, 1000.0, 1000.0, 0.0), 1000.0);  // no delay set
  // No fall by kFallSlideWaitMs: it slides anyway.
  CHECK_EQ(PushSlideStartMs(true, 1000.0, -1.0, 1600.0, 100.0), 1600.0);
  CHECK_EQ(kFallSlideWaitMs, 600);
  // The defaults: a slide inside the push's 1.2 s fall, a Shove's stumble
  // slower than 0.58.7's 0.1 s.
  CHECK_EQ(kFallSlideDelayMs + kFallSlideMs <= 1200, true);
  CHECK_EQ(kShoveSlideMs >= 300, true);
}
