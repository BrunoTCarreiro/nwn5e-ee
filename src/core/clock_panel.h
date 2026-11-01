#pragma once

// The HUD clock (rules/hud_clock.h; #105): a card above the compass with a sun
// or moon and the game time, `!clock` on / off.

// Its spot, GUI units (rules::kClockFromRight / kClockFromBottom built in) -
// test switches for a session (!test: clock_x, clock_y) while it's fitted.
struct ClockOffsets {
  int from_right;
  int from_bottom;
};
ClockOffsets& ClockOffsetsForTests();

void InstallHudClock();
