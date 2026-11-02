#include "data/anim_flourish.h"

#include <cmath>

namespace data {

namespace {
constexpr int kPadMs = 75;
}  // namespace

FlourishStep DecideFlourish(const FlourishTick& t) {
  if (!t.alive || !t.wanted || t.now_ms > t.give_up_ms) return FlourishStep::Drop;
  if (t.now_ms < t.not_before_ms) return FlourishStep::Wait;
  if (t.moving) return FlourishStep::Wait;
  if (t.has_round && t.paused) return FlourishStep::Wait;
  if (!t.has_round || (!t.round_started && !t.attacking)) return FlourishStep::PlayNow;
  if (!t.round_started) return FlourishStep::Wait;
  return t.fits ? FlourishStep::PauseAndPlay : FlourishStep::Wait;
}

int FlourishPauseMs(float clip_seconds) {
  const float ms = clip_seconds > 0.0f && std::isfinite(clip_seconds) ? clip_seconds * 1000.0f : 0.0f;
  return static_cast<int>(std::lround(ms)) + kPadMs;
}

}  // namespace data
