#pragma once

// Phase 5 (docs/PLAN.md "Phase 5 — rests, healing, death"): Hit Point Dice
// and the Short Rest, SRD 5.2. A character has one Hit Die per level, its
// class's die (rules::HitDie2024); a Short Rest spends them - each heals the
// die's roll + the Constitution modifier, at least 1 - and a Long Rest gives
// them all back. nwn5e's Short Rest (owner, 2026-10-23) is instant, out of
// combat, and spends at least one die: as many as bring the character to its
// maximum, the biggest first.

#include <functional>
#include <vector>

#include "rules/combat.h"

namespace rules {

// Hit Dice by size: d6, d8, d10, d12.
constexpr int kHitDieSizes = 4;
struct HitDice {
  int count[kHitDieSizes] = {};
  int Total() const;
  bool operator==(const HitDice& other) const;
};
int HitDieIndex(int sides);         // 6 -> 0 ... 12 -> 3, -1 for another size
int HitDieSides(int index);         // 0 -> 6 ... 3 -> 12
HitDice HitDiceOf(const std::vector<ClassLevels>& classes);  // one per class level
HitDice Remaining(const HitDice& all, const HitDice& spent);  // never below 0

// The spent dice in one int (6 bits per size) - a character record.
int PackHitDice(const HitDice& dice);
HitDice UnpackHitDice(int packed);

// A Short Rest's dice: at least one when any are left, then more while the
// character is below its maximum, the biggest first. `roll(sides)` rolls a
// die. Each die heals roll + con_modifier, at least 1; `healed` is what the
// character gains (up to its maximum).
struct ShortRest {
  HitDice spent;            // the dice this rest spends
  std::vector<int> sides;   // in the order spent
  std::vector<int> rolls;   // each die's roll
  int healed = 0;
};
ShortRest ShortRestDice(const HitDice& remaining, int hit_points, int max_hit_points, int con_modifier,
                        const std::function<int(int sides)>& roll);

}  // namespace rules
