#include "rules/hud_clock.h"

#include <stdio.h>

namespace rules {

int ClockGameMinute(int minute, int second, int seconds_per_hour) {
  if (seconds_per_hour <= 0) return minute < 0 ? 0 : minute > 59 ? 59 : minute;
  const long long real = static_cast<long long>(minute) * 60 + second;
  const long long game = real * 60 / seconds_per_hour;
  return game < 0 ? 0 : game > 59 ? 59 : static_cast<int>(game);
}

std::string ClockText(int hour, int game_minute) {
  char text[16];
  sprintf_s(text, "%02d:%02d", hour < 0 ? 0 : hour % 24, game_minute < 0 ? 0 : game_minute % 60);
  return text;
}

ClockPhase ClockPhaseOf(bool dawn, bool day, bool dusk) {
  if (dawn) return ClockPhase::kDawn;
  if (dusk) return ClockPhase::kDusk;
  return day ? ClockPhase::kDay : ClockPhase::kNight;
}

HudBox PlaceClock(float screen_w, float screen_h, float scale, float window_w, float window_h, float from_right,
                  float from_bottom) {
  const float s = scale > 0.0f ? scale : 1.0f;
  float x = screen_w - from_right * s - window_w * s / 2.0f;
  float y = screen_h - from_bottom * s - window_h * s;
  if (x > screen_w - window_w * s) x = screen_w - window_w * s;
  if (y > screen_h - window_h * s) y = screen_h - window_h * s;
  if (x < 0.0f) x = 0.0f;
  if (y < 0.0f) y = 0.0f;
  return HudBox{x, y, window_w, window_h};
}

}  // namespace rules
