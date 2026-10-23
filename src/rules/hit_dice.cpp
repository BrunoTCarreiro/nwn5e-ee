#include "rules/hit_dice.h"

#include <algorithm>

#include "rules/hit_points.h"

namespace rules {

int HitDice::Total() const {
  int total = 0;
  for (int i = 0; i < kHitDieSizes; ++i) total += count[i];
  return total;
}

bool HitDice::operator==(const HitDice& other) const {
  for (int i = 0; i < kHitDieSizes; ++i) {
    if (count[i] != other.count[i]) return false;
  }
  return true;
}

int HitDieIndex(int sides) {
  switch (sides) {
    case 6: return 0;
    case 8: return 1;
    case 10: return 2;
    case 12: return 3;
    default: return -1;
  }
}

int HitDieSides(int index) { return 6 + 2 * index; }

HitDice HitDiceOf(const std::vector<ClassLevels>& classes) {
  HitDice dice;
  for (const ClassLevels& levels : classes) {
    const int index = HitDieIndex(HitDie2024(levels.character_class));
    if (index >= 0 && levels.levels > 0) dice.count[index] += levels.levels;
  }
  return dice;
}

HitDice Remaining(const HitDice& all, const HitDice& spent) {
  HitDice left;
  for (int i = 0; i < kHitDieSizes; ++i) left.count[i] = std::max(0, all.count[i] - spent.count[i]);
  return left;
}

int PackHitDice(const HitDice& dice) {
  int packed = 0;
  for (int i = 0; i < kHitDieSizes; ++i) packed |= (std::min(dice.count[i], 63) & 63) << (6 * i);
  return packed;
}

HitDice UnpackHitDice(int packed) {
  HitDice dice;
  for (int i = 0; i < kHitDieSizes; ++i) dice.count[i] = (packed >> (6 * i)) & 63;
  return dice;
}

ShortRest ShortRestDice(const HitDice& remaining, int hit_points, int max_hit_points, int con_modifier,
                        const std::function<int(int sides)>& roll) {
  ShortRest rest;
  HitDice left = remaining;
  int hp = hit_points;
  while (left.Total() > 0 && (rest.sides.empty() || hp < max_hit_points)) {
    int index = kHitDieSizes - 1;
    while (index > 0 && left.count[index] == 0) --index;  // the biggest left
    const int sides = HitDieSides(index);
    const int rolled = roll(sides);
    const int heal = std::max(1, rolled + con_modifier);
    --left.count[index];
    ++rest.spent.count[index];
    rest.sides.push_back(sides);
    rest.rolls.push_back(rolled);
    const int gained = std::max(0, std::min(heal, max_hit_points - hp));
    rest.healed += gained;
    hp += gained;
  }
  return rest;
}

}  // namespace rules
