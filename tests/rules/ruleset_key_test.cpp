#include "check.h"
#include "rules/ruleset_key.h"

using namespace rules;

// Keys seen in NWN: EE 89.8193.37-17's code (docs/EE-PORT.md). Breaks caught:
// another FNV variant (FNV-1, other basis or prime), upper/lower-casing the
// label, or hashing only part of it.
TEST(RulesetLabelKeysMatchEE) {
  // The seed ResolveAttackRoll's callers pass for labels starting with "N".
  CHECK_EQ(RulesetLabelKey("N") == 0xaf64034c86022ed1ull, true);
  // Read by the Linux client's character creation (constant in its code).
  CHECK_EQ(RulesetLabelKey("CHARGEN_BASE_ABILITY_MIN") == 0xb2a34e81e5213cb9ull, true);
  CHECK_EQ(RulesetLabelKey("COUP_DE_GRACE_LEVEL_LIMIT") == 0x1f61e608804daf56ull, true);
  CHECK_EQ(RulesetLabelKey("NUM_ATTACKS_OF_OPPORTUNITY") == 0xb4a27fd68b125111ull, true);
}
