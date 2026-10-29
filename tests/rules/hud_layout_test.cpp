#include "check.h"
#include "rules/class_features.h"
#include "rules/hud_layout.h"

#include <cmath>
#include <string>

using namespace rules;

namespace {
HudPool Focus(int left, int total) {
  HudPool pool;
  pool.name = "Focus";
  pool.left = left;
  pool.total = total;
  pool.flags.push_back({"Metabolism", true});
  return pool;
}
bool Inside(const HudBox& inner, float width, float height) {
  return inner.x >= 0.0f && inner.y >= 0.0f && inner.Right() <= width && inner.Bottom() <= height;
}
}  // namespace

// The resource panel (0.57.0). Breaks caught: more pips than points or past
// 40, a row longer than 10, rows not stacked, the count over the pips, a flag
// over the count, the content outside the panel's size, a second pool over
// the first, a count reading total/left, the shape changing with the points
// left (a rebuild every spend), a fade that overshoots or never arrives.
TEST(HudLayoutPips) {
  const HudLayout one = LayoutHud({Focus(14, 18)});
  CHECK_EQ(one.pools.size(), 1u);
  const HudPoolLayout& p = one.pools[0];
  CHECK_EQ(p.pips.size(), 18u);
  CHECK_EQ(p.pips[9].y == p.pips[0].y, true);                      // ten across
  CHECK_EQ(p.pips[10].y == p.pips[0].y + kHudPipStep, true);       // then the next row
  CHECK_EQ(p.pips[10].x == p.pips[0].x, true);
  CHECK_EQ(p.pips[1].x - p.pips[0].x == kHudPipStep, true);
  CHECK_EQ(p.pips[0].x >= p.name.Right(), true);
  CHECK_EQ(p.count.x >= p.pips[9].Right(), true);                  // after the longest row
  CHECK_EQ(p.flag_names.size() == 1 && p.flag_dots.size() == 1, true);
  CHECK_EQ(p.flag_names[0].x >= p.count.Right(), true);
  CHECK_EQ(p.flag_dots[0].x >= p.flag_names[0].Right(), true);
  CHECK_EQ(p.block.h == kHudLine + kHudPipStep, true);             // two rows
  for (const HudBox& b : p.pips) CHECK_EQ(Inside(b, one.width, one.height), true);
  CHECK_EQ(Inside(p.flag_dots[0], one.width, one.height), true);
  CHECK_EQ(Inside(p.count, one.width, one.height), true);
  const HudLayout two = LayoutHud({Focus(2, 2), Focus(0, 5)});
  CHECK_EQ(two.pools[0].pips.size(), 2u);
  CHECK_EQ(two.pools[0].count.x < p.count.x, true);                // two pips: a shorter block
  CHECK_EQ(two.pools[1].block.y >= two.pools[0].block.Bottom(), true);
  CHECK_EQ(Inside(two.pools[1].block, two.width, two.height), true);
  const HudLayout stacked = LayoutHud({Focus(14, 18), Focus(1, 2)});  // two rows, then one under them
  CHECK_EQ(stacked.pools[1].block.y >= stacked.pools[0].pips[17].Bottom(), true);
  CHECK_EQ(Inside(stacked.pools[1].count, stacked.width, stacked.height), true);
  CHECK_EQ(LayoutHud({Focus(50, 60)}).pools[0].pips.empty(), true);  // past 40: the count alone
  CHECK_EQ(LayoutHud({Focus(20, 20)}).pools[0].block.h == kHudLine + 1 * kHudPipStep, true);
  CHECK_EQ(LayoutHud({Focus(21, 21)}).pools[0].block.h == kHudLine + 2 * kHudPipStep, true);
  CHECK_EQ(LayoutHud({}).width == 0.0f && LayoutHud({}).height == 0.0f, true);
  CHECK_EQ(HudCount(Focus(14, 18)) == "14/18", true);
  CHECK_EQ(HudShape({Focus(14, 18)}) == HudShape({Focus(3, 18)}), true);
  CHECK_EQ(HudShape({Focus(14, 18)}) == HudShape({Focus(14, 19)}), false);
  HudPool lit = Focus(1, 18);
  lit.flags[0].ready = false;
  CHECK_EQ(HudShape({lit}) == HudShape({Focus(1, 18)}), true);
  CHECK_EQ(std::fabs(HudFade(1.0f, 0.0f, 0.1f, 0.4f) - 0.75f) < 1e-5f, true);
  CHECK_EQ(HudFade(0.1f, 0.0f, 0.1f, 0.4f) == 0.0f, true);       // no overshoot
  CHECK_EQ(HudFade(0.9f, 1.0f, 0.5f, 0.4f) == 1.0f, true);
  CHECK_EQ(HudFade(0.5f, 0.5f, 0.1f, 0.4f) == 0.5f, true);
}

// The Focus tooltip, in paragraphs (0.57.1). Breaks caught: the points or DC
// missing, Stunning Strike's cost before 5, Superior Defense's before 18,
// Uncanny Metabolism shown ready once used, the heal's die not the level's
// Martial Arts die, the paragraphs run together, a cost not on its own line.
TEST(HudFocusTooltip) {
  const std::string five = FocusTooltip(5, 3, 13, true);
  CHECK_EQ(five.find("3 of 5 points") != std::string::npos, true);
  CHECK_EQ(five.find("DC is 13.") != std::string::npos, true);
  CHECK_EQ(five.find("\nStunning Strike: 1") != std::string::npos, true);
  CHECK_EQ(five.find("\n\nCosts\nFlurry of Blows: 1 a round in its mode\nPatient Defense: 1\n") != std::string::npos,
           true);
  CHECK_EQ(five.find("Superior Defense") == std::string::npos, true);
  CHECK_EQ(five.find("Uncanny Metabolism: ready") != std::string::npos, true);
  CHECK_EQ(five.find("d8 + 5") != std::string::npos, true);
  CHECK_EQ(FocusTooltip(4, 4, 12, true).find("Stunning Strike") == std::string::npos, true);
  const std::string eighteen = FocusTooltip(18, 0, 17, false);
  CHECK_EQ(eighteen.find("\nSuperior Defense: 3") != std::string::npos, true);
  int paragraphs = 1;
  for (size_t at = eighteen.find("\n\n"); at != std::string::npos; at = eighteen.find("\n\n", at + 2)) ++paragraphs;
  CHECK_EQ(paragraphs, 4);  // the points, the costs, Uncanny Metabolism, Perfect Focus
  CHECK_EQ(eighteen.find("\n\nPerfect Focus\n") != std::string::npos, true);
  CHECK_EQ(eighteen.find("used - back on a Long Rest") != std::string::npos, true);
  CHECK_EQ(eighteen.find("Perfect Focus") != std::string::npos, true);
  CHECK_EQ(eighteen.find("d12 + 18") != std::string::npos, true);
}
