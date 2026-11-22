#include <math.h>

#include "check.h"
#include "rules/search_glance.h"

using namespace rules;

namespace {
bool Near(float a, float b) { return fabsf(a - b) < 0.01f; }
}  // namespace

// #9 (owner, 2026-11-22: "1.A" - glances replace the sweep). Breaks caught: the defaults off the ini's, keys not made
// sane, a hold outside its range or not spread over it.
TEST(SearchGlanceTuning) {
  const GlanceTuning def;
  CHECK_EQ(def.min_degrees == 45 && def.max_degrees == 90 && def.min_ms == 1500 && def.max_ms == 2500, true);
  const GlanceTuning same = SaneGlance(def);
  CHECK_EQ(same.min_degrees == 45 && same.max_degrees == 90 && same.min_ms == 1500 && same.max_ms == 2500, true);
  GlanceTuning raw;
  raw.min_degrees = 300;
  raw.max_degrees = 1;
  raw.min_ms = 50000;
  raw.max_ms = 10;
  const GlanceTuning sane = SaneGlance(raw);
  CHECK_EQ(sane.min_degrees, 5);
  CHECK_EQ(sane.max_degrees, 180);
  CHECK_EQ(sane.min_ms, 300);
  CHECK_EQ(sane.max_ms, 10000);
  CHECK_EQ(GlanceHoldMs(def, 0.0), 1500);
  CHECK_EQ(GlanceHoldMs(def, 0.5), 2000);
  CHECK_EQ(GlanceHoldMs(def, 0.9999999), 2500);
  CHECK_EQ(GlanceHoldMs(def, 3.0), 2500);
  CHECK_EQ(GlanceHoldMs(def, -1.0), 1500);
}

// Breaks caught: the first glance not toward the noise, the sides not alternating, the toward's side glances swung
// round the stand's start instead, a turn outside min .. max, a facing not wrapped to 0-360, a glance before the
// creature settles or before its hold is over.
TEST(SearchGlanceTurnsAndAlternates) {
  const GlanceTuning def;
  // No noise to face: from 100 degrees, left 45-90, then right, then left again.
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 0, 0.0, false, 0.0f, true), 145.0f), true);
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 1, 0.0, false, 0.0f, true), 55.0f), true);
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 2, 0.5, false, 0.0f, true), 167.5f), true);
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 3, 0.9999999, false, 0.0f, true), 10.0f), true);
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 0, 0.0, false, 0.0f, false), 55.0f), true);  // right first
  // Toward the noise first (at 250), then to either side of it.
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 0, 0.7, true, 250.0f, true), 250.0f), true);
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 1, 0.0, true, 250.0f, true), 295.0f), true);
  CHECK_EQ(Near(GlanceFacing(def, 100.0f, 2, 0.0, true, 250.0f, true), 205.0f), true);
  // Wrapped.
  CHECK_EQ(Near(GlanceFacing(def, 340.0f, 0, 0.0, false, 0.0f, true), 25.0f), true);
  CHECK_EQ(Near(GlanceFacing(def, 20.0f, 0, 0.0, false, 0.0f, false), 335.0f), true);
  CHECK_EQ(Near(GlanceFacing(def, 0.0f, 0, 0.0, true, -90.0f, true), 270.0f), true);
  // Every turn within 45-90 of its line.
  int outside = 0;
  for (int n = 0; n < 6; ++n) {
    for (double roll = 0.0; roll < 1.0; roll += 0.05) {
      float off = GlanceFacing(def, 180.0f, n, roll, false, 0.0f, true) - 180.0f;
      off = fabsf(off);
      if (off < 44.99f || off > 90.01f) ++outside;
    }
  }
  CHECK_EQ(outside, 0);
  CHECK_EQ(GlanceDue(299, true), false);  // not settled
  CHECK_EQ(GlanceDue(300, true), true);
  CHECK_EQ(GlanceDue(5000, false), false);  // holding its last glance
}
