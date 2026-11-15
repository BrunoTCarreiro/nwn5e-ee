#include "rules/cover.h"

#include <cmath>

namespace rules {

int CoverArmorClassBonus(Cover cover) {
  switch (cover) {
    case Cover::kHalf:
      return 2;
    case Cover::kThreeQuarters:
      return 5;
    default:
      return 0;
  }
}

Cover MoreCover(Cover a, Cover b) { return static_cast<int>(a) >= static_cast<int>(b) ? a : b; }

float CreatureBodyRadius(int nwn_size) {
  if (nwn_size <= 1) return 0.2f;
  if (nwn_size <= 3) return 0.4f;
  if (nwn_size == 4) return 0.8f;
  return 1.2f;
}

bool SizeGivesCover(int nwn_size) { return nwn_size >= 2; }

bool CreatureInTheWay(float ax, float ay, float tx, float ty, float bx, float by, float radius, float attacker_radius,
                      float target_radius) {
  const float dx = tx - ax;
  const float dy = ty - ay;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length < 0.01f) return false;
  const float along = ((bx - ax) * dx + (by - ay) * dy) / length;  // metres from the attacker, along the line
  if (along <= attacker_radius || along >= length - target_radius) return false;
  const float across = std::fabs((bx - ax) * dy - (by - ay) * dx) / length;  // metres off the line
  return across <= radius;
}

}  // namespace rules
