#include "check.h"
#include "rules/hit_points.h"

using namespace rules;

// Breaks caught: NWN's d6 Bard / Rogue or d4 Sorcerer / Wizard kept, the
// fixed average off by one, later levels at the maximum, the first level not
// at the maximum, a negative Constitution taking a level below 1.
TEST(HitPointsAre2024) {
  CHECK_EQ(HitDie2024(CharacterClass::kBarbarian), 12);
  CHECK_EQ(HitDie2024(CharacterClass::kFighter), 10);
  CHECK_EQ(HitDie2024(CharacterClass::kBard), 8);
  CHECK_EQ(HitDie2024(CharacterClass::kRogue), 8);
  CHECK_EQ(HitDie2024(CharacterClass::kWarlock), 8);
  CHECK_EQ(HitDie2024(CharacterClass::kSorcerer), 6);
  CHECK_EQ(HitDie2024(CharacterClass::kWizard), 6);
  CHECK_EQ(MaxHitPoints2024({10}, 2), 12);                  // Fighter 1, Con +2
  CHECK_EQ(MaxHitPoints2024({10, 10, 10, 10, 10}, 2), 44);  // Fighter 5: 12 + 4 x (6 + 2)
  CHECK_EQ(MaxHitPoints2024({6, 6, 6}, 0), 14);             // Wizard 3: 6 + 4 + 4
  CHECK_EQ(MaxHitPoints2024({8, 8}, 1), 15);                // Rogue 2: 9 + 6
  CHECK_EQ(MaxHitPoints2024({10, 6}, 0), 14);               // Fighter 1 / Wizard 1: 10 + 4
  CHECK_EQ(MaxHitPoints2024({6, 10}, 0), 12);               // Wizard 1 / Fighter 1: 6 + 6
  CHECK_EQ(MaxHitPoints2024({6, 6, 6}, -5), 3);             // at least 1 per level
  CHECK_EQ(MaxHitPoints2024({}, 3), 0);
}
