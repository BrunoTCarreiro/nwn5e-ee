#include "check.h"
#include "rules/monster.h"

using namespace rules;

// Breaks caught: a tier boundary off by one (e.g. AC 15 starting at CR 4 or 6),
// or the table not capping at 19 (HotU creatures have 3e AC 30-50).
TEST(ExpectedMonsterArmorClassTiers) {
  CHECK_EQ(ExpectedMonsterArmorClass(0), 13);
  CHECK_EQ(ExpectedMonsterArmorClass(3), 13);
  CHECK_EQ(ExpectedMonsterArmorClass(4), 14);
  CHECK_EQ(ExpectedMonsterArmorClass(5), 15);
  CHECK_EQ(ExpectedMonsterArmorClass(7), 15);
  CHECK_EQ(ExpectedMonsterArmorClass(8), 16);
  CHECK_EQ(ExpectedMonsterArmorClass(9), 16);
  CHECK_EQ(ExpectedMonsterArmorClass(10), 17);
  CHECK_EQ(ExpectedMonsterArmorClass(12), 17);
  CHECK_EQ(ExpectedMonsterArmorClass(13), 18);
  CHECK_EQ(ExpectedMonsterArmorClass(16), 18);
  CHECK_EQ(ExpectedMonsterArmorClass(17), 19);
  CHECK_EQ(ExpectedMonsterArmorClass(30), 19);
  CHECK_EQ(ExpectedMonsterArmorClass(60), 19);
}

// Breaks caught: NWN's fractional CRs rounding up into the next tier.
TEST(ExpectedMonsterArmorClassRoundsCrDown) {
  CHECK_EQ(ExpectedMonsterArmorClass(0.25), 13);
  CHECK_EQ(ExpectedMonsterArmorClass(3.9), 13);
  CHECK_EQ(ExpectedMonsterArmorClass(4.5), 14);
}
