#include "rules/round_bar.h"

#include <algorithm>

#include "rules/queue_strip.h"

namespace rules {

float RoundFill(long long timer_ms, long long length_ms) {
  if (length_ms <= 0) return 0.0f;
  const float fill = static_cast<float>(timer_ms) / static_cast<float>(length_ms);
  return std::clamp(fill, 0.0f, 1.0f);
}

std::vector<float> RoundSwingMarks(const std::vector<long long>& due_ms, long long length_ms) {
  std::vector<float> marks;
  for (long long due : due_ms) marks.push_back(RoundFill(due, length_ms));
  std::sort(marks.begin(), marks.end());
  if (marks.size() > static_cast<size_t>(kRoundBarMaxSwings)) marks.resize(kRoundBarMaxSwings);
  return marks;
}

RoundKind RoundKindOf(bool running, bool cast_round) {
  if (!running) return RoundKind::kNone;
  return cast_round ? RoundKind::kCast : RoundKind::kAttack;
}

RoundPip ActionPip(bool round_running) { return round_running ? RoundPip::kSpent : RoundPip::kFree; }

RoundPip BonusPip(bool free, bool waiting) {
  if (waiting) return RoundPip::kWaiting;
  return free ? RoundPip::kFree : RoundPip::kSpent;
}

RoundPip ReactionPip(bool available) { return available ? RoundPip::kFree : RoundPip::kSpent; }

float RoundBarTop(bool strip_on, float scale) {
  const float s = scale > 0.0f ? scale : 1.0f;
  return strip_on ? kQueueTileTop + kQueueTileHeight * s : 0.0f;
}

}  // namespace rules
