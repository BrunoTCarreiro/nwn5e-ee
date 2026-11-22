#include <string>

#include "check.h"
#include "rules/hud_clock.h"
#include "rules/light_gem.h"

using namespace rules;

// #9 a. Breaks caught: a stored level read one off, the gem shown out of stealth mode, with its key off or before the
// light is worked out, the words of another level, two levels the same colour.
TEST(LightGemShowsTheLightWhileHiding) {
  CHECK_EQ(GemLightOf(0) == GemLight::kNone, true);
  CHECK_EQ(GemLightOf(1) == GemLight::kDark, true);
  CHECK_EQ(GemLightOf(2) == GemLight::kDim, true);
  CHECK_EQ(GemLightOf(3) == GemLight::kBright, true);
  CHECK_EQ(GemLightOf(4) == GemLight::kNone, true);
  CHECK_EQ(GemLightOf(-1) == GemLight::kNone, true);
  CHECK_EQ(LightGemShown(true, true, GemLight::kDim), true);
  CHECK_EQ(LightGemShown(true, true, GemLight::kDark), true);
  CHECK_EQ(LightGemShown(true, false, GemLight::kBright), false);  // out of stealth mode
  CHECK_EQ(LightGemShown(false, true, GemLight::kBright), false);  // light_gem=0
  CHECK_EQ(LightGemShown(true, true, GemLight::kNone), false);     // not worked out yet
  CHECK_EQ(std::string(LightGemTextKey(GemLight::kBright)) == "light_gem.bright", true);
  CHECK_EQ(std::string(LightGemTextKey(GemLight::kDim)) == "light_gem.dim", true);
  CHECK_EQ(std::string(LightGemTextKey(GemLight::kDark)) == "light_gem.dark", true);
  CHECK_EQ(std::string(LightGemTextKey(GemLight::kNone)).empty(), true);
  const GemColour bright = LightGemColour(GemLight::kBright), dim = LightGemColour(GemLight::kDim),
                  dark = LightGemColour(GemLight::kDark);
  const auto sum = [](const GemColour& c) { return c.r + c.g + c.b; };
  CHECK_EQ(sum(bright) > sum(dim) && sum(dim) > sum(dark), true);  // brighter for more light
  CHECK_EQ(bright.a == 255 && dim.a == 255 && dark.a == 255, true);
  CHECK_EQ(LightGemColour(GemLight::kNone).a, 0);
}

// Breaks caught: the card not flush with the clock's right edge, not over the clock, overlapping it or off by the
// GUI scale.
TEST(LightGemSitsOverTheClock) {
  // The clock's window as clock_panel.cpp places it: 82 x 34, its centre 37 from the right, its bottom 125 up.
  const float clock_w = kClockWidth + 16.0f, clock_h = kClockHeight + 16.0f;
  const float gem_w = kLightGemWidth + 16.0f, gem_h = kLightGemHeight + 16.0f;
  for (float s : {1.0f, 2.0f}) {
    const HudBox clock = PlaceClock(1700.0f, 900.0f, s, clock_w, clock_h, static_cast<float>(kClockFromRight),
                                    static_cast<float>(kClockFromBottom), 4.0f);
    const HudBox gem = PlaceLightGem(1700.0f, 900.0f, s, gem_w, gem_h, static_cast<float>(kClockFromRight),
                                     static_cast<float>(kClockFromBottom), clock_w, clock_h, 4.0f, 4.0f, 4.0f);
    CHECK_EQ(gem.x + gem_w * s == clock.x + clock_w * s, true);         // right edges together
    CHECK_EQ(gem.y + gem_h * s - 4.0f * s, clock.y + 4.0f * s - 4.0f * s);  // card bottom 4 over the clock card's top
    CHECK_EQ(gem.y < clock.y, true);
  }
  // The clock moved left and up (!clock right 200 / bottom 300): the gem follows it, flush with its right edge.
  const HudBox moved_clock = PlaceClock(1700.0f, 900.0f, 1.0f, clock_w, clock_h, 200.0f, 300.0f, 4.0f);
  const HudBox moved = PlaceLightGem(1700.0f, 900.0f, 1.0f, gem_w, gem_h, 200.0f, 300.0f, clock_w, clock_h, 4.0f,
                                     4.0f, 4.0f);
  CHECK_EQ(moved.x + gem_w, moved_clock.x + clock_w);
  CHECK_EQ(moved.y + gem_h - 4.0f, moved_clock.y);
  const HudBox one = PlaceLightGem(1700.0f, 900.0f, 1.0f, gem_w, gem_h, 37.0f, 125.0f, clock_w, clock_h, 4.0f, 4.0f,
                                   4.0f);
  CHECK_EQ(one.x, 1584.0f);
  CHECK_EQ(one.y, 711.0f);
  CHECK_EQ(one.w, gem_w);
}
