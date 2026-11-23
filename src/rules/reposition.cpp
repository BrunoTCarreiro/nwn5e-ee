#include "rules/reposition.h"

#include <math.h>

namespace rules {

std::vector<Sidestep> SidestepSpots(float ax, float ay, float tx, float ty) {
  std::vector<Sidestep> spots;
  float dx = tx - ax, dy = ty - ay;
  const float d = sqrtf(dx * dx + dy * dy);
  if (d < 1e-4f) return spots;
  dx /= d;
  dy /= d;
  const float lx = -dy, ly = dx;  // the attacker's left, square to its line
  for (float metres = 1.5f; metres <= kSidestepMetres + 0.01f; metres += 1.5f) {
    spots.push_back(Sidestep{ax + lx * metres, ay + ly * metres});
    spots.push_back(Sidestep{ax - lx * metres, ay - ly * metres});
  }
  return spots;
}

Reposition ChooseReposition(bool ranged, const std::vector<bool>& clear, int* pick) {
  if (pick) *pick = -1;
  if (!ranged) return Reposition::kWalkRound;
  for (size_t i = 0; i < clear.size(); ++i) {
    if (clear[i]) {
      if (pick) *pick = static_cast<int>(i);
      return Reposition::kSidestep;
    }
  }
  return Reposition::kCloseIn;
}

}  // namespace rules
