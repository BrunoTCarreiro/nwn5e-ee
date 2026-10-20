#pragma once

// Phase 3 CM4 / CM5 / CM7 / CM12 (PLAN.md): a character's 2024 ability
// scores, as changes to the base scores NWN stores. SRD 5.2.

#include <optional>
#include <vector>

#include "rules/combat.h"
#include "rules/saves.h"

namespace rules {

constexpr int kAbilityIncreaseCap = 20;  // backgrounds and ASIs can't raise a score past 20

// One Ability Score Improvement: +2 to `first`, or +1 to `first` and +1 to `second`.
struct AbilityIncrease {
  Ability first;
  std::optional<Ability> second;
};

// The change to apply to each NWN-stored base score (indexed by Ability):
// minus NWN's per-level +1s (`level_bumps`), plus the background's increases
// (`background`), then each ASI in order — none of the increases raising a
// score above 20 (what doesn't fit is lost, as in 2024).
void FaithfulAbilityDelta(const int nwn_base[6], const int level_bumps[6], const int background[6],
                          const std::vector<AbilityIncrease>& asis, int delta[6]);

// The abilities a henchman raises with its own ASIs (CM12): the class's main
// abilities, then Constitution; the Fighter takes Strength or Dexterity,
// whichever is higher.
std::vector<Ability> AsiPriority2024(CharacterClass character_class, int strength, int dexterity);

// `count` ASIs of +2, each to the first ability in `priority` still below 20
// (given the scores so far).
std::vector<AbilityIncrease> AutomaticAsis(int count, const std::vector<Ability>& priority, const int scores[6]);

// "Strength" ... and "str" ... ("" for an out-of-range index).
const char* AbilityName(Ability ability);
const char* AbilityShortName(Ability ability);
// "str", "Strength", "dex" ... (any case) -> the ability.
std::optional<Ability> ParseAbility(const char* text);

}  // namespace rules
