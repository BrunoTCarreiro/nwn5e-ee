#include "check.h"
#include "rules/hit_dice.h"

using namespace rules;

TEST(HitDiceAreTheClassesDice) {
  // A Fighter 3 / Rogue 2 / Wizard 1: three d10s, two d8s, one d6.
  const HitDice dice = HitDiceOf({{CharacterClass::kFighter, 3}, {CharacterClass::kRogue, 2}, {CharacterClass::kWizard, 1}});
  CHECK_EQ(dice.count[HitDieIndex(10)], 3);
  CHECK_EQ(dice.count[HitDieIndex(8)], 2);
  CHECK_EQ(dice.count[HitDieIndex(6)], 1);
  CHECK_EQ(dice.count[HitDieIndex(12)], 0);
  CHECK_EQ(dice.Total(), 6);
  CHECK_EQ(UnpackHitDice(PackHitDice(dice)) == dice, true);
  HitDice spent;
  spent.count[HitDieIndex(10)] = 5;  // more than there are: none left, not negative
  CHECK_EQ(Remaining(dice, spent).count[HitDieIndex(10)], 0);
  CHECK_EQ(Remaining(dice, spent).Total(), 3);
}

TEST(ShortRestSpendsTheBiggestDiceToTheMaximum) {
  HitDice dice;
  dice.count[HitDieIndex(10)] = 2;
  dice.count[HitDieIndex(6)] = 2;
  auto fixed = [](int sides) { return sides / 2; };  // d10 -> 5, d6 -> 3
  // 20 of 40 HP, Con +2: d10 (5+2), d10 (5+2), d6 (3+2), d6 (3+2) = 24, capped at 20.
  ShortRest rest = ShortRestDice(dice, 20, 40, 2, fixed);
  CHECK_EQ(rest.sides.size(), 4u);
  CHECK_EQ(rest.sides[0], 10);
  CHECK_EQ(rest.sides[2], 6);
  CHECK_EQ(rest.healed, 20);
  // 33 of 40: one d10 (5 + 2) brings it to the maximum - no more dice.
  rest = ShortRestDice(dice, 33, 40, 2, fixed);
  CHECK_EQ(rest.sides.size(), 1u);
  CHECK_EQ(rest.healed, 7);
  // 30 of 40: the first d10 heals 7, the second only the 3 left to the maximum.
  rest = ShortRestDice(dice, 30, 40, 2, fixed);
  CHECK_EQ(rest.sides.size(), 2u);
  CHECK_EQ(rest.healed, 10);
  // At the maximum a Short Rest still spends one die (its cost) and heals nothing.
  rest = ShortRestDice(dice, 40, 40, 2, fixed);
  CHECK_EQ(rest.sides.size(), 1u);
  CHECK_EQ(rest.healed, 0);
  // A die heals at least 1, even with a Constitution penalty.
  HitDice one;
  one.count[HitDieIndex(6)] = 1;
  rest = ShortRestDice(one, 5, 10, -3, [](int) { return 1; });
  CHECK_EQ(rest.healed, 1);
  // No dice left: no rest.
  rest = ShortRestDice(HitDice(), 5, 10, 0, fixed);
  CHECK_EQ(rest.sides.size(), 0u);
}

TEST(LongRestWait) {
  // NWN's calendar: 28-day months, 12 of them; an hour is the count's unit.
  CHECK_EQ(GameHourOf(1372, 1, 2, 0) - GameHourOf(1372, 1, 1, 0), 24);
  CHECK_EQ(GameHourOf(1372, 1, 1, 5) - GameHourOf(1372, 1, 1, 0), 5);
  CHECK_EQ(GameHourOf(1372, 2, 1, 0) - GameHourOf(1372, 1, 28, 0), 24);   // a month's end
  CHECK_EQ(GameHourOf(1373, 1, 1, 0) - GameHourOf(1372, 12, 28, 0), 24);  // a year's end
  CHECK_EQ(GameHourOf(1372, 1, 1, 0) > 0, true);
  // SRD 5.2: 16 hours after a Long Rest ends before another starts.
  const int end = GameHourOf(1372, 3, 10, 8);
  CHECK_EQ(LongRestWaitLeft(true, false, end, end), 16);
  CHECK_EQ(LongRestWaitLeft(true, false, end, end + 10), 6);
  CHECK_EQ(LongRestWaitLeft(true, false, end, end + 15), 1);
  CHECK_EQ(LongRestWaitLeft(true, false, end, end + 16), 0);  // on the hour: it may
  CHECK_EQ(LongRestWaitLeft(true, false, end, end + 40), 0);
  CHECK_EQ(LongRestWaitLeft(true, false, 0, end), 0);         // no Long Rest yet
  CHECK_EQ(LongRestWaitLeft(true, false, 0, 5), 0);           // ... whatever the clock reads
  CHECK_EQ(LongRestWaitLeft(true, true, end, end), 0);        // the house rule: any time
  CHECK_EQ(LongRestWaitLeft(false, false, end, end), 0);      // NWN mode
  CHECK_EQ(LongRestWaitLeft(true, false, end, end - 5), 0);   // a clock set back
  // #135 b: at least 1 Hit Point to start a Long Rest - a creature that is down doesn't.
  CHECK_EQ(DownBlocksLongRest(true, true), true);
  CHECK_EQ(DownBlocksLongRest(true, false), false);
  CHECK_EQ(DownBlocksLongRest(false, true), false);  // NWN mode: the engine's own (no rest while dying)
}
