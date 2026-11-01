#pragma once

// The HUD clock (core/hud_clock.cpp; #105; owner, 2026-11-01: "there should
// be a clock on top of the compass on the bottom right, we'll have to look
// into how to make it fit the UI but for now try to come up with something
// appropriate, the sun and moon icons are a nice touch"). A small card above
// the compass: a sun (an orange one at dawn and dusk) or a moon, and the game
// time. This file only decides; the engine side reads NWN's clock.

#include <string>

#include "rules/hud_layout.h"

namespace rules {

// The game's minute (0-59). NWN's GetTimeMinute counts the module's real
// minutes within the game hour (0 .. its "minutes per hour" - 1) and
// GetTimeSecond their seconds; a game hour lasts `seconds_per_hour` real
// seconds (HoursToSeconds(1)). 0 or less: the minute as it is.
int ClockGameMinute(int minute, int second, int seconds_per_hour);

// "07:05".
std::string ClockText(int hour, int game_minute);

enum class ClockPhase { kDawn, kDay, kDusk, kNight };
ClockPhase ClockPhaseOf(bool dawn, bool day, bool dusk);

// The card's content (GUI units) and the default spot: its centre this far
// from the screen's right edge, its bottom this far over the bottom edge, both
// scaled by the GUI scale - a first guess at the compass's top (the owner
// tunes it: the clock_x / clock_y test switches).
constexpr float kClockWidth = 66.0f;
constexpr float kClockHeight = 18.0f;
constexpr int kClockFromRight = 64;
constexpr int kClockFromBottom = 132;

// The window's spot: x / y in window pixels, w / h the window's size in GUI
// units (as NUI takes them) - `window_w` x `window_h` GUI units, centred
// `from_right` x scale from the right edge, its bottom `from_bottom` x scale
// over the bottom edge, kept on screen.
HudBox PlaceClock(float screen_w, float screen_h, float scale, float window_w, float window_h, float from_right,
                  float from_bottom);

}  // namespace rules
