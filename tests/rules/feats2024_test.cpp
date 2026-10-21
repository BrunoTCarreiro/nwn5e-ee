#include "check.h"
#include "rules/feats2024.h"

using namespace rules;

// Breaks caught: an NWN feat (Power Attack, Dodge, Weapon Focus) still
// offered in faithful, a background feat offered on the Feats page (the
// Background page gives it), a built 2024 feat (Alert, an ASI row, a style)
// left out, the ranges' ends off by one.
TEST(ChoosableFeatsAre2024) {
  CHECK_EQ(IsChoosableFeat2024(377), true);   // Alert (Improved Initiative)
  CHECK_EQ(IsChoosableFeat2024(1200), true);  // Ability Score Improvement 1
  CHECK_EQ(IsChoosableFeat2024(1206), true);  // ... 7
  CHECK_EQ(IsChoosableFeat2024(1207), false);
  CHECK_EQ(IsChoosableFeat2024(1210), true);  // Archery
  CHECK_EQ(IsChoosableFeat2024(1213), true);  // Two-Weapon Fighting
  CHECK_EQ(IsChoosableFeat2024(1214), false);
  CHECK_EQ(IsChoosableFeat2024(1220), false);  // Second Wind (granted)
  CHECK_EQ(IsChoosableFeat2024(1230), false);  // a background feat
  CHECK_EQ(IsChoosableFeat2024(28), false);    // Power Attack
  CHECK_EQ(IsChoosableFeat2024(10), false);    // Dodge
  CHECK_EQ(IsChoosableFeat2024(0), false);
}
