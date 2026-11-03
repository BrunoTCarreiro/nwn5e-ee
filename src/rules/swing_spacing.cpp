#include "rules/swing_spacing.h"

#include <algorithm>

namespace rules {

int EngineRoundSwingTotalMs(int attacks) {
  const int n = std::max(attacks, 1);
  return std::min(750 * n + 500, 3000);
}

int EngineSwingMs(int attacks) {
  const int n = std::max(attacks, 1);
  float divisor = 1.0f;
  if (n == 2) divisor = 2.0f;
  if (n >= 3) divisor = static_cast<float>(n - 3) * 0.5f + 3.0f;
  return static_cast<int>(static_cast<float>(EngineRoundSwingTotalMs(n)) / divisor);
}

SwingLayout OneSwingPerAttack(int attacks, int first_due, int length_now, int swing_ms) {
  SwingLayout layout;
  if (attacks < 2 || swing_ms <= 0 || first_due < 0) return layout;
  const int length = length_now + EngineRoundSwingTotalMs(attacks) - attacks * swing_ms;
  const int room = length - first_due;
  if (room < attacks) return layout;
  layout.round_length = length;
  for (int i = 0; i < attacks; ++i) layout.dues.push_back(first_due + room * i / attacks);
  return layout;
}

SwingLayout AddedSwings(int pending, int added, int now, int length) {
  SwingLayout layout;
  if (added <= 0 || pending < 0 || now < 0) return layout;
  const int swings = pending + added;
  const int room = std::max(length - now, swings * kMinSwingGapMs);
  layout.round_length = now + room;
  for (int i = 0; i < swings; ++i) layout.dues.push_back(now + room * i / swings);
  return layout;
}

}  // namespace rules
