#include "rules/smoothing.h"

namespace rules {

SmoothPoint SlidePoint(SmoothPoint from, SmoothPoint to, double elapsed_ms, double duration_ms) {
  if (!(duration_ms > 0.0) || elapsed_ms >= duration_ms) return to;
  if (!(elapsed_ms > 0.0)) return from;
  const float t = static_cast<float>(elapsed_ms / duration_ms);
  return SmoothPoint{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t};
}

bool SmoothHandBack(SmoothPoint to, SmoothPoint network_target, double elapsed_ms, double duration_ms) {
  if (elapsed_ms < duration_ms) return false;
  if (elapsed_ms >= duration_ms + kSmoothHandBackAfterMs) return true;
  const float dx = network_target.x - to.x, dy = network_target.y - to.y;
  return dx * dx + dy * dy <= kSmoothHandBackMetres * kSmoothHandBackMetres;
}

}  // namespace rules
