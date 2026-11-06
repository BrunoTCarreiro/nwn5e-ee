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

// #135 (owner, 2026-11-06: "Also impose the 16 hour limit but also make it so the user can remove this limit as part
// of the house rules we talked about earlier"): SRD 5.2 - "After you finish a Long Rest, you must wait at least 16
// hours before starting another one." On the game clock, in whole hours.
constexpr int kLongRestWaitHours = 16;
// A calendar date's hour as a count from year 0 (NWN's calendar: 12 months of 28 days).
int GameHourOf(int year, int month, int day, int hour);
// The hours still to wait before a Long Rest may start; 0 when it may: no Long Rest finished yet (`last_end_hour`
// 0), NWN mode, the house rule `any_time` ([house_rules] long_rest_any_time), or a clock behind the last one's end.
int LongRestWaitLeft(bool faithful, bool any_time, int last_end_hour, int now_hour);
// #135 b (owner, 2026-11-06: "1 yes, 2 keep, 3 a+b, 4 B"): SRD 5.2 - "To start a Long Rest, you must have at least
// 1 Hit Point." A creature that is down (nwn5e's 0 HP: a player's dying state, a henchman lying down at 1 HP until
// healed) doesn't start one - healing gets it up first. Faithful only.
bool DownBlocksLongRest(bool faithful, bool down);

}  // namespace rules
