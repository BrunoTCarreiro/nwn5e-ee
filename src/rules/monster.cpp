#include "rules/monster.h"

#include <cmath>

namespace rules {

int ExpectedMonsterArmorClass(double challenge_rating) {
  const int cr = challenge_rating < 1 ? 0 : static_cast<int>(std::floor(challenge_rating));
  if (cr <= 3) return 13;
  if (cr == 4) return 14;
  if (cr <= 7) return 15;
  if (cr <= 9) return 16;
  if (cr <= 12) return 17;
  if (cr <= 16) return 18;
  return 19;
}

}  // namespace rules
