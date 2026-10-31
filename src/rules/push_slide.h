#pragma once

// A push's slide on screen (core/rules_push.cpp; #11, 0.58.8). Owner,
// 2026-10-31, G2-11 (Forceful Blow with Topple): "the animation order should be
// falls prone first and then, while the falling prone animation is playing,
// sliding back. that way it looks like they are flying back. But the speed is
// too great at the moment." S1 (a Shove's push): "The slide isn't smooth, it's
// very fast." The slides kept the speed tuned when the client still snapped
// them (30 m/s: 15 ft in 0.15 s, 5 ft in 0.1 s) and started as the damage
// landed, with Topple's knockdown, so they were over before the fall showed.
//
// Now a push that knocks the creature down (the Push mastery's own fall,
// Topple's knockdown, a killing blow's body) waits for its fall to show and
// slides kFallSlideDelayMs into it, over kFallSlideMs whatever the distance.
// A Shove's push (on its feet: a flinch) is a stumble back over kShoveSlideMs,
// at once. The three are test switches for a session (!test: push_slide_ms,
// push_slide_delay_ms, shove_slide_ms) so they can be tuned in game.

namespace rules {

constexpr int kFallSlideDelayMs = 100;
constexpr int kFallSlideMs = 500;
constexpr int kShoveSlideMs = 350;

// The longest a falling push waits for its fall to show (Topple's knockdown
// lands by a script about when the damage does, a frame or two either side);
// then it slides anyway.
constexpr int kFallSlideWaitMs = 600;

// When a push's slide starts, in ms on the caller's clock; -1 while it still
// waits. `falls`: it waits for its fall - shown at `fall_seen_ms` (-1 while it
// hasn't) - and slides `delay_ms` after it, or at kFallSlideWaitMs after the
// push if no fall has shown by then. Otherwise it slides at once.
double PushSlideStartMs(bool falls, double pushed_ms, double fall_seen_ms, double now_ms, double delay_ms);

}  // namespace rules
