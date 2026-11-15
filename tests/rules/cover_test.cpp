#include "check.h"
#include "rules/cover.h"

using namespace rules;

// #166 part 1: 2024 Cover. Breaks caught: a degree's bonus wrong, the less protective degree kept, a creature
// behind the attacker or the target counted, one inside their own space counted, one to the side counted, a Large
// one's reach ignored, a Tiny one giving cover.
TEST(CoverDegrees) {
  CHECK_EQ(CoverArmorClassBonus(Cover::kNone), 0);
  CHECK_EQ(CoverArmorClassBonus(Cover::kHalf), 2);
  CHECK_EQ(CoverArmorClassBonus(Cover::kThreeQuarters), 5);
  CHECK_EQ(CoverArmorClassBonus(Cover::kTotal), 0);
  CHECK_EQ(MoreCover(Cover::kHalf, Cover::kThreeQuarters) == Cover::kThreeQuarters, true);
  CHECK_EQ(MoreCover(Cover::kThreeQuarters, Cover::kHalf) == Cover::kThreeQuarters, true);
  CHECK_EQ(MoreCover(Cover::kHalf, Cover::kNone) == Cover::kHalf, true);
}

TEST(CoverCreatureInTheWay) {
  // An attack from (0, 0) at a target at (10, 0); Medium bodies (0.4 m).
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 5, 0, 0.4f, 0.4f, 0.4f), true);       // on the line, halfway
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 5, 0.35f, 0.4f, 0.4f, 0.4f), true);   // its body reaches the line
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 5, -0.35f, 0.4f, 0.4f, 0.4f), true);  // either side
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 5, 0.5f, 0.4f, 0.4f, 0.4f), false);   // off to the side
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, -2, 0, 0.4f, 0.4f, 0.4f), false);     // behind the attacker
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 12, 0, 0.4f, 0.4f, 0.4f), false);     // behind the target
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 9.8f, 0, 0.4f, 0.4f, 0.4f), false);   // inside the target's own space
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 0.3f, 0, 0.4f, 0.4f, 0.4f), false);   // inside the attacker's own space
  CHECK_EQ(CreatureInTheWay(0, 0, 0, 0, 0, 0, 0.4f, 0.4f, 0.4f), false);       // no line
  CHECK_EQ(CreatureInTheWay(0, 0, 10, 0, 5, 0.7f, CreatureBodyRadius(4), 0.4f, 0.4f), true);  // a Large body reaches
  CHECK_EQ(CreatureInTheWay(0, 0, 0, 10, 0.3f, 5, 0.4f, 0.4f, 0.4f), true);    // any direction
  CHECK_EQ(CreatureInTheWay(3, 4, -3, -4, 0, 0, 0.4f, 0.4f, 0.4f), true);      // a diagonal, through the middle
}

TEST(CoverSizes) {
  CHECK_EQ(SizeGivesCover(1), false);  // Tiny
  CHECK_EQ(SizeGivesCover(2), true);   // Small
  CHECK_EQ(SizeGivesCover(5), true);   // Huge
  CHECK_EQ(CreatureBodyRadius(3) == 0.4f, true);
  CHECK_EQ(CreatureBodyRadius(1) < CreatureBodyRadius(3), true);
  CHECK_EQ(CreatureBodyRadius(3) < CreatureBodyRadius(4), true);
  CHECK_EQ(CreatureBodyRadius(4) < CreatureBodyRadius(5), true);
}
