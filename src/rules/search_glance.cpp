#include "rules/search_glance.h"

#include <math.h>

namespace rules {

namespace {
int Clamp(int value, int low, int high) { return value < low ? low : (value > high ? high : value); }
double Unit(double roll) { return roll < 0.0 ? 0.0 : (roll >= 1.0 ? 0.999999 : roll); }
float Facing(float degrees) {
  float d = fmodf(degrees, 360.0f);
  if (d < 0.0f) d += 360.0f;
  return d >= 360.0f ? d - 360.0f : d;
}
}  // namespace

GlanceTuning SaneGlance(GlanceTuning tuning) {
  if (tuning.min_degrees > tuning.max_degrees) {
    const int swap = tuning.min_degrees;
    tuning.min_degrees = tuning.max_degrees;
    tuning.max_degrees = swap;
  }
  if (tuning.min_ms > tuning.max_ms) {
    const int swap = tuning.min_ms;
    tuning.min_ms = tuning.max_ms;
    tuning.max_ms = swap;
  }
  tuning.min_degrees = Clamp(tuning.min_degrees, 5, 180);
  tuning.max_degrees = Clamp(tuning.max_degrees, 5, 180);
  tuning.min_ms = Clamp(tuning.min_ms, 300, 10000);
  tuning.max_ms = Clamp(tuning.max_ms, 300, 10000);
  return tuning;
}

float GlanceFacing(const GlanceTuning& tuning, float base, int n, double roll, bool has_toward, float toward,
                   bool left_first) {
  const float line = has_toward ? toward : base;
  if (n <= 0 && has_toward) return Facing(toward);
  const float turn =
      static_cast<float>(tuning.min_degrees + (tuning.max_degrees - tuning.min_degrees) * Unit(roll));
  // The sides alternate: without a toward the first glance is the first side; with one, the first side comes next.
  const int side_index = has_toward ? n - 1 : n;
  const bool left = (side_index % 2 == 0) == left_first;
  return Facing(line + (left ? turn : -turn));
}

int GlanceHoldMs(const GlanceTuning& tuning, double roll) {
  return static_cast<int>(tuning.min_ms + (tuning.max_ms - tuning.min_ms) * Unit(roll) + 0.5);
}

bool GlanceDue(int still_ms, bool hold_over) { return still_ms >= 300 && hold_over; }

}  // namespace rules
