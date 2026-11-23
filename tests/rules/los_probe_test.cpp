#include <math.h>

#include <string>
#include <vector>

#include "check.h"
#include "rules/los_probe.h"

using namespace rules;

namespace {
bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
}  // namespace

// #166 part 2's research step. Breaks caught: a point at the wrong height, the shoulders not beside the line (or on
// the same side), sideways taken from the world's axes instead of the line, an observer on top of the target.
TEST(LosProbeBodyPoints) {
  const char* names[kLosBodyPoints] = {"feet", "knees", "waist", "chest", "head", "left_shoulder", "right_shoulder"};
  const float heights[kLosBodyPoints] = {0.15f, 0.5f, 0.95f, 1.3f, 1.65f, 1.45f, 1.45f};
  int wrong = 0;
  for (int i = 0; i < kLosBodyPoints; ++i) {
    wrong += std::string(kLosBody[i].key) != names[i] || !Near(kLosBody[i].height, heights[i]);
  }
  CHECK_EQ(wrong, 0);
  float x = 0.0f, y = 0.0f, z = 0.0f;
  // The observer to the west, the target at (10, 5, 2): its left shoulder 0.25 m north (the observer's left).
  LosPointAt(kLosBody[5], 0.0f, 5.0f, 10.0f, 5.0f, 2.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 10.0f) && Near(y, 5.25f) && Near(z, 3.45f), true);
  LosPointAt(kLosBody[6], 0.0f, 5.0f, 10.0f, 5.0f, 2.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 10.0f) && Near(y, 4.75f) && Near(z, 3.45f), true);
  // The observer to the north: left is east.
  LosPointAt(kLosBody[5], 10.0f, 15.0f, 10.0f, 5.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 10.25f) && Near(y, 5.0f), true);
  // Seen at an angle (from the south-west): still 0.25 m from the target, square to the line.
  LosPointAt(kLosBody[5], 0.0f, 0.0f, 3.0f, 4.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 3.0f - 0.8f * 0.25f) && Near(y, 4.0f + 0.6f * 0.25f), true);
  LosPointAt(kLosBody[4], 0.0f, 0.0f, 3.0f, 4.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 3.0f) && Near(y, 4.0f) && Near(z, 1.65f), true);
  // On top of it: no line - the points still beside the target.
  LosPointAt(kLosBody[5], 3.0f, 4.0f, 3.0f, 4.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(fabsf(x - 3.0f) + fabsf(y - 4.0f), 0.25f), true);
}

// #166 (owner, 2026-11-23): the body by size and crouch. Breaks caught: a size's height wrong or an unknown one not
// Medium, the crouch not lowering the points or narrowing the shoulders, a Large body's points not scaled.
TEST(LosProbeBodyBySizeAndCrouch) {
  CHECK_EQ(Near(StandingHeight(1), 0.6f) && Near(StandingHeight(2), 1.0f) && Near(StandingHeight(3), 1.8f), true);
  CHECK_EQ(Near(StandingHeight(4), 3.0f) && Near(StandingHeight(5), 5.0f) && Near(StandingHeight(0), 1.8f), true);
  CHECK_EQ(Near(StandingHeight(7), 5.0f), true);
  CHECK_EQ(Near(CrouchedHeight(3), 1.1f), true);
  const LosBodyPoint medium = ScaledBodyPoint(5, 3, false);
  CHECK_EQ(Near(medium.height, 1.45f) && Near(medium.side, 0.25f), true);
  const LosBodyPoint crouched = ScaledBodyPoint(5, 3, true);
  CHECK_EQ(Near(crouched.height, 1.45f * 1.1f / 1.8f) && Near(crouched.side, 0.25f), true);
  const LosBodyPoint large = ScaledBodyPoint(4, 4, false);  // a Large head: 1.65 x 3.0 / 1.8
  CHECK_EQ(Near(large.height, 2.75f), true);
  CHECK_EQ(Near(ScaledBodyPoint(6, 4, false).side, -0.25f * 3.0f / 1.8f), true);
  CHECK_EQ(std::string(ScaledBodyPoint(1, 2, true).key) == "knees", true);
}

// Breaks caught: points with one result not grouped, groups out of the body's order, a blocker's name lost or another
// blocker's group merged in, the words' placeholders not filled.
TEST(LosProbeSummary) {
  const std::string clear = "{points} clear", blocked = "{points} blocked by {blocker}";
  std::vector<LosPointResult> points = {
      {"feet", false, "plc_table"}, {"waist", false, "plc_table"}, {"chest", true, ""},
      {"head", true, ""},           {"left shoulder", true, ""},   {"right shoulder", false, "the tileset"}};
  CHECK_EQ(LosSummary(points, clear, blocked) ==
               "feet, waist blocked by plc_table; chest, head, left shoulder clear; right shoulder blocked by the tileset",
           true);
  points = {{"feet", true, ""}, {"waist", true, ""}};
  CHECK_EQ(LosSummary(points, clear, blocked) == "feet, waist clear", true);
  points = {{"feet", false, "a"}, {"waist", false, "b"}, {"chest", false, "a"}};
  CHECK_EQ(LosSummary(points, clear, blocked) == "feet, chest blocked by a; waist blocked by b", true);
  CHECK_EQ(LosSummary({}, clear, blocked).empty(), true);
  CHECK_EQ(LosSummary({{"head", true, ""}}, "{points}: clear ({points})", blocked) == "head: clear (head)", true);
}
