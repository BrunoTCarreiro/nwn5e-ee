#include <math.h>

#include <vector>

#include "check.h"
#include "rules/reposition.h"

using namespace rules;

namespace {
bool Near(float a, float b) { return fabsf(a - b) < 0.01f; }
}  // namespace

// #166 decision 3 (owner, 2026-11-23): a refused attacker repositions. Breaks caught: the spots along the line
// instead of square to it, farther than 5 m, the far ones first, one side only, an attacker on its target given spots;
// a melee attacker sidestepping, a ranged one walking round, the first clear spot not taken, closing in with a clear
// spot, or standing when none is clear.
TEST(RepositionSpotsAndChoice) {
  // The attacker at (0, 0), its target east at (10, 0): left is north.
  const std::vector<Sidestep> spots = SidestepSpots(0.0f, 0.0f, 10.0f, 0.0f);
  CHECK_EQ(spots.size(), static_cast<size_t>(6));
  CHECK_EQ(Near(spots[0].x, 0.0f) && Near(spots[0].y, 1.5f), true);
  CHECK_EQ(Near(spots[1].x, 0.0f) && Near(spots[1].y, -1.5f), true);
  CHECK_EQ(Near(spots[2].y, 3.0f) && Near(spots[3].y, -3.0f), true);
  CHECK_EQ(Near(spots[4].y, 4.5f) && Near(spots[5].y, -4.5f), true);
  bool far = false;
  for (const Sidestep& s : spots) far = far || sqrtf(s.x * s.x + s.y * s.y) > kSidestepMetres + 0.01f;
  CHECK_EQ(far, false);
  // Seen at an angle: square to the line (its target north-east).
  const std::vector<Sidestep> angled = SidestepSpots(0.0f, 0.0f, 3.0f, 4.0f);
  CHECK_EQ(Near(angled[0].x, -0.8f * 1.5f) && Near(angled[0].y, 0.6f * 1.5f), true);
  CHECK_EQ(SidestepSpots(2.0f, 2.0f, 2.0f, 2.0f).empty(), true);
  int pick = 7;
  CHECK_EQ(ChooseReposition(false, {true, true}, &pick) == Reposition::kWalkRound && pick == -1, true);
  CHECK_EQ(ChooseReposition(true, {false, false, true, true}, &pick) == Reposition::kSidestep && pick == 2, true);
  CHECK_EQ(ChooseReposition(true, {true, false}, &pick) == Reposition::kSidestep && pick == 0, true);
  CHECK_EQ(ChooseReposition(true, {false, false, false}, &pick) == Reposition::kCloseIn && pick == -1, true);
  CHECK_EQ(ChooseReposition(true, {}, &pick) == Reposition::kCloseIn, true);
}
