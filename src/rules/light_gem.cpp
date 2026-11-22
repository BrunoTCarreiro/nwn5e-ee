#include "rules/light_gem.h"

#include "rules/hud_clock.h"

namespace rules {

GemLight GemLightOf(int stored_level) {
  switch (stored_level) {
    case 1: return GemLight::kDark;
    case 2: return GemLight::kDim;
    case 3: return GemLight::kBright;
    default: return GemLight::kNone;
  }
}

bool LightGemShown(bool enabled, bool hiding, GemLight light) {
  return enabled && hiding && light != GemLight::kNone;
}

const char* LightGemTextKey(GemLight light) {
  switch (light) {
    case GemLight::kBright: return "light_gem.bright";
    case GemLight::kDim: return "light_gem.dim";
    case GemLight::kDark: return "light_gem.dark";
    case GemLight::kNone: break;
  }
  return "";
}

GemColour LightGemColour(GemLight light) {
  switch (light) {
    case GemLight::kBright: return GemColour{255, 240, 190, 255};
    case GemLight::kDim: return GemColour{150, 140, 105, 255};
    case GemLight::kDark: return GemColour{28, 34, 62, 255};
    case GemLight::kNone: break;
  }
  return GemColour{0, 0, 0, 0};
}

HudBox PlaceLightGem(float screen_w, float screen_h, float scale, float gem_w, float gem_h, float clock_from_right,
                     float clock_from_bottom, float clock_w, float clock_h, float inset, float gap, float overhang) {
  // Right edges together: the clock window's right edge is clock_from_right - clock_w / 2 from the screen's.
  const float from_right = clock_from_right - clock_w / 2.0f + gem_w / 2.0f;
  // The gem card's bottom `gap` over the clock card's top (each card `inset` in from its window).
  const float from_bottom = clock_from_bottom + clock_h - 2.0f * inset + gap;
  return PlaceClock(screen_w, screen_h, scale, gem_w, gem_h, from_right, from_bottom, overhang);
}

}  // namespace rules
