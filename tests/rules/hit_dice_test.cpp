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
