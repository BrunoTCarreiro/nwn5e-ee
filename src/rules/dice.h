#pragma once

namespace rules {

// Source of die rolls. The game adapter supplies the engine's random numbers;
// tests supply scripted rolls.
class Dice {
 public:
  virtual ~Dice() = default;
  // Returns a value in [1, sides].
  virtual int Roll(int sides) = 0;
};

}  // namespace rules
