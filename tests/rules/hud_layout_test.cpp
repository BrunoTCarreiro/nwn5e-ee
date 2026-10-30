#include "check.h"
#include "rules/class_features.h"
#include "rules/hud_layout.h"
#include "rules/queue_strip.h"

#include <cmath>
#include <string>

using namespace rules;

// Breaks caught (0.57.11): a paragraph's gap lost, a line longer than the box,
// a word split where a space was free, a long word left whole, text lost or
// reordered, an empty line left at the end.
TEST(HudTipLinesKeepParagraphsAndWrap) {
  const std::vector<std::string> lines = HudTipLines("ab cd ef gh ij\n\nKL\nmnopqrstuvwxyz\n", 75.0f);  // 10 a line
  CHECK_EQ(lines.size(), 6u);
  if (lines.size() == 6u) {
    CHECK_EQ(lines[0] == "ab cd ef", true);
    CHECK_EQ(lines[1] == "gh ij", true);
    CHECK_EQ(lines[2].empty(), true);  // the paragraphs' gap
    CHECK_EQ(lines[3] == "KL", true);
    CHECK_EQ(lines[4] == "mnopqrstuv", true);  // a word longer than a line, cut
    CHECK_EQ(lines[5] == "wxyz", true);
  }
  // The Monk's tooltip at the box's width: four paragraphs, three gaps, no line too long.
  const std::vector<std::string> focus = HudTipLines(FocusTooltip(18, 18, 17, true), kHudTipWidth);
  int gaps = 0;
  size_t longest = 0;
  for (const std::string& line : focus) {
    if (line.empty()) ++gaps;
    if (line.size() > longest) longest = line.size();
  }
  CHECK_EQ(gaps, 3);
  CHECK_EQ(longest <= static_cast<size_t>(kHudTipWidth / kHudCharWidth), true);
  CHECK_EQ(!focus.empty() && focus[0] == "Focus: 18 of 18 points", true);
  CHECK_EQ(HudTipLines("", kHudTipWidth).empty(), true);
}

// Breaks caught (0.57.11): the box not right-aligned with the panel, not above
// it, not below it at the screen's top, or off the left edge.
TEST(HudTipPlacedByThePanel) {
  HudBox panel;
  panel.x = 1000.0f;
  panel.y = 800.0f;
  panel.w = 200.0f;
  panel.h = 60.0f;
  HudBox box = PlaceHudTip(panel, 300.0f, 200.0f, 4.0f);
  CHECK_EQ(static_cast<int>(box.x), 900);
  CHECK_EQ(static_cast<int>(box.y), 596);  // 800 - 4 - 200: above
  panel.y = 100.0f;
  box = PlaceHudTip(panel, 300.0f, 200.0f, 4.0f);
  CHECK_EQ(static_cast<int>(box.y), 164);  // no room above: 100 + 60 + 4, below
  panel.x = 50.0f;
  box = PlaceHudTip(panel, 300.0f, 200.0f, 4.0f);
  CHECK_EQ(static_cast<int>(box.x), 0);  // never off the left edge
}

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

// Where the card goes (0.57.2, 0.57.3). Breaks caught: its right edge off the
// bar's (the scale left out), a gap over the Talk line or a card into it, a
// card that reaches the chat window left there, the move under the strip not
// centred, a gap under the strip's tiles or a card into them, the strip off
// and the card not on the top edge, a wide window moved anyway, off the screen.
TEST(HudPlacement) {
  HudScreen wide;
  wide.width = 1638.0f;
  wide.height = 904.0f;
  wide.chat_right = 720.0f;
  const HudSpot a = PlaceHud(wide, 351.0f, 51.0f);
  CHECK_EQ(a.under_strip, false);
  CHECK_EQ(a.x + 351.0f == 1638.0f / 2.0f + 400.0f, true);  // the bar's right edge
  CHECK_EQ(a.y + 51.0f == 904.0f - kQuickbarHeight - kTalkLineHeight, true);  // right on the Talk line
  HudScreen scaled = wide;
  scaled.width = 2560.0f;
  scaled.height = 1440.0f;
  scaled.scale = 1.5f;
  scaled.chat_right = 720.0f * 1.5f;
  const HudSpot b = PlaceHud(scaled, 351.0f, 51.0f);
  CHECK_EQ(b.under_strip, false);
  CHECK_EQ(b.x + 351.0f * 1.5f == 1280.0f + 600.0f, true);
  CHECK_EQ(b.y + 51.0f * 1.5f == 1440.0f - (kQuickbarHeight + kTalkLineHeight) * 1.5f, true);
  HudScreen narrow = wide;
  narrow.width = 1078.0f;  // the owner's window once: the bar's right edge 939, the card would start at 588
  const HudSpot c = PlaceHud(narrow, 351.0f, 51.0f);
  CHECK_EQ(c.under_strip, true);
  CHECK_EQ(c.x + 351.0f / 2.0f == 1078.0f / 2.0f, true);  // centred
  CHECK_EQ(c.y == kQueueTileTop + kQueueTileHeight, true);  // right under the strip's tiles
  HudScreen narrow_off = narrow;
  narrow_off.strip_on = false;  // !queue: the strip turned off
  const HudSpot e = PlaceHud(narrow_off, 351.0f, 51.0f);
  CHECK_EQ(e.under_strip && e.y == 0.0f && e.x == c.x, true);  // on the window's top edge
  HudScreen narrow_scaled = scaled;
  narrow_scaled.width = 1600.0f;  // at 1.5: the bar's right edge 1400, the card would start at 873 < 1092
  const HudSpot d = PlaceHud(narrow_scaled, 351.0f, 51.0f);
  CHECK_EQ(d.under_strip, true);
  CHECK_EQ(d.x + 351.0f * 1.5f / 2.0f == 800.0f, true);  // centred at the scale
  CHECK_EQ(d.y == kQueueTileTop + kQueueTileHeight * 1.5f, true);
  HudScreen edge = wide;
  edge.width = 2.0f * (720.0f + 8.0f + 351.0f - 400.0f);  // the card just clear of the chat: stays
  CHECK_EQ(PlaceHud(edge, 351.0f, 51.0f).under_strip, false);
  edge.width -= 2.0f;
  CHECK_EQ(PlaceHud(edge, 351.0f, 51.0f).under_strip, true);
  HudScreen tiny = wide;
  tiny.width = 300.0f;
  CHECK_EQ(PlaceHud(tiny, 351.0f, 51.0f).x >= 0.0f, true);
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
