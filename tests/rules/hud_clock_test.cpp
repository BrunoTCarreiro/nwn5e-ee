#include "check.h"
#include "rules/hud_clock.h"

#include <cmath>

using namespace rules;

// #105. Breaks caught: the game minute read as the real one, its rounding up,
// no fallback for an unknown hour length, the hour or minute not two digits,
// dawn / dusk shown as day or night, the clock off its spot or off screen, its
// pad not allowed past the edge (0.59.9: flush to the right edge).
TEST(HudClockTime) {
  CHECK_EQ(ClockGameMinute(0, 0, 120), 0);    // 2 real minutes a game hour
  CHECK_EQ(ClockGameMinute(0, 30, 120), 15);
  CHECK_EQ(ClockGameMinute(0, 31, 120), 15);  // 15.5 -> 15
  CHECK_EQ(ClockGameMinute(1, 0, 120), 30);
  CHECK_EQ(ClockGameMinute(1, 59, 120), 59);  // 59.5 -> 59, never 60
  CHECK_EQ(ClockGameMinute(30, 0, 3600), 30);  // an hour of real time
  CHECK_EQ(ClockGameMinute(7, 0, 0), 7);
  CHECK_EQ(ClockGameMinute(90, 0, 0), 59);
  CHECK_EQ(ClockText(7, 5) == "07:05", true);
  CHECK_EQ(ClockText(23, 59) == "23:59", true);
  CHECK_EQ(ClockText(0, 0) == "00:00", true);
  CHECK_EQ(ClockPhaseOf(true, false, false) == ClockPhase::kDawn, true);
  CHECK_EQ(ClockPhaseOf(false, true, false) == ClockPhase::kDay, true);
  CHECK_EQ(ClockPhaseOf(false, false, true) == ClockPhase::kDusk, true);
  CHECK_EQ(ClockPhaseOf(false, false, false) == ClockPhase::kNight, true);
}

TEST(HudClockPlacedOverTheCompass) {
  const HudBox a = PlaceClock(1700.0f, 900.0f, 1.0f, 82.0f, 34.0f, 64.0f, 132.0f, 0.0f);
  CHECK_EQ(std::fabs(a.x - 1595.0f) < 0.01f, true);
  CHECK_EQ(std::fabs(a.y - 734.0f) < 0.01f, true);
  CHECK_EQ(std::fabs(a.w - 82.0f) < 0.01f && std::fabs(a.h - 34.0f) < 0.01f, true);  // GUI units
  const HudBox b = PlaceClock(1700.0f, 900.0f, 2.0f, 82.0f, 34.0f, 64.0f, 132.0f, 0.0f);
  CHECK_EQ(std::fabs(b.x - 1490.0f) < 0.01f, true);
  CHECK_EQ(std::fabs(b.y - 568.0f) < 0.01f, true);
  const HudBox c = PlaceClock(1700.0f, 900.0f, 1.0f, 82.0f, 34.0f, 0.0f, -50.0f, 0.0f);  // kept on screen
  CHECK_EQ(std::fabs(c.x - 1618.0f) < 0.01f && std::fabs(c.y - 866.0f) < 0.01f, true);
  // The pad (4) may hang past the edges: the card flush with them.
  const HudBox d = PlaceClock(1700.0f, 900.0f, 1.0f, 82.0f, 34.0f, 0.0f, -50.0f, 4.0f);
  CHECK_EQ(std::fabs(d.x - 1622.0f) < 0.01f && std::fabs(d.y - 870.0f) < 0.01f, true);
  // The built-in spot (0.59.16): the window (66 x 18 + the window's and the group's pads, 82 x 34) with its
  // pad of 4 past the right edge - the card on the edge - its bottom 125 over the bottom edge (the owner's fit);
  // kept on screen (no pad past), the window flush.
  const HudBox e = PlaceClock(1700.0f, 900.0f, 1.0f, 82.0f, 34.0f, static_cast<float>(kClockFromRight),
                              static_cast<float>(kClockFromBottom), 4.0f);
  CHECK_EQ(std::fabs(e.x + 82.0f - 4.0f - 1700.0f) < 0.01f, true);
  CHECK_EQ(kClockFromRight * 2 + 8, 82);  // the card flush by itself, not by the clamp
  CHECK_EQ(std::fabs(e.y + 34.0f - (900.0f - 125.0f)) < 0.01f, true);
  const HudBox f = PlaceClock(1700.0f, 900.0f, 1.0f, 82.0f, 34.0f, static_cast<float>(kClockFromRight),
                              static_cast<float>(kClockFromBottom), 0.0f);
  CHECK_EQ(std::fabs(f.x + 82.0f - 1700.0f) < 0.01f, true);
}
