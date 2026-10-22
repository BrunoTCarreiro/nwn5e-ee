#pragma once

// Species (CM6) and the creation screen's 2024 words (CHARGEN.md C1): in
// faithful mode some of the game's own strings are shown as nwn5e's
// (data/strings.txt) — Race reads Species, the Half-Orc is the SRD's Orc, each
// species' description lists its 2024 traits and what nwn5e does with them,
// the hub's Abilities step reads Ability Scores, its Packages step Builds (the
// 2024 starting kits and Recommended's picks). dialog.tlk is not changed.

#include <stdint.h>

#include <optional>

namespace rules {

// The rule a swap belongs to (each follows its own config key).
enum class StringRule { kSpecies, kAbilityScores, kStartingEquipment };

struct StringSwap {
  uint32_t nwn5e;  // the nwn5e string shown instead (16000000+)
  StringRule rule;
};

// The 2024 string for one of the game's (dialog.tlk strref), if it has one.
std::optional<StringSwap> CreationStringSwap2024(uint32_t game_strref);

}  // namespace rules
