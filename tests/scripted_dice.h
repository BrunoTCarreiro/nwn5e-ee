#pragma once

// Test double for rules::Dice: hands out scripted results in order and records
// the die size of every request, so tests can assert both what was kept and
// which dice were rolled (e.g. a critical rolls each damage die twice).

#include <initializer_list>
#include <vector>

#include "check.h"
#include "rules/dice.h"

class ScriptedDice : public rules::Dice {
 public:
  ScriptedDice(std::initializer_list<int> results) : results_(results) {}

  int Roll(int sides) override {
    requested_.push_back(sides);
    if (next_ >= results_.size()) {
      check::FailText(__FILE__, __LINE__, "ScriptedDice: more rolls requested than scripted");
      return 1;
    }
    const int value = results_[next_++];
    if (value < 1 || value > sides) {
      check::FailText(__FILE__, __LINE__, "ScriptedDice: scripted value does not fit the requested die");
    }
    return value;
  }

  const std::vector<int>& requested() const { return requested_; }
  size_t unused() const { return results_.size() - next_; }

 private:
  std::vector<int> results_;
  std::vector<int> requested_;
  size_t next_ = 0;
};

// Fails unless the dice requested exactly these die sizes, in order.
inline void CheckRequestedSides(const ScriptedDice& dice, std::initializer_list<int> expected, const char* file,
                                int line) {
  const std::vector<int> want(expected);
  if (dice.requested() == want) return;
  char message[512];
  int used = snprintf(message, sizeof(message), "requested dice {");
  for (int sides : dice.requested()) used += snprintf(message + used, sizeof(message) - used, " d%d", sides);
  used += snprintf(message + used, sizeof(message) - used, " }, want {");
  for (int sides : want) used += snprintf(message + used, sizeof(message) - used, " d%d", sides);
  snprintf(message + used, sizeof(message) - used, " }");
  check::FailText(file, line, message);
}

#define CHECK_ROLLED(dice, ...) CheckRequestedSides((dice), __VA_ARGS__, __FILE__, __LINE__)
