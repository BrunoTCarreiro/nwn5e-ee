#include <string.h>

#include "check.h"
#include "rules/armor.h"

using namespace rules;

// SRD 5.2.1 p. 92 against NWN's tiers (the game's own descriptions: 4 scale
// mail, 5 chainmail, 6 banded / splint, 7 half-plate, 8 full plate). Breaks
// caught: the 3e line (AC 6+ heavy: chain mail medium, half plate heavy),
// NWN's AC kept (chain mail 15, half plate 17), a heavy armour adding Dex.
TEST(ArmorTiersAre2024) {
  CHECK_EQ(strcmp(ArmorForNwnTier(5).name, "Chain Mail"), 0);
  CHECK_EQ(IsHeavyArmor2024(5), true);   // Chain Mail
  CHECK_EQ(IsHeavyArmor2024(6), true);   // Splint
  CHECK_EQ(IsHeavyArmor2024(8), true);   // Plate
  CHECK_EQ(IsHeavyArmor2024(7), false);  // Half Plate: medium in 2024
  CHECK_EQ(IsHeavyArmor2024(4), false);  // Scale Mail
  CHECK_EQ(IsHeavyArmor2024(0), false);
  CHECK_EQ(ArmorForNwnTier(5).base_ac, 16);
  CHECK_EQ(ArmorForNwnTier(6).base_ac, 17);
  CHECK_EQ(ArmorForNwnTier(7).base_ac, 15);
  CHECK_EQ(ArmorForNwnTier(8).base_ac, 18);
  CHECK_EQ(ArmorForNwnTier(3).base_ac, 12);
  CHECK_EQ(ArmorForNwnTier(2).base_ac, 11);
  CHECK_EQ(ArmorForNwnTier(1).base_ac, 11);
  CHECK_EQ(ArmorForNwnTier(5).strength, 13);
  CHECK_EQ(ArmorForNwnTier(8).strength, 15);
  CHECK_EQ(ArmorForNwnTier(1).stealth_disadvantage, true);
  CHECK_EQ(ArmorForNwnTier(2).stealth_disadvantage, false);
  CHECK_EQ(strcmp(ArmorForNwnTier(9).name, "no armour"), 0);
  CHECK_EQ(strcmp(ArmorForNwnTier(-1).name, "no armour"), 0);
}

// What NWN's AC changes by: 2024 base - (10 + tier).
TEST(ArmorBaseAdjustment) {
  CHECK_EQ(ArmorBaseAdjustment2024(0), 0);
  CHECK_EQ(ArmorBaseAdjustment2024(1), 0);   // padded 11
  CHECK_EQ(ArmorBaseAdjustment2024(2), -1);  // leather 11 (NWN 12)
  CHECK_EQ(ArmorBaseAdjustment2024(3), -1);  // studded 12 (NWN 13)
  CHECK_EQ(ArmorBaseAdjustment2024(4), 0);   // scale 14
  CHECK_EQ(ArmorBaseAdjustment2024(5), 1);   // chain mail 16 (NWN 15)
  CHECK_EQ(ArmorBaseAdjustment2024(6), 1);   // splint 17 (NWN 16)
  CHECK_EQ(ArmorBaseAdjustment2024(7), -2);  // half plate 15 (NWN 17)
  CHECK_EQ(ArmorBaseAdjustment2024(8), 0);   // plate 18
}

// Dexterity into AC: light all, medium at most +2, heavy none - a penalty too.
TEST(ArmorDexterity) {
  CHECK_EQ(ArmorDexterity2024(0, 5), 5);
  CHECK_EQ(ArmorDexterity2024(2, 5), 5);
  CHECK_EQ(ArmorDexterity2024(4, 5), 2);
  CHECK_EQ(ArmorDexterity2024(7, 1), 1);
  CHECK_EQ(ArmorDexterity2024(7, -1), -1);
  CHECK_EQ(ArmorDexterity2024(5, 3), 0);
  CHECK_EQ(ArmorDexterity2024(8, -2), 0);
}
