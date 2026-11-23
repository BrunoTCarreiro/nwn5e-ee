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
  const char* names[kLosBodyPoints] = {"feet", "waist", "chest", "head", "left_shoulder", "right_shoulder"};
  const float heights[kLosBodyPoints] = {0.2f, 1.0f, 1.3f, 1.7f, 1.4f, 1.4f};
  int wrong = 0;
  for (int i = 0; i < kLosBodyPoints; ++i) {
    wrong += std::string(kLosBody[i].key) != names[i] || !Near(kLosBody[i].height, heights[i]);
  }
  CHECK_EQ(wrong, 0);
  float x = 0.0f, y = 0.0f, z = 0.0f;
  // The observer to the west, the target at (10, 5, 2): its left shoulder 0.3 m north (the observer's left).
  LosPointAt(kLosBody[4], 0.0f, 5.0f, 10.0f, 5.0f, 2.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 10.0f) && Near(y, 5.3f) && Near(z, 3.4f), true);
  LosPointAt(kLosBody[5], 0.0f, 5.0f, 10.0f, 5.0f, 2.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 10.0f) && Near(y, 4.7f) && Near(z, 3.4f), true);
  // The observer to the north: left is east.
  LosPointAt(kLosBody[4], 10.0f, 15.0f, 10.0f, 5.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 10.3f) && Near(y, 5.0f), true);
  // Seen at an angle (from the south-west): still 0.3 m from the target, square to the line.
  LosPointAt(kLosBody[4], 0.0f, 0.0f, 3.0f, 4.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 3.0f - 0.8f * 0.3f) && Near(y, 4.0f + 0.6f * 0.3f), true);
  LosPointAt(kLosBody[3], 0.0f, 0.0f, 3.0f, 4.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(x, 3.0f) && Near(y, 4.0f) && Near(z, 1.7f), true);
  // On top of it: no line - the points still beside the target.
  LosPointAt(kLosBody[4], 3.0f, 4.0f, 3.0f, 4.0f, 0.0f, &x, &y, &z);
  CHECK_EQ(Near(fabsf(x - 3.0f) + fabsf(y - 4.0f), 0.3f), true);
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
