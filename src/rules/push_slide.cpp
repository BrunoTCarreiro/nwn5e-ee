#include "rules/push_slide.h"

namespace rules {

double PushSlideStartMs(bool falls, double pushed_ms, double fall_seen_ms, double now_ms, double delay_ms) {
  if (!falls) return pushed_ms;
  if (fall_seen_ms >= 0.0) return fall_seen_ms + delay_ms;
  if (now_ms - pushed_ms >= kFallSlideWaitMs) return pushed_ms + kFallSlideWaitMs;
  return -1.0;
}

}  // namespace rules
