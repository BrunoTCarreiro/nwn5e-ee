#include "check.h"
#include "rules/species.h"

using namespace rules;

// Breaks caught: a species description left on NWN's text (ability
// adjustments the 2024 rules don't have), the Half-Orc not named Orc, a swap
// under the wrong rule key, an id outside nwn5e's string range, a strref that
// isn't swapped (a name other than the Half-Orc's) swapped by mistake.
TEST(CreationStringsAre2024) {
  for (uint32_t description = 251; description <= 257; ++description) {
    const auto swap = CreationStringSwap2024(description);
    CHECK_EQ(swap.has_value(), true);
    CHECK_EQ(swap->rule == StringRule::kSpecies, true);
    CHECK_EQ(swap->nwn5e >= 16000000u && swap->nwn5e < 16500000u, true);
  }
  CHECK_EQ(CreationStringSwap2024(32)->nwn5e, 16000264u);  // Half-Orc -> Orc
  CHECK_EQ(CreationStringSwap2024(148)->rule == StringRule::kSpecies, true);
  CHECK_EQ(CreationStringSwap2024(143)->rule == StringRule::kAbilityScores, true);
  CHECK_EQ(CreationStringSwap2024(22).has_value(), false);  // "Dwarf" stays
  CHECK_EQ(CreationStringSwap2024(30).has_value(), false);  // "Half-Elf" stays (legacy)
  CHECK_EQ(CreationStringSwap2024(0).has_value(), false);
}
